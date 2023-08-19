# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates expectations and baselines when updating web-platform-tests.

Specifically, this class fetches results from try bots for the current CL, then
(1) downloads new baseline files for any tests that can be rebaselined, and
(2) updates the generic TestExpectations file for any other failing tests.
"""

import argparse
import copy
import logging
import re
from collections import defaultdict, namedtuple
from typing import List, Optional, Set

from blinkpy.common.memoized import memoized
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.net.web_test_results import (
    WebTestResult,
    WebTestResults,
)
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models.test_expectations import (
    ExpectationsChange,
    ParseError,
    SystemConfigurationEditor,
    TestExpectations,
)
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


SimpleTestResult = namedtuple('SimpleTestResult', ['expected', 'actual', 'bug'])
DesktopConfig = namedtuple('DesktopConfig', ['port_name'])


class WPTExpectationsUpdater:
    MARKER_COMMENT = '# ====== New tests from wpt-importer added here ======'
    UMBRELLA_BUG = 'crbug.com/626703'

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

    def suite_for_builder(self,
                          builder: str,
                          flag_specific: Optional[str] = None) -> str:
        for step in self.host.builders.step_names_for_builder(builder):
            if self.host.builders.flag_specific_option(builder,
                                                       step) == flag_specific:
                suite_match = re.match(r'(?P<suite>[\w_-]*blink_wpt_tests)',
                                       step)
                if suite_match:
                    return suite_match['suite']
        raise ValueError('"%s" flag-specific suite on "%s" not found' %
                         (flag_specific, builder))

    def update_expectations(self, flag_specific: Optional[str] = None):
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

        build_to_status = self.git_cl.latest_try_jobs(
            builder_names=self._get_try_bots(), patchset=self.patchset)
        _log.debug('Latest try jobs: %r', build_to_status)
        if not build_to_status:
            raise ScriptError('No try job information was collected.')

        fetcher = self.host.results_fetcher
        results = []
        for build, job_status in build_to_status.items():
            if (job_status.result == 'SUCCESS' and
                    not self.options.include_unexpected_pass):
                continue
            try:
                wpt_tests_suite = self.suite_for_builder(
                    build.builder_name, flag_specific)
            except ValueError:
                _log.debug(
                    'Builder %s does not run flag-specific suite %s, skipping',
                    build.builder_name, flag_specific or '(generic)')
                continue
            suite_results = self.host.results_fetcher.gather_results(
                build,
                wpt_tests_suite,
                # `exclude_exonerations=(not include_unexpected_pass)` will leave
                # out unexpected passes as well as other kinds of exonerations
                # (e.g., experimental build). This is good enough in practice.
                not self.options.include_unexpected_pass)
            results.append(suite_results)

        results = self._make_results_for_update(results)
        test_expectations = {}
        for suite_results in results:
            test_expectations = self.merge_dicts(
                test_expectations,
                self.generate_failing_results_dict(suite_results))

        # At this point, test_expectations looks like: {
        #     'test-with-failing-result': {
        #         config1: SimpleTestResult,
        #         config2: SimpleTestResult,
        #         config3: AnotherSimpleTestResult
        #     }
        # }

        # And then we merge results for different platforms that had the same results.
        for test_name, platform_result in test_expectations.items():
            # platform_result is a dict mapping platforms to results.
            test_expectations[test_name] = self.merge_same_valued_keys(
                platform_result)

        # At this point, test_expectations looks like: {
        #     'test-with-failing-result': {
        #         (config1, config2): SimpleTestResult,
        #         (config3,): AnotherSimpleTestResult
        #     }
        # }

        # Do not create expectations for tests which should have baseline
        tests_to_rebaseline, test_expectations = self.get_tests_to_rebaseline(
            test_expectations)
        exp_lines_dict = self.write_to_test_expectations(
            test_expectations, flag_specific)
        return tests_to_rebaseline, exp_lines_dict

    def _make_results_for_update(
        self,
        results: List[WebTestResults],
    ) -> List[WebTestResults]:
        completed_results, missing_results = [], []
        for suite_results in results:
            if len(suite_results) == 0:
                missing_results.append(suite_results)
            else:
                completed_results.append(
                    self.filter_results_for_update(suite_results))
        filled_results = [
            self.fill_missing_results(results, completed_results)
            for results in missing_results
        ]
        return completed_results + filled_results

    def fill_missing_results(
        self,
        missing_results: WebTestResults,
        completed_results: List[WebTestResults],
    ) -> WebTestResults:
        # Handle any platforms with missing results.
        @memoized
        def os_name(port_name):
            return self.host.port_factory.get(port_name).operating_system()

        missing_port = self.host.builders.port_name_for_builder_name(
            missing_results.builder_name)
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
                if os_name(completed_port) == os_name(missing_port):
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
                              builder_name=missing_results.builder_name,
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
    ) -> WebTestResults:
        """Returns a nested dict of failing test results.

        Collects the builder name, platform and a list of tests that did not
        run as expected.

        Args:
            min_attempts_for_update: Threshold for the number of attempts at
                which a test's expectations are updated. This prevents excessive
                expectation creation due to infrastructure issues or flakiness.
                Note that this threshold is necessary for updating without
                `--include-unexpected-pass`, but sufficient otherwise.
        """
        tests_to_update = []
        if not self.options.include_unexpected_pass:
            # TODO(crbug.com/1149035): Consider suppressing flaky passes.
            for result in test_results.didnt_run_as_expected_results():
                if (result.attempts >= min_attempts_for_update
                        and not result.did_pass()):
                    tests_to_update.append(result)
        else:
            for result in test_results.didnt_run_as_expected_results():
                if (result.attempts >= min_attempts_for_update
                        or result.did_pass()):
                    tests_to_update.append(result)

        tests_to_update = [
            result for result in tests_to_update
            if 'SKIP' not in result.actual_results()
        ]
        # TODO(crbug.com/1149035): Extract or make this check configurable
        # to allow updating expectations for non-WPT tests.
        tests_to_update = [
            result for result in tests_to_update
            if self._is_wpt_test(result.test_name())
        ]
        return WebTestResults(tests_to_update,
                              step_name=test_results.step_name(),
                              interrupted=test_results.interrupted,
                              builder_name=test_results.builder_name)

    def generate_failing_results_dict(self, web_test_results):
        """Makes a dict with results for one platform.

        Args:
            web_test_results: A list of WebTestResult objects.

        Returns:
            A dictionary with the structure: {
                'test-name': {
                    ('full-port-name',): SimpleTestResult
                }
            }
        """
        test_dict = {}
        port_name = self.host.builders.port_name_for_builder_name(
            web_test_results.builder_name)
        config = DesktopConfig(port_name=port_name)
        for result in web_test_results.didnt_run_as_expected_results():
            test_name = result.test_name()
            statuses = set(result.actual_results())
            test_dict[test_name] = {
                config:
                # Note: we omit `expected` so that existing expectation lines
                # don't prevent us from merging current results across platform.
                # Eg: if a test FAILs everywhere, it should not matter that it
                # has a pre-existing TIMEOUT expectation on Win7. This code is
                # not currently capable of updating that existing expectation.
                SimpleTestResult(expected="",
                                 actual=' '.join(sorted(statuses)),
                                 bug=self.UMBRELLA_BUG)
            }
        return test_dict

    def _is_wpt_test(self, test_name):
        """Check if a web test is a WPT tests.

        In blink web tests results, each test name is relative to
        the web_tests directory instead of the wpt directory. We
        need to use the port.is_wpt_test() function to find out if a test
        is from the WPT suite.

        Returns: True if a test is in the external/wpt subdirectory of
            the web_tests directory."""
        return self.port.is_wpt_test(test_name)

    def merge_dicts(self, target, source, path=None):
        """Recursively merges nested dictionaries.

        Args:
            target: First dictionary, which is updated based on source.
            source: Second dictionary, not modified.
            path: A list of keys, only used for making error messages.

        Returns:
            The updated target dictionary.
        """
        path = path or []
        for key in source:
            if key in target:
                if (isinstance(target[key], dict)) and isinstance(
                        source[key], dict):
                    self.merge_dicts(target[key], source[key],
                                     path + [str(key)])
                elif target[key] == source[key]:
                    pass
                else:
                    # We have two different SimpleTestResults for the same test
                    # from two different builders. This can happen when a CQ bot
                    # and a blink-rel bot run on the same platform. We union the
                    # actual statuses from both builders.
                    _log.info(
                        "Joining differing results for path %s, key %s\n target:%s\nsource:%s"
                        % (path, key, target[key], source[key]))
                    target[key] = SimpleTestResult(
                        expected=target[key].expected,
                        actual='%s %s' %
                        (target[key].actual, source[key].actual),
                        bug=target[key].bug)
            else:
                target[key] = source[key]
        return target

    def merge_same_valued_keys(self, dictionary):
        """Merges keys in dictionary with same value.

        Traverses through a dict and compares the values of keys to one another.
        If the values match, the keys are combined to a tuple and the previous
        keys are removed from the dict.

        Args:
            dictionary: A dictionary with a dictionary as the value.

        Returns:
            A new dictionary with updated keys to reflect matching values of keys.
            Example: {
                'one': {'foo': 'bar'},
                'two': {'foo': 'bar'},
                'three': {'foo': 'bar'}
            }
            is converted to a new dictionary with that contains
            {('one', 'two', 'three'): {'foo': 'bar'}}
        """
        merged_dict = {}
        matching_value_keys = set()
        keys = sorted(dictionary.keys())
        while keys:
            current_key = keys[0]
            found_match = False
            if current_key == keys[-1]:
                merged_dict[tuple([current_key])] = dictionary[current_key]
                keys.remove(current_key)
                break
            current_result_set = set(dictionary[current_key].actual.split())
            for next_item in keys[1:]:
                if (current_result_set ==
                        set(dictionary[next_item].actual.split())):
                    found_match = True
                    matching_value_keys.update([current_key, next_item])

                if next_item == keys[-1]:
                    if found_match:
                        merged_dict[
                            tuple(sorted(matching_value_keys))] = dictionary[current_key]
                        keys = [
                            k for k in keys if k not in matching_value_keys
                        ]
                    else:
                        merged_dict[tuple([current_key])] = dictionary[current_key]
                        keys.remove(current_key)
            matching_value_keys = set()
        return merged_dict

    def get_expectations(self, result, test_name=''):
        """Returns a set of test expectations based on the result of a test.

        Returns a set of one or more test expectations based on the expected
        and actual results of a given test name. This function is to decide
        expectations for tests that could not be rebaselined.

        Args:
            result: A SimpleTestResult.
            test_name: The test name string (optional).

        Returns:
            A set of one or more test expectation strings with the first letter
            capitalized. Example: {'Failure', 'Timeout'}.
        """
        actual_results = set(result.actual.split())

        # If the result is MISSING, this implies that the test was not
        # rebaselined and has an actual result but no baseline. We can't
        # add a Missing expectation (this is not allowed), but no other
        # expectation is correct.
        if 'MISSING' in actual_results:
            return {ResultType.Skip}
        expectations = set()
        failure_types = {'TEXT', 'IMAGE+TEXT', 'IMAGE', 'AUDIO', 'FAIL'}
        other_types = {'TIMEOUT', 'CRASH', 'PASS'}
        for actual in actual_results:
            if actual in failure_types:
                expectations.add(ResultType.Failure)
            if actual in other_types:
                expectations.add(actual)
        return expectations

    def skipped_specifiers(self, test_name):
        """Returns a list of platform specifiers for which the test is skipped."""
        specifiers = []
        for port in self.all_try_builder_ports():
            if port.skips_test(test_name):
                specifiers.append(
                    self.host.builders.version_specifier_for_port_name(
                        port.name()))
        return specifiers

    @memoized
    def all_try_builder_ports(self):
        """Returns a list of Port objects for all try builders."""
        return [
            self.host.port_factory.get_from_builder_name(name)
            for name in self._get_try_bots()
        ]

    def _platform_specifiers_covered_by_try_bots(
            self, flag_specific: Optional[str] = None):
        all_platform_specifiers = set()
        for builder_name in self._get_try_bots(flag_specific):
            all_platform_specifiers.add(
                self.host.builders.platform_specifier_for_builder(
                    builder_name).lower())
        return frozenset(all_platform_specifiers)

    def write_to_test_expectations(self,
                                   test_expectations,
                                   flag_specific: Optional[str] = None):
        """Writes the given lines to the TestExpectations file.

        The place in the file where the new lines are inserted is after a marker
        comment line. If this marker comment line is not found, then everything
        including the marker line is appended to the end of the file.

        Args:
            test_expectations: A dictionary mapping test names to a dictionary
            mapping platforms and test results.
        Returns:
            Dictionary mapping test names to lists of test expectation strings.
        """
        covered_versions = self._platform_specifiers_covered_by_try_bots(
            flag_specific)
        port = self.host.port_factory.get()
        if flag_specific:
            port.set_option_default('flag_specific', flag_specific)
            path = port.path_to_flag_specific_expectations_file(flag_specific)
        else:
            path = port.path_to_generic_test_expectations_file()
        expectations, change = TestExpectations(port), ExpectationsChange()
        for test in sorted(filter(self._is_wpt_test, test_expectations)):
            skipped_versions = {
                version.lower()
                for version in self.skipped_specifiers(test)
            }
            # Find version specifiers needed to promote versions to their OS
            # (covered versions that are not skipped). For flag-specific
            # expectations, there should only be one covered version at most
            # that will automatically be promoted to a generic line.
            macros = {
                os: [
                    version for version in versions
                    if version in covered_versions - skipped_versions
                ]
                for os, versions in
                self.port.configuration_specifier_macros().items()
            }
            editor = SystemConfigurationEditor(expectations, path, macros)
            for configs, result in test_expectations[test].items():
                versions = set()
                for config in configs:
                    specifier = self.host.builders.version_specifier_for_port_name(
                        config.port_name)
                    if specifier:
                        versions.add(specifier)
                statuses = self.get_expectations(result, test)
                # Avoid writing flag-specific expectations redundant with
                # generic ones.
                if flag_specific and statuses == expectations.get_expectations(
                        test).results:
                    continue
                change += editor.update_versions(
                    test,
                    versions,
                    statuses,
                    reason=result.bug,
                    marker=self.MARKER_COMMENT[len('# '):])
                change += editor.merge_versions(test)
        if not change.lines_added:
            _log.info(
                'No lines to write to %s.',
                self.host.filesystem.relpath(path, self.port.web_tests_dir()))
        expectations.commit_changes()
        new_lines = defaultdict(list)
        for line in change.lines_added:
            new_lines[line.test].append(line.to_string())
        return {test: sorted(lines) for test, lines in new_lines.items()}

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

        args = ['--no-trigger-jobs']
        if self.options.verbose:
            args.append('--verbose')
        if self.patchset:
            args.append('--patchset=%d' % self.patchset)
        args += tests_to_rebaseline
        self._run_blink_tool('rebaseline-cl', args)

    def update_metadata(self):
        """Update WPT metadata for all tests with unexpected results."""
        args = ['--no-trigger-jobs']
        if self.options.verbose:
            args.append('--verbose')
        if self.patchset:
            args.append('--patchset=%d', self.patchset)
        self._run_blink_tool('update-metadata', args)

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

    def get_tests_to_rebaseline(self, test_results):
        """Filters failing tests that can be rebaselined.

        Creates a list of tests to rebaseline depending on the tests' platform-
        specific results. In general, this will be non-ref tests that failed
        due to a baseline mismatch (rather than crash or timeout).

        Args:
            test_results: A dictionary of failing test results, mapping test
                names to lists of platforms to SimpleTestResult.

        Returns:
            A pair: A set of tests to be rebaselined, and a modified copy of
            the test_results dictionary. The tests to be rebaselined should
            include testharness.js tests that failed due to a baseline mismatch.
        """
        new_test_results = copy.deepcopy(test_results)
        tests_to_rebaseline = set()
        for test_name in test_results:
            for platforms, result in test_results[test_name].items():
                if self.can_rebaseline(test_name, result):
                    # We always assume rebaseline is successful, so we delete
                    # the line if Failure is the only result, otherwise we
                    # change Failure to Pass.
                    # TODO: consider rebaseline failed scenario in future
                    self.update_test_results_for_rebaselined_test(
                        new_test_results, test_name, platforms)
                    tests_to_rebaseline.add(test_name)

        return sorted(tests_to_rebaseline), new_test_results

    def update_test_results_for_rebaselined_test(self, test_results, test_name, platforms):
        """Update test results if we successfully rebaselined a test

        After rebaseline, a failed test will now pass. And if 'PASS' is the
        only result, we don't add one line for that.
        We are assuming rebaseline is always successful for now. We can
        improve this logic in future once rebaseline-cl can tell which test
        it failed to rebaseline.
        """
        result = test_results[test_name][platforms]
        actual = set(result.actual.split(' '))
        if 'FAIL' in actual:
            actual.remove('FAIL')
            actual.add('PASS')
            if len(actual) == 1:
                del test_results[test_name][platforms]
            else:
                test_results[test_name][platforms] = SimpleTestResult(expected=result.expected,
                                                                      actual=' '.join(sorted(actual)),
                                                                      bug=result.bug)

    def can_rebaseline(self, test_name, result):
        """Checks if a test can be rebaselined.

        Args:
            test_name: The test name string.
            result: A SimpleTestResult.
        """
        if self.is_reference_test(test_name):
            return False
        if any(x in result.actual for x in ('CRASH', 'TIMEOUT', 'MISSING')):
            return False
        return True

    def is_reference_test(self, test_name):
        """Checks whether a given test is a reference test."""
        return bool(self.port.reference_files(test_name))

    @memoized
    def _get_try_bots(self, flag_specific: Optional[str] = None):
        return self.host.builders.filter_builders(
            is_try=True,
            exclude_specifiers={'android'},
            flag_specific=flag_specific)
