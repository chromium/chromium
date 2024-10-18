# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates expectations and baselines when updating web-platform-tests.

Specifically, this class fetches results from try bots for the current CL, then
(1) downloads new baseline files for any tests that can be rebaselined, and
(2) updates the generic TestExpectations file for any other failing tests.
"""

import argparse
import logging
import re
from collections import defaultdict
from typing import Collection, List, Optional, Set, Tuple

from blinkpy.common.memoized import memoized
from blinkpy.common.net.git_cl import BuildStatuses, GitCL
from blinkpy.common.net.rpc import Build
from blinkpy.common.net.web_test_results import (
    WebTestResult,
    WebTestResults,
)
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.tool.commands.build_resolver import (
    BuildResolver,
    UnresolvedBuildException,
)
from blinkpy.web_tests.port.base import Port
from blinkpy.web_tests.models.test_expectations import (
    ExpectationsChange,
    ParseError,
    SystemConfigurationEditor,
    TestExpectations,
    TestExpectationsCache,
)
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


class WPTExpectationsUpdater:
    MARKER_COMMENT = '# ====== New tests from wpt-importer added here ======'

    def __init__(self, host, args=None, wpt_manifests=None):
        self.host = host
        self.port = self.host.port_factory.get()
        self.finder = PathFinder(self.host.filesystem)
        self.git_cl = GitCL(host)
        self.git = self.host.git(self.finder.chromium_base())
        self.patchset = None
        self.wpt_manifests = (
            wpt_manifests or
            [self.port.wpt_manifest(d) for d in self.port.WPT_DIRS])

        # Get options from command line arguments.
        parser = argparse.ArgumentParser(description=__doc__)
        self.add_arguments(parser)
        self.options = parser.parse_args(args or [])
        if not (self.options.clean_up_test_expectations or
                self.options.clean_up_test_expectations_only):
            assert not self.options.clean_up_affected_tests_only, (
                'Cannot use --clean-up-affected-tests-only without using '
                '--clean-up-test-expectations or '
                '--clean-up-test-expectations-only')
        # Set up TestExpectations instance which contains all
        # expectations files associated with the platform.
        expectations_dict = {p: self.host.filesystem.read_text_file(p)
                             for p in self.expectations_files()}
        self._test_expectations = TestExpectations(
            self.port, expectations_dict=expectations_dict)

    def expectations_files(self):
        """Returns list of expectations files.

        Each expectation file in the list will be cleaned of expectations
        for tests that were removed and will also have test names renamed
        for tests that were renamed. Also the files may have their expectations
        updated using builder results.
        """
        return list(self.port.all_expectations_dict().keys())

    def run(self):
        """Does required setup before calling update_expectations().

        Do not override this function!
        """
        log_level = logging.DEBUG if self.options.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        self.patchset = self.options.patchset

        if (self.options.clean_up_test_expectations or
                self.options.clean_up_test_expectations_only):
            # Remove expectations for deleted tests and rename tests in
            # expectations for renamed tests.
            self.cleanup_test_expectations_files()

        if not self.options.clean_up_test_expectations_only:
            # Use try job results to update expectations and baselines
            self.update_expectations()

        return 0

    def add_arguments(self, parser):
        parser.add_argument(
            '--patchset',
            default=None,
            help='Patchset number to fetch new baselines from.')
        parser.add_argument(
            '-v',
            '--verbose',
            action='store_true',
            help='More verbose logging.')
        parser.add_argument(
            '--clean-up-test-expectations',
            action='store_true',
            help='Cleanup test expectations files.')
        parser.add_argument(
            '--clean-up-test-expectations-only',
            action='store_true',
            help='Clean up expectations and then exit script.')
        parser.add_argument(
            '--clean-up-affected-tests-only',
            action='store_true',
            help='Only cleanup expectations deleted or renamed in current CL. '
                 'If flag is not used then a full cleanup of deleted or '
                 'renamed tests will be done in expectations.')
        parser.add_argument(
            '--include-unexpected-pass',
            action='store_true',
            help='Adds Pass to tests with failure expectations. '
                 'This command line argument can be used to mark tests '
                 'as flaky.')

    def suites_for_builder(self, builder: str) -> Set[str]:
        # TODO(crbug.com/1502294): Make everything a suite name (i.e., without
        # the `(with patch)` suffix) to be consistent.
        suites = set()
        for step in self.host.builders.step_names_for_builder(builder):
            suite_match = re.match(r'(?P<suite>[\w_-]*wpt_tests)', step)
            if suite_match:
                suites.add(step)
        return suites

    def update_expectations(self):
        """Adds test expectations lines.

        Returns:
            A pair: A set of tests that should be rebaselined, and a dictionary
            mapping tests that couldn't be rebaselined to lists of expectation
            lines written to TestExpectations.
        """
        # The wpt_manifest function in Port is cached by default, but may be out
        # of date if this code is called during test import. An out of date
        # manifest will cause us to mistreat newly added tests, as they will not
        # exist in the cached manifest. To avoid this, we invalidate the cache
        # here. See https://crbug.com/1154650 .
        self.port.wpt_manifest.cache_clear()

        resolver = BuildResolver(self.host,
                                 self.git_cl,
                                 can_trigger_jobs=False)
        builds = [Build(builder) for builder in self._get_try_bots()]
        try:
            build_to_status = resolver.resolve_builds(builds, self.patchset)
            _log.debug('Latest try jobs: %r', build_to_status)
        except UnresolvedBuildException as error:
            raise ScriptError(
                'No try job information was collected.') from error

        tests_to_rebaseline, results = self._fetch_results_for_update(
            build_to_status)
        # TODO(crbug.com/1475013): Decide how to organize Android expectations.
        results_by_path = defaultdict(list)
        for suite_results in results:
            port = self._port_for_build_step(suite_results.builder_name,
                                             suite_results.step_name())
            flag_specific = port.flag_specific_config_name()
            if flag_specific:
                path = port.path_to_flag_specific_expectations_file(
                    flag_specific)
            else:
                path = port.path_to_generic_test_expectations_file()
            results_by_path[path].append(suite_results)

        exp_lines_dict = defaultdict(list)
        for path in sorted(results_by_path, key=self._update_order):
            for test, lines in self.write_to_test_expectations(
                    results_by_path[path], path).items():
                exp_lines_dict[test].extend(lines)
        return sorted(tests_to_rebaseline), exp_lines_dict

    def _update_order(self, path: str) -> int:
        # Update generic expectations first.
        if path == self.port.path_to_generic_test_expectations_file():
            return 0
        return 1

    def _fetch_results_for_update(
        self,
        build_to_status: BuildStatuses,
    ) -> Tuple[Set[str], List[WebTestResults]]:
        completed_results, missing_results, final_results = [], [], []
        tests_to_rebaseline = set()

        fetcher = self.host.results_fetcher
        incomplete_builds = GitCL.filter_incomplete(build_to_status)
        for build, job_status in build_to_status.items():
            for suite in self.suites_for_builder(build.builder_name):
                # `exclude_exonerations=(not include_unexpected_pass)` will
                # leave out unexpected passes as well as other kinds of
                # exonerations (e.g., experimental build). This is good enough
                # in practice.
                suite_results = self.host.results_fetcher.gather_results(
                    build, suite, not self.options.include_unexpected_pass)
                to_rebaseline, suite_results = self.filter_results_for_update(
                    suite_results)
                tests_to_rebaseline.update(to_rebaseline)
                if 'webdriver_wpt_tests' in suite:
                    if build in incomplete_builds:
                        raise ValueError(
                            f"{suite!r} on {build!r} doesn't run the main WPT "
                            'suite and therefore cannot inherit results from '
                            'other builds.')
                    final_results.append(suite_results)
                elif build in incomplete_builds:
                    missing_results.append(suite_results)
                else:
                    completed_results.append(suite_results)

        final_results.extend(
            self.fill_missing_results(results, completed_results)
            for results in missing_results)
        final_results.extend(completed_results)
        return tests_to_rebaseline, final_results

    def fill_missing_results(
        self,
        missing_results: WebTestResults,
        completed_results: List[WebTestResults],
    ) -> WebTestResults:
        # Handle any platforms with missing results.
        @memoized
        def os_name(port_name):
            return self.host.port_factory.get(port_name).operating_system()

        missing_port = self._port_for_build_step(missing_results.builder_name,
                                                 missing_results.step_name())

        # When a config has no results, we try to guess at what its results are
        # based on other results. We prefer to use results from other builds on
        # the same OS, but fallback to all other builders otherwise (eg: there
        # is usually only one Linux).
        # In both cases, we union the results across the other builders (whether
        # same OS or all builders), so we are usually over-expecting.
        filled_results = []
        _log.warning('No results for %s on %s, inheriting from other builds',
                     missing_results.step_name(), missing_results.builder_name)
        for test_name in self._tests(completed_results):
            if missing_port.skips_test(test_name):
                continue
            # The union of all other actual statuses is used when there is
            # no similar OS to inherit from (eg: no results on Linux, and
            # inheriting from Mac and Win).
            union_actual_all = set()
            # The union of statuses on the same OS is used when there are
            # multiple versions of the same OS with results (eg: no results
            # on Mac10.12, and inheriting from Mac10.15 and Mac11)
            union_actual_sameos = set()
            for completed_suite_results in completed_results:
                result = completed_suite_results.result_for_test(test_name)
                # May not exist because not all platforms run the same tests.
                if not result:
                    continue
                union_actual_all.update(result.actual_results())
                completed_port = self.host.builders.port_name_for_builder_name(
                    completed_suite_results.builder_name)
                if os_name(completed_port) == os_name(missing_port.name()):
                    union_actual_sameos.update(result.actual_results())

            statuses = union_actual_sameos or union_actual_all
            _log.debug(
                'Inheriting %s result for test %s on %s from %s builders.',
                ', '.join(sorted(statuses)), test_name,
                missing_results.builder_name,
                'same OS' if union_actual_sameos else 'all')
            filled_result = WebTestResult(test_name, {
                'actual': ' '.join(sorted(statuses)),
                'is_unexpected': True,
            }, {})
            filled_results.append(filled_result)

        return WebTestResults(filled_results,
                              build=missing_results.build,
                              step_name=missing_results.step_name())

    def _tests(self, results: List[WebTestResults]) -> Set[str]:
        tests = set()
        for builder_results in results:
            tests.update(result.test_name() for result in builder_results)
        return tests

    def filter_results_for_update(
        self,
        test_results: WebTestResults,
        min_attempts_for_update: int = 3,
    ) -> Tuple[Set[str], WebTestResults]:
        """Filters for failing test results that need TestExpectations.

        Arguments:
            min_attempts_for_update: Threshold for the number of attempts at
                which a test's expectations are updated. This prevents excessive
                expectation creation due to infrastructure issues or flakiness.
                Note that this threshold is necessary for updating without
                `--include-unexpected-pass`, but sufficient otherwise.

        Returns:
            * A set of tests that should be handled by rebaselining, which
              captures subtest-level testharness/wdspec results. Tests that
              fail to produce output (crash/timeout) or don't support subtests
              aren't rebaselined.
            * The filtered test results.
        """
        failing_results = []
        if not self.options.include_unexpected_pass:
            # TODO(crbug.com/1149035): Consider suppressing flaky passes.
            for result in test_results.didnt_run_as_expected_results():
                if (result.attempts >= min_attempts_for_update
                        and not result.did_pass()):
                    failing_results.append(result)
        else:
            for result in test_results.didnt_run_as_expected_results():
                if (result.attempts >= min_attempts_for_update
                        or result.did_pass()):
                    failing_results.append(result)

        failing_results = [
            result for result in failing_results
            if ResultType.Skip not in result.actual_results()
        ]
        # TODO(crbug.com/1149035): Extract or make this check configurable
        # to allow updating expectations for non-WPT tests.
        failing_results = [
            result for result in failing_results
            if self._is_wpt_test(result.test_name())
        ]

        tests_to_rebaseline, results_to_update = set(), []
        for result in failing_results:
            if self.can_rebaseline(result):
                tests_to_rebaseline.add(result.test_name())
                # Always assume rebaseline is successful, so delete the line if
                # Failure is the only result. Otherwise, change Failure to Pass.
                # TODO(crbug.com/1149035): Consider rebaseline failed scenario
                # in future, or have `rebaseline-cl` update expectations
                # instead.
                statuses = {*result.actual_results(), ResultType.Pass}
                statuses.discard(ResultType.Failure)
                if len(statuses) > 1:
                    new_leaf = {
                        **result._result_dict, 'actual':
                        ' '.join(sorted(statuses))
                    }
                    result = WebTestResult(result.test_name(), new_leaf,
                                           result.artifacts)
                    results_to_update.append(result)
            else:
                results_to_update.append(result)

        results_to_update = WebTestResults(
            results_to_update,
            step_name=test_results.step_name(),
            incomplete_reason=test_results.incomplete_reason,
            build=test_results.build)
        return tests_to_rebaseline, results_to_update

    def _is_wpt_test(self, test_name):
        """Check if a web test is a WPT tests.

        In blink web tests results, each test name is relative to
        the web_tests directory instead of the wpt directory. We
        need to use the port.is_wpt_test() function to find out if a test
        is from the WPT suite.

        Returns: True if a test is in the external/wpt subdirectory of
            the web_tests directory."""
        return self.port.is_wpt_test(test_name)

    def _platform_specifiers(self, test: str,
                             ports: Collection[Port]) -> Set[str]:
        return {
            self.host.builders.version_specifier_for_port_name(
                port.name()).lower()
            for port in ports if not port.skips_test(test)
        }

    def write_to_test_expectations(self,
                                   results: Collection[WebTestResults],
                                   path: Optional[str] = None):
        """Writes the given lines to the TestExpectations file.

        The place in the file where the new lines are inserted is after a marker
        comment line. If this marker comment line is not found, then everything
        including the marker line is appended to the end of the file.

        Arguments:
            results: A collection of `WebTestResults` from builder/step pairs
                that run the same tests and comprise the supported platforms
                for the file.
            path: Path to the test expectations file to write to. Every port
                must include this file.

        Returns:
            Dictionary mapping test names to lists of test expectation strings.
        """
        path = path or self.port.path_to_generic_test_expectations_file()
        port_by_results = {
            suite_results:
            self._port_for_build_step(suite_results.builder_name,
                                      suite_results.step_name())
            for suite_results in results
        }
        for port in port_by_results.values():
            assert path in port.expectations_dict(), (
                f'{path!r} not in {list(port.expectations_dict())!r} for {port!r}'
            )
        rel_path = self.host.filesystem.relpath(path,
                                                self.port.web_tests_dir())
        _log.info(f'Updating {rel_path!r}')

        exp_by_port = TestExpectationsCache()
        # These expectations are for writing to the file. The exact port doesn't
        # matter, since they should use the same files.
        port_for_file = min(port_by_results.values(), key=Port.name)
        expectations = exp_by_port.load(port_for_file)
        change = ExpectationsChange()
        for test in sorted(filter(self._is_wpt_test, self._tests(results))):
            # Find version specifiers needed to promote versions to their OS
            # (covered versions that are not skipped). For flag-specific
            # expectations, there should only be one covered version at most
            # that will automatically be promoted to a generic line.
            macros = {
                os: set(versions)
                & self._platform_specifiers(test, port_by_results.values())
                for os, versions in
                port_for_file.configuration_specifier_macros().items()
            }
            editor = SystemConfigurationEditor(expectations, path, macros)
            for suite_results, port in port_by_results.items():
                version = self.host.builders.version_specifier_for_port_name(
                    port.name())
                result = suite_results.result_for_test(test)
                if not version or not result:
                    continue
                statuses = frozenset(result.actual_results())
                # Avoid writing flag- or product-specific expectations redundant
                # with generic ones.
                exp_for_port = exp_by_port.load(port)
                if statuses == exp_for_port.get_expectations(test).results:
                    continue
                change += editor.update_versions(
                    test, {version},
                    statuses,
                    reason=' '.join(result.bugs),
                    marker=self.MARKER_COMMENT[len('# '):])
            change += editor.merge_versions(test)

        if not change.lines_added:
            _log.info(f'No lines to write to {rel_path}.')
        expectations.commit_changes()
        new_lines = defaultdict(list)
        for line in change.lines_added:
            new_lines[line.test].append(line.to_string())
        return {test: sorted(lines) for test, lines in new_lines.items()}

    @memoized
    def _port_for_build_step(self, builder: str, step: str) -> Port:
        """"Get the port used to run a build step in CQ/CI."""
        builders = self.host.builders
        port_name = builders.port_name_for_builder_name(builder)
        port = self.host.port_factory.get(port_name)
        port.set_option_default('flag_specific',
                                builders.flag_specific_option(builder, step))
        return port

    def skip_slow_timeout_tests(self, port):
        """Skip any Slow and Timeout tests found in TestExpectations.
        """
        _log.info('Skip Slow and Timeout tests.')
        try:
            test_expectations = TestExpectations(port)
        except ParseError as err:
            _log.warning('Error when parsing TestExpectations\n%s', str(err))
            return False

        path = port.path_to_generic_test_expectations_file()
        changed = False
        for line in test_expectations.get_updated_lines(path):
            if not line.test or line.is_glob:
                continue
            if (ResultType.Timeout in line.results and
                    len(line.results) == 1 and
                    (test_expectations.get_expectations(line.test).is_slow_test or
                        port.is_slow_wpt_test(line.test))):
                test_expectations.remove_expectations(path, [line])
                line.add_expectations({ResultType.Skip})
                test_expectations.add_expectations(path, [line], line.lineno)
                changed = True
        if changed:
            test_expectations.commit_changes()
        return changed

    def cleanup_test_expectations_files(self):
        """Removes deleted tests from expectations files.

        Removes expectations for deleted tests or renames test names in
        expectation files for tests that were renamed. If the
        --clean-up-affected-tests-only command line argument is used then
        only tests deleted in the CL will have their expectations removed
        through this script. If that command line argument is not used then
        expectations for test files that no longer exist will be deleted.
        """
        deleted_files = self._list_deleted_files()
        renamed_files = self._list_renamed_files()
        modified_files = self._list_modified_files()

        for path in self._test_expectations.expectations_dict:
            _log.info('Updating %s for any removed or renamed tests.',
                      self.host.filesystem.basename(path))
            self._clean_single_test_expectations_file(path, deleted_files,
                                                      renamed_files)
        self._test_expectations.commit_changes()

    def _list_files(self, diff_filter):
        paths = self.git.run(
            ['diff', 'origin/main', '--diff-filter=' + diff_filter,
             '--name-only']).splitlines()
        files = []
        for p in paths:
            rel_path = self._relative_to_web_test_dir(p)
            if rel_path:
                files.append(rel_path)
        return files

    def _list_add_files(self):
        return self._list_files('A')

    def _list_modified_files(self):
        return self._list_files('M')

    def _list_deleted_files(self):
        return self._list_files('D')

    def _list_renamed_files(self):
        """Returns a dictionary mapping tests to their new name.

        Regardless of the command line arguments used this test will only
        return a dictionary for tests affected in the current CL.

        Returns a dictionary mapping source name to destination name.
        """
        out = self.git.run([
            'diff', 'origin/main', '-M90%', '--diff-filter=R',
            '--name-status'
        ])
        renamed_tests = {}
        for line in out.splitlines():
            try:
                _, source_path, dest_path = line.split('\t')
            except ValueError:
                _log.info("ValueError for line: %s" % line)
                continue
            source_test = self._relative_to_web_test_dir(source_path)
            dest_test = self._relative_to_web_test_dir(dest_path)
            if source_test and dest_test:
                renamed_tests[source_test] = dest_test
        return renamed_tests

    def _clean_single_test_expectations_file(
            self, path, deleted_files, renamed_files):
        """Cleans up a single test expectations file.

        Args:
            path: Path of expectations file that is being cleaned up.
            deleted_files: List of file paths relative to the web tests
                directory which were deleted.
            renamed_files: Dictionary mapping file paths to their new file
                name after renaming.
        """
        deleted_files = set(deleted_files)
        for line in self._test_expectations.get_updated_lines(path):
            # if a test is a glob type expectation or empty line or comment then
            # add it to the updated expectations file without modifications
            if not line.test or line.is_glob:
                continue
            root_file = self._get_root_file(line.test)
            if root_file in deleted_files:
                self._test_expectations.remove_expectations(path, [line])
            elif root_file in renamed_files:
                self._test_expectations.remove_expectations(path, [line])
                new_file_name = renamed_files[root_file]
                if self.port.is_wpt_test(line.test):
                    # Based on logic in Base._wpt_test_urls_matching_paths
                    line.test = line.test.replace(
                        re.sub(r'\.js$', '.', root_file),
                        re.sub(r'\.js$', '.', new_file_name))
                else:
                    line.test = new_file_name
                self._test_expectations.add_expectations(
                    path, [line], lineno=line.lineno)
            elif not root_file or not self.host.filesystem.isfile(
                    self.finder.path_from_web_tests(root_file)):
                if not self.options.clean_up_affected_tests_only:
                    self._test_expectations.remove_expectations(path, [line])

    @memoized
    def _get_root_file(self, test_name):
        """Finds the physical file in web tests directory for a test

        If a test is a WPT test then it will look in each of the WPT manifests
        for the physical file. If test name cannot be found in any of the manifests
        then the test no longer exists and the function will return None. If a
        test is a legacy web test then it will return the test name.

        Args:
            test_name: Test name which may include test arguments.

        Returns:
            Returns the path of the physical file that backs
            up a test. The path is relative to the web_tests directory.
        """
        if self.port.is_wpt_test(test_name):
            for wpt_manifest in self.wpt_manifests:
                if test_name.startswith(wpt_manifest.wpt_dir):
                    wpt_test = test_name[len(wpt_manifest.wpt_dir) + 1:]
                    if wpt_manifest.is_test_url(wpt_test):
                        return self.host.filesystem.join(
                            wpt_manifest.wpt_dir,
                            wpt_manifest.file_path_for_test_url(wpt_test))
            # The test was not found in any of the wpt manifests, therefore
            # the test does not exist. So we will return None in this case.
            return None
        else:
            # Non WPT tests have no file parameters, and the physical file path
            # is the actual name of the test.
            return test_name

    def _relative_to_web_test_dir(self, path_relative_to_repo_root):
        """Returns a path that's relative to the web tests directory."""
        abs_path = self.finder.path_from_chromium_base(
            path_relative_to_repo_root)
        if not abs_path.startswith(self.finder.web_tests_dir()):
            return None
        return self.host.filesystem.relpath(
            abs_path, self.finder.web_tests_dir())

    # TODO(robertma): Unit test this method.
    def download_text_baselines(self, tests_to_rebaseline):
        """Fetches new baseline files for tests that should be rebaselined.

        Invokes `blink_tool.py rebaseline-cl` in order to download new baselines
        (-expected.txt files) for testharness.js tests that did not crash or
        time out.

        Args:
            tests_to_rebaseline: A list of tests that should be rebaselined.

        Returns: None
        """
        if not tests_to_rebaseline:
            _log.info('No tests to rebaseline.')
            return

        _log.info('Tests to rebaseline:')
        for test in tests_to_rebaseline:
            _log.info('  %s', test)

        # The importer should have already updated the manifests.
        args = ['--no-trigger-jobs', '--no-manifest-update']
        if self.options.verbose:
            args.append('--verbose')
        if self.patchset:
            args.append('--patchset=%d' % self.patchset)
        args += tests_to_rebaseline
        self._run_blink_tool('rebaseline-cl', args)

    def _run_blink_tool(self, subcommand: str, args: List[str]):
        output = self.host.executive.run_command([
            self.host.executable,
            self.finder.path_from_blink_tools('blink_tool.py'),
            subcommand,
            *args,
        ])
        _log.info('Output of %s:', subcommand)
        for line in output.splitlines():
            _log.info('  %s: %s', subcommand, line)
        _log.info('-- end of %s output --', subcommand)

    def can_rebaseline(self, result: WebTestResult) -> bool:
        """Checks if a test can be rebaselined."""
        if self.is_reference_test(result.test_name()):
            return False
        statuses = set(result.actual_results())
        if {ResultType.Pass, ResultType.Failure} <= statuses:
            return False  # Has nondeterministic output; cannot be rebaselined.
        return not (statuses & {ResultType.Crash, ResultType.Timeout})

    def is_reference_test(self, test_name: str) -> bool:
        """Checks whether a given test is a reference test."""
        return bool(self.port.reference_files(test_name))

    @memoized
    def _get_try_bots(self, flag_specific: Optional[str] = None):
        builders = self.host.builders.filter_builders(
            is_try=True,
            exclude_specifiers={'android'},
            flag_specific=flag_specific)
        # Exclude CQ builders like `win-rel`.
        return sorted(
            set(builders) & self.host.builders.builders_for_rebaselining())
