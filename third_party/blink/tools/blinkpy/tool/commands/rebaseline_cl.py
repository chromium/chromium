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
from typing import Dict, List, Optional

from blinkpy.common.net.git_cl import GitCL, TryJobStatus
from blinkpy.common.net.rpc import Build, RPCError
from blinkpy.common.net.web_test_results import WebTestResults
from blinkpy.common.path_finder import PathFinder
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.tool.commands.command import check_file_option
from blinkpy.tool.commands.rebaseline import AbstractParallelRebaselineCommand
from blinkpy.tool.commands.rebaseline import TestBaselineSet

_log = logging.getLogger(__name__)


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
    test_name_file_option = optparse.make_option(
        '--test-name-file',
        action='callback',
        callback=check_file_option,
        type='string',
        help=('Read names of tests to update from this file, '
              'one test per line.'))
    patchset_option = optparse.make_option(
        '--patchset',
        default=None,
        type='int',
        help='Patchset number to fetch results from.')

    def __init__(self, tool, io_pool: Optional[Executor] = None):
        super(RebaselineCL, self).__init__(options=[
            self.only_changed_tests_option,
            self.no_trigger_jobs_option,
            optparse.make_option(
                '--fill-missing',
                dest='fill_missing',
                action='store_true',
                default=None,
                help='If some platforms have no try job results, use results '
                'from try job results of other platforms.'),
            optparse.make_option('--no-fill-missing',
                                 dest='fill_missing',
                                 action='store_false'),
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
        allowed_builders = {
            builder
            for builder in self._tool.builders.all_try_builder_names()
            if not self._tool.builders.uses_wptrunner(builder)
        }
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
        self._tool = tool
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
            self._tool.web,
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

        builders_with_infra_failures = {
            build.builder_name
            for build in GitCL.filter_infra_failed(build_statuses)
        }
        jobs_to_results = self._fetch_results(build_statuses)
        builders_with_results = {b.builder_name for b in jobs_to_results}
        builders_without_results = (set(self.selected_try_bots) -
                                    builders_with_results -
                                    builders_with_infra_failures)
        if builders_without_results:
            _log.warning('Some builders have no results:')
            for builder in sorted(builders_without_results):
                _log.warning('  %s', builder)

        builders_without_results.update(builders_with_infra_failures)
        if options.fill_missing is None and builders_without_results:
            should_continue = self._tool.user.confirm(
                'Would you like to continue?',
                default=self._tool.user.DEFAULT_NO)
            if not should_continue:
                _log.info('Aborting.')
                return 1
            options.fill_missing = self._tool.user.confirm(
                'Would you like to try to fill in missing results with '
                'available results?\n'
                'Note: This is generally not suggested unless the results '
                'are platform agnostic.',
                default=self._tool.user.DEFAULT_NO)
            if not options.fill_missing:
                _log.info('Please rebaseline again for builders '
                          'with incomplete results later.')

        if options.test_name_file:
            test_baseline_set = self._make_test_baseline_set_from_file(
                options.test_name_file, jobs_to_results)
        elif args:
            test_baseline_set = self._make_test_baseline_set_for_tests(
                args, jobs_to_results)
        else:
            test_baseline_set = self._make_test_baseline_set(
                jobs_to_results, options.only_changed_tests)

        if options.fill_missing:
            self.fill_in_missing_results(test_baseline_set)
        with self._io_pool or contextlib.nullcontext():
            return self.rebaseline(options, test_baseline_set)

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

    def _fetch_results(
        self,
        build_statuses: Dict[Build, TryJobStatus],
    ) -> Dict[Build, List[WebTestResults]]:
        """Fetches results for all of the given builds.

        There should be a one-to-one correspondence between Builds, supported
        platforms, and try bots. If not all of the builds can be fetched, then
        continuing with rebaselining may yield incorrect results, when the new
        baselines are deduped, an old baseline may be kept for the platform
        that's missing results.

        Returns:
            A dict mapping Builds to lists of WebTestResults for all completed
            jobs.
        """
        results_fetcher = self._tool.results_fetcher
        builds_to_results = collections.defaultdict(list)
        build_steps = []

        for build, status in build_statuses.items():
            if status == TryJobStatus('COMPLETED', 'SUCCESS'):
                _log.debug('No baselines to download for passing %r build %s.',
                           build.builder_name, build.build_number
                           or '(unknown)')
                # This empty entry indicates the builder is not missing.
                builds_to_results[build] = []
                continue
            if status != TryJobStatus('COMPLETED', 'FAILURE'):
                # Only completed failed builds will contain actual failed
                # web tests to download baselines for.
                continue

            step_names = self._tool.builders.step_names_for_builder(
                build.builder_name)
            build_steps.extend((build, step_name) for step_name in step_names)

        map_fn = self._io_pool.map if self._io_pool else map
        step_results = map_fn(
            lambda build_step: results_fetcher.gather_results(*build_step),
            build_steps)
        for (build, _), results in zip(build_steps, step_results):
            if len(results) > 0:
                builds_to_results[build].append(results)
        return builds_to_results

    def _make_test_baseline_set_from_file(self, filename, builds_to_results):
        tests = []
        try:
            with self._tool.filesystem.open_text_file_for_reading(
                    filename) as fh:
                _log.info('Reading list of tests to rebaseline '
                          'from %s', filename)
                for test in fh.readlines():
                    test = test.strip()
                    if not test or test.startswith('#'):
                        continue
                    tests.append(test)
        except IOError:
            _log.info('Could not read test names from %s', filename)
        return self._make_test_baseline_set_for_tests(tests, builds_to_results)

    def _make_test_baseline_set_for_tests(self, test_patterns,
                                          builds_to_results):
        """Determines the set of test baselines to fetch from a list of tests.

        Args:
            tests_patterns: A list of test patterns (e.g., directories).
            builds_to_results: A dict mapping Builds to lists of WebTestResults.

        Returns:
            A TestBaselineSet object.
        """
        test_baseline_set = TestBaselineSet(self._tool.builders)
        port, tests = self._tool.port_factory.get(), set()
        for test_pattern in sorted(test_patterns):
            resolved_tests = port.tests([test_pattern])
            if not resolved_tests:
                _log.warning(
                    '%r does not represent any tests and may be misspelled.',
                    test_pattern)
            tests.update(resolved_tests)

        for test, (build, builder_results) in itertools.product(
                tests, builds_to_results.items()):
            for step_results in builder_results:
                # Check for bad user-supplied test names early to create a
                # smaller test baseline set and send fewer bad requests.
                if step_results.result_for_test(test):
                    test_baseline_set.add(test, build,
                                          step_results.step_name())
        return test_baseline_set

    def _make_test_baseline_set(self, builds_to_results, only_changed_tests):
        """Determines the set of test baselines to fetch.

        The list of tests are not explicitly provided, so all failing tests or
        modified tests will be rebaselined (depending on only_changed_tests).

        Args:
            builds_to_results: A dict mapping Builds to lists of WebTestResults.
            only_changed_tests: Whether to only include baselines for tests that
               are changed in this CL. If False, all new baselines for failing
               tests will be downloaded, even for tests that were not modified.

        Returns:
            A TestBaselineSet object.
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

        test_baseline_set = TestBaselineSet(self._tool.builders)
        for build, builder_results in builds_to_results.items():
            for step_results in builder_results:
                tests_to_rebaseline = self._tests_to_rebaseline(
                    build, step_results)
                # Here we have a concrete list of tests so we don't need prefix lookup.
                for test in tests_to_rebaseline:
                    if only_changed_tests and test not in tests_in_cl:
                        continue
                    test_baseline_set.add(test, build,
                                          step_results.step_name())
        return test_baseline_set

    def _test_base_path(self):
        """Returns the relative path from the repo root to the web tests."""
        finder = PathFinder(self._tool.filesystem)
        return self._tool.filesystem.relpath(
            finder.web_tests_dir(), finder.path_from_chromium_base()) + '/'

    def _tests_to_rebaseline(self, build, web_test_results):
        """Fetches a list of tests that should be rebaselined for some build.

        Args:
            build: A Build instance.
            web_test_results: A WebTestResults instance.

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
        new_failures = self._fetch_tests_with_new_failures(build, test_suite)
        if new_failures is None:
            _log.warning('No retry summary available for ("%s", "%s").',
                         build.builder_name, test_suite)
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

    def fill_in_missing_results(self, test_baseline_set):
        """Adds entries, filling in results for missing jobs.

        For each test prefix, if there is an entry missing for some port,
        then an entry should be added for that port using a build that is
        available.

        For example, if there's no entry for the port "win-win10", but there
        is an entry for the "win-win11" port, then an entry might be added
        for "win-win10" using the results from "win-win11".
        """
        all_ports = {
            self._tool.builders.port_name_for_builder_name(b)
            for b in self.selected_try_bots
        }
        for test in test_baseline_set.all_tests():
            build_port_pairs = test_baseline_set.build_port_pairs(test)
            missing_ports = all_ports - {p for _, p in build_port_pairs}
            if not missing_ports:
                continue
            _log.info('For %s:', test)
            for port in sorted(missing_ports):
                build = self._choose_fill_in_build(port, build_port_pairs)
                _log.info('Using "%s" build %d for %s.', build.builder_name,
                          build.build_number, port)
                test_baseline_set.add(test, build, port_name=port)
        return test_baseline_set

    def _choose_fill_in_build(self, target_port, build_port_pairs):
        """Returns a Build to use to supply results for the given port.

        Ideally, this should return a build for a similar port so that the
        results from the selected build may also be correct for the target port.
        """

        # A full port name should normally always be of the form <os>-<version>;
        # for example "win-win11", or "linux-trusty". For the test port used in
        # unit tests, though, the full port name may be "test-<os>-<version>".
        def os_name(port):
            if '-' not in port:
                return port
            return port[:port.rfind('-')]

        # If any Build exists with the same OS, use the first one.
        target_os = os_name(target_port)
        same_os_builds = sorted(
            b for b, p in build_port_pairs if os_name(p) == target_os)
        if same_os_builds:
            return same_os_builds[0]

        # Otherwise, perhaps any build will do, for example if the results are
        # the same on all platforms. In this case, just return the first build.
        return sorted(build_port_pairs)[0][0]
