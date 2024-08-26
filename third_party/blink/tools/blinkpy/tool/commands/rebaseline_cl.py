# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A command to fetch new baselines from try jobs for the current CL."""

import collections
import contextlib
import itertools
import json
import logging
import optparse
import re
from concurrent.futures import Executor
from typing import Collection, List, MutableMapping, Optional

from blinkpy.common.memoized import memoized
from blinkpy.common.net.git_cl import BuildStatus, BuildStatuses, GitCL
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.common.net.web_test_results import (
    IncompleteResultsReason,
    WebTestResults,
)
from blinkpy.common.path_finder import PathFinder
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.tool.commands.command import resolve_test_patterns
from blinkpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from blinkpy.tool.commands.rebaseline import TestBaselineSet
from blinkpy.tool.grammar import pluralize

_log = logging.getLogger(__name__)


# Maps target builds to rebaseline to the test results to use. Note that
# multiple builds may map to the same `WebTestResults` if the user chooses to
# fill incomplete results with available ones. As a corollary, the target build
# may differ from its `WebTestResults.build`, which represents the source of
# the test results.
ResultsByBuild = MutableMapping[Build, WebTestResults]

ResultsBySuite = MutableMapping[str, ResultsByBuild]


class RebaselineCL(AbstractParallelRebaselineCommand):
    name = 'rebaseline-cl'
    help_text = 'Fetches new baselines for a CL from test runs on try bots.'
    long_help = (
        'This command downloads new baselines for failing web '
        'tests from archived try job test results. Cross-platform '
        'baselines are deduplicated after downloading.  Without '
        'positional parameters or --test-name-file, all failing tests '
        'are rebaselined. If positional parameters are provided, '
        'they are interpreted as test names to rebaseline.')

    show_in_main_help = True
    argument_names = '[testname,...]'

    only_changed_tests_option = optparse.make_option(
        '--only-changed-tests',
        action='store_true',
        default=False,
        help='Only update files for tests directly modified in the CL.')
    no_trigger_jobs_option = optparse.make_option(
        '--no-trigger-jobs',
        dest='trigger_jobs',
        action='store_false',
        default=True,
        help='Do not trigger any try jobs.')
    patchset_option = optparse.make_option(
        '--patchset',
        default=None,
        type='int',
        help='Patchset number to fetch try results from (defaults to latest).')

    def __init__(self, tool, io_pool: Optional[Executor] = None):
        super().__init__(options=[
            self.only_changed_tests_option,
            self.no_trigger_jobs_option,
            self.test_name_file_option,
            optparse.make_option(
                '--builders',
                default=set(),
                type='string',
                callback=self._check_builders,
                action='callback',
                help=('Comma-separated-list of builders to pull new baselines '
                      'from (can also be provided multiple times).')),
            self.patchset_option,
            self.no_optimize_option,
            self.dry_run_option,
            self.results_directory_option,
            *self.wpt_options,
        ])
        self._tool = tool
        # Use a separate thread pool for parallel network I/O in the main
        # process because `message_pool.get(...)` must know all tasks in
        # advance; it has no API for submitting new tasks after the pool runs.
        # Also, because communication is asynchronous (callback-based), a worker
        # cannot return a value for a specific task without a custom tracking
        # mechanism.
        self._io_pool = io_pool
        self.git_cl = None
        self._builders = []

    def _check_builders(self, option, _opt_str, value, parser):
        selected_builders = getattr(parser.values, option.dest, set())
        # This set includes CQ builders, whereas `builder_for_rebaselining()`
        # does not.
        allowed_builders = self._tool.builders.all_try_builder_names()
        for builder in value.split(','):
            if builder in allowed_builders:
                selected_builders.add(builder)
            else:
                lines = [
                    "'%s' is not a try builder." % builder,
                    '',
                    "The try builders that 'rebaseline-cl' recognizes are:",
                ]
                lines.extend('  * %s' % builder
                             for builder in sorted(allowed_builders))
                raise optparse.OptionValueError('\n'.join(lines))
        setattr(parser.values, option.dest, selected_builders)

    def execute(self, options, args, tool):
        self._dry_run = options.dry_run
        self.git_cl = self.git_cl or GitCL(tool)

        # '--dry-run' implies '--no-trigger-jobs'.
        options.trigger_jobs = options.trigger_jobs and not self._dry_run
        if args and options.test_name_file:
            _log.error('Aborted: Cannot combine --test-name-file and '
                       'positional parameters.')
            return 1

        if not self.check_ok_to_run():
            return 1

        self._builders = options.builders

        build_resolver = BuildResolver(
            self._tool,
            self.git_cl,
            self._io_pool,
            can_trigger_jobs=(options.trigger_jobs and not self._dry_run))
        builds = [Build(builder) for builder in self.selected_try_bots]
        try:
            build_statuses = build_resolver.resolve_builds(
                builds, options.patchset)
        except RPCError as error:
            _log.error('%s', error)
            _log.error('Request payload: %s',
                       json.dumps(error.request_body, indent=2))
            return 1
        except UnresolvedBuildException as error:
            _log.error('%s', error)
            return 1

        results_by_suite = self._fetch_results(build_statuses)
        incomplete_results = self._partition_incomplete_results(
            results_by_suite)
        if incomplete_results:
            self._log_incomplete_results(incomplete_results)
            if self._tool.user.confirm(
                    'Would you like to fill in missing results '
                    'with available results?\n'
                    'This is generally not suggested unless the results are '
                    'platform agnostic or the needed results happen to be not '
                    'missing.'):
                results_by_suite = self.fill_in_missing_results(
                    incomplete_results, results_by_suite)
            else:
                _log.info('Aborting. Please retry builders with no results.')
                return 1

        if options.test_name_file:
            test_baseline_set = self._make_test_baseline_set_from_file(
                options.test_name_file, results_by_suite)
        elif args:
            test_baseline_set = self._make_test_baseline_set_for_tests(
                args, results_by_suite)
        else:
            test_baseline_set = self._make_test_baseline_set(
                results_by_suite, options.only_changed_tests)

        with self._io_pool or contextlib.nullcontext():
            return self.rebaseline(options, test_baseline_set)

    def _partition_incomplete_results(
            self, results_by_suite: ResultsBySuite) -> ResultsBySuite:
        """Remove and return incomplete results from the given results map.

        Arguments:
            results_by_suite: Test results keyed by suite name, then by build.
                This map is mutated to leave only complete results when this
                method returns.
        """
        incomplete_results_by_suite = collections.defaultdict(dict)
        for suite, results_by_build in results_by_suite.items():
            for target_build, results in list(results_by_build.items()):
                if results.incomplete_reason:
                    del results_by_build[target_build]
                    incomplete_results_by_suite[suite][target_build] = results
        return incomplete_results_by_suite

    def _log_incomplete_results(self, results_by_suite: ResultsBySuite):
        _log.warning('Some builds have incomplete results:')
        for suite in sorted(results_by_suite):
            for build in sorted(results_by_suite[suite],
                                key=lambda build: build.builder_name):
                _log.warning(
                    f'  {build}, "{suite}": '
                    f'{results_by_suite[suite][build].incomplete_reason}')
        # TODO(crbug.com/352762538): Link to the document about handling bot
        # timeouts if it's one of the reasons.

    def check_ok_to_run(self):
        unstaged_baselines = self.unstaged_baselines()
        if unstaged_baselines:
            _log.error('Aborting: there are unstaged baselines:')
            for path in unstaged_baselines:
                _log.error('  %s', path)
            return False
        return True

    @property
    def selected_try_bots(self):
        if self._builders:
            return set(self._builders)
        return self._tool.builders.builders_for_rebaselining()

    def _fetch_results(self, build_statuses: BuildStatuses) -> ResultsBySuite:
        """Fetches results for all of the given builds.

        There should be a one-to-one correspondence between Builds, supported
        platforms, and try bots. If not all of the builds can be fetched, then
        continuing with rebaselining may yield incorrect results, when the new
        baselines are deduped, an old baseline may be kept for the platform
        that's missing results.
        """
        results_fetcher = self._tool.results_fetcher
        results_by_suite = collections.defaultdict(dict)
        build_steps_to_fetch = []

        for build, status in build_statuses.items():
            for step in self._tool.builders.step_names_for_builder(
                    build.builder_name):
                if status is BuildStatus.TEST_FAILURE:
                    # Only completed failed builds will contain actual failed
                    # web tests to download baselines for.
                    build_steps_to_fetch.append((build, step))
                    continue

                incomplete_reason = None
                if status is BuildStatus.SUCCESS:
                    _log.debug(
                        f'No baselines to download for passing {build}.')
                else:
                    incomplete_reason = IncompleteResultsReason(status)
                # These empty results are a no-op when constructing the
                # `TestBaselineSet` later.
                results_by_suite[step][build] = WebTestResults(
                    [],
                    step_name=step,
                    build=build,
                    incomplete_reason=incomplete_reason)

        _log.info('Fetching test results for '
                  f'{pluralize("suite", len(build_steps_to_fetch))}.')
        map_fn = self._io_pool.map if self._io_pool else map
        for results in map_fn(
                lambda build_step: results_fetcher.gather_results(*build_step),
                build_steps_to_fetch):
            results_by_suite[results.step_name()][results.build] = results
        return results_by_suite

    def _make_test_baseline_set_from_file(
        self,
        filename: str,
        results_by_suite: ResultsBySuite,
    ) -> TestBaselineSet:
        tests = set()
        try:
            _log.info('Reading list of tests to rebaseline from %s', filename)
            tests = self._host_port.tests_from_file(filename)
        except IOError:
            _log.info('Could not read test names from %s', filename)
        return self._make_test_baseline_set_for_tests(tests, results_by_suite)

    def _make_test_baseline_set_for_tests(
        self,
        test_patterns: Collection[str],
        results_by_suite: ResultsBySuite,
    ) -> TestBaselineSet:
        """Determines the set of test baselines to fetch from a list of tests.

        Args:
            tests_patterns: A list of test patterns (e.g., directories).
        """
        test_baseline_set = TestBaselineSet(self._tool.builders)
        tests = resolve_test_patterns(self._host_port, test_patterns)
        for test, (suite, results_by_build) in itertools.product(
                tests, results_by_suite.items()):
            for target_build, results in results_by_build.items():
                if not results.result_for_test(test):
                    # No need to rebaseline tests that ran expectedly. Expected
                    # results aren't fetched from ResultDB.
                    continue
                target_port = self._tool.builders.port_name_for_builder_name(
                    target_build.builder_name)
                test_baseline_set.add(test, results.build, suite, target_port)
        return test_baseline_set

    def _make_test_baseline_set(
        self,
        results_by_suite: ResultsBySuite,
        only_changed_tests: bool,
    ) -> TestBaselineSet:
        """Determines the set of test baselines to fetch.

        The list of tests are not explicitly provided, so all failing tests or
        modified tests will be rebaselined (depending on only_changed_tests).

        Args:
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.
        """
        if only_changed_tests:
            files_in_cl = self._tool.git().changed_files(diff_filter='AM')
            # In the changed files list from Git, paths always use "/" as
            # the path separator, and they're always relative to repo root.
            test_base = self._test_base_path()
            tests_in_cl = {
                f[len(test_base):]
                for f in files_in_cl if f.startswith(test_base)
            }

        builders = self._tool.builders
        test_baseline_set = TestBaselineSet(self._tool.builders)
        for suite, results_by_build in results_by_suite.items():
            for target_build, results in results_by_build.items():
                # Here we have a concrete list of tests so we don't need prefix lookup.
                for test in self._tests_to_rebaseline(results):
                    if only_changed_tests and test not in tests_in_cl:
                        continue
                    target_port = builders.port_name_for_builder_name(
                        target_build.builder_name)
                    test_baseline_set.add(test, results.build, suite,
                                          target_port)

        # Validate test existence, since the builder may run tests not found
        # locally. `Port.tests()` performs an expensive filesystem walk, so
        # filter out all invalid tests here instead of filtering at each build
        # step.
        tests = set(self._host_port.tests(test_baseline_set.all_tests()))
        missing_tests, valid_set = set(), TestBaselineSet(self._tool.builders)
        for task in test_baseline_set:
            if task.test in tests:
                valid_set.add(*task)
            else:
                missing_tests.add(task.test)
        if missing_tests:
            _log.warning(
                'Skipping rebaselining for %s missing from the local checkout:',
                pluralize('test', len(missing_tests)))
            for test in sorted(missing_tests):
                _log.warning(f'  {test}')
            _log.warning('You may want to rebase or trigger new builds.')
        return valid_set

    def _test_base_path(self):
        """Returns the relative path from the repo root to the web tests."""
        finder = PathFinder(self._tool.filesystem)
        return self._tool.filesystem.relpath(
            finder.web_tests_dir(), finder.path_from_chromium_base()) + '/'

    def _tests_to_rebaseline(self,
                             web_test_results: WebTestResults) -> List[str]:
        """Fetches a list of tests that should be rebaselined for some build.

        Returns:
            A sorted list of tests to rebaseline for this build.
        """
        unexpected_results = web_test_results.didnt_run_as_expected_results()
        tests = sorted(r.test_name() for r in unexpected_results
                       if r.is_missing_baseline() or r.has_mismatch())
        if not tests:
            # no need to fetch retry summary in this case
            return []

        test_suite = re.sub('\s*\(.*\)$', '', web_test_results.step_name())
        new_failures = self._fetch_tests_with_new_failures(
            web_test_results.build, test_suite)
        if new_failures is None:
            _log.warning('No retry summary available for (%s, "%s").',
                         web_test_results.build, test_suite)
        else:
            tests = [t for t in tests if t in new_failures]
        return tests

    def _fetch_tests_with_new_failures(self, build, test_suite):
        """For a given test suite in the try job, lists tests that only failed
        with the patch.

        If a test failed only with the patch but not without, then that
        indicates that the failure is actually related to the patch and
        is not failing at HEAD.

        If the list of new failures could not be obtained, this returns None.
        """
        results_fetcher = self._tool.results_fetcher
        content = results_fetcher.fetch_retry_summary_json(build, test_suite)
        if content is None:
            return None
        try:
            retry_summary = json.loads(content)
            return set(retry_summary['failures'])
        except (ValueError, KeyError):
            _log.warning('Unexpected retry summary content:\n%s', content)
            return None

    def fill_in_missing_results(
        self,
        incomplete_results: ResultsBySuite,
        complete_results: ResultsBySuite,
    ) -> ResultsBySuite:
        """Create a map of complete results to rebaseline with.

        Returns:
            The union of `incomplete_results` and `complete_results`, where the
            former's entries have been substituted with those selected from the
            latter. The selection prioritizes same-OS substitutions.
        """
        # Store complete and filled-in results separately to avoid mutating
        # `complete_results` while it's being used to fill in results.
        merged_results = {
            suite: dict(results_by_build)
            for suite, results_by_build in complete_results.items()
        }
        for suite in incomplete_results:
            for incomplete_build in list(incomplete_results[suite]):
                fill_results = self._select_fill_results(
                    incomplete_build, suite, complete_results)
                if fill_results is None:
                    continue
                merged_results[suite][incomplete_build] = fill_results
                _log.debug('Using %s, "%s" for %s, "%s".', fill_results.build,
                           fill_results.step_name(), incomplete_build, suite)
        return merged_results

    def _select_fill_results(
        self,
        incomplete_build: Optional[Build],
        suite: str,
        complete_results: ResultsBySuite,
    ) -> Optional[WebTestResults]:
        # TODO(crbug.com/350775866): Consider allowing substitutions across
        # related suites (e.g., ordinary `blink_wpt_tests` and
        # `not_site_per_process_blink_wpt_tests`).
        complete_results_by_build = complete_results.get(suite, {})
        # Some platforms only run a small subset of tests. Don't allow them to
        # provide results to other platforms that run all tests.
        candidate_builds = {
            build
            for build in complete_results_by_build
            if not self._default_smoke_test_only(build)
        }
        if not candidate_builds:
            return None
        target_os = self._os_name(incomplete_build)
        # If no builds have the same OS, choose an arbitrary one, which is OK
        # for platform-agnostic tests.
        source_build = max(candidate_builds,
                           key=lambda build: self._os_name(build) == target_os)
        return complete_results_by_build[source_build]

    @memoized
    def _default_smoke_test_only(self, build: Build) -> bool:
        port_name = self._tool.builders.port_name_for_builder_name(
            build.builder_name)
        port = self._tool.port_factory.get(port_name)
        return port.default_smoke_test_only()

    @memoized
    def _os_name(self, build: Build) -> str:
        # A full port name should normally always be of the form <os>-<version>;
        # for example "win-win11", or "mac-mac13-arm64". For the test port used
        # in unit tests, though, the full port name may be
        # "test-<os>-<version>".
        port_name = self._tool.builders.port_name_for_builder_name(
            build.builder_name)
        return self._tool.port_factory.get(port_name).operating_system()
