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
from typing import List, Optional

from blinkpy.common.memoized import memoized
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.log_utils import configure_logging
from blinkpy.web_tests.models.test_expectations import (
    ParseError, SystemConfigurationRemover, TestExpectations)
from blinkpy.web_tests.models.typ_types import ResultType

_log = logging.getLogger(__name__)


# TODO(robertma): Investigate reusing web_tests.models.test_expectations and
# alike in this module.

SimpleTestResult = namedtuple('SimpleTestResult', ['expected', 'actual', 'bug'])

DesktopConfig = namedtuple('DesktopConfig', ['port_name'])


class WPTExpectationsUpdater(object):
    MARKER_COMMENT = '# ====== New tests from wpt-importer added here ======'
    UMBRELLA_BUG = 'crbug.com/626703'

    def __init__(self, host, args=None, wpt_manifests=None):
        self.host = host
        self.port = self.host.port_factory.get()
        self.finder = PathFinder(self.host.filesystem)
        self.git_cl = GitCL(host)
        self.git = self.host.git(self.finder.chromium_base())
        self.configs_with_no_results = []
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
        self.testid_prefix = "ninja://:blink_wpt_tests/"
        self.test_suite = "blink_wpt_tests"

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

    def update_expectations_for_flag_specific(self, flag_specific):
        """Adds test expectations lines for flag specific builders.

        Returns:
            A pair: A set of tests that should be rebaselined, and a dictionary
            mapping tests that couldn't be rebaselined to lists of expectation
            lines written to flag specific test expectations.
        """
        # TODO(crbug.com/1344709): This method has no coverage and should be
        # merged with `update_expectations`.
        self.port.wpt_manifest.cache_clear()

        issue_number = self.get_issue_number()
        if issue_number == 'None':
            raise ScriptError('No issue on current branch.')

        # TODO(crbug.com/1406978): Retrieve builder names from the config
        # instead of hardcoding.
        builder = 'linux-blink-rel'
        builder_names = [builder]
        test_suite = self.suite_for_builder(builder, flag_specific)

        build_to_status = self.git_cl.latest_try_jobs(
            builder_names=builder_names,
            patchset=self.patchset)
        if not build_to_status:
            raise ScriptError('No try job information was collected.')

        # Here we build up a dict of failing test results for all platforms.
        test_expectations = {}
        for build, job_status in build_to_status.items():
            if (job_status.result == 'SUCCESS' and
                    not self.options.include_unexpected_pass):
                continue
            # Temporary logging for https://crbug.com/1154650
            result_dicts = self.get_failing_results_dicts(build, test_suite)
            _log.info('Merging failing results dicts for %s', build)
            for result_dict in result_dicts:
                test_expectations = self.merge_dicts(
                    test_expectations, result_dict)

        generic_expectations = TestExpectations(self.port)

        # Do not create expectations for tests which should have baseline
        tests_to_rebaseline, test_expectations = self.get_tests_to_rebaseline(
            test_expectations)
        exp_lines_dict = self.write_to_test_expectations(test_expectations,
                                                         flag_specific,
                                                         generic_expectations)
        return tests_to_rebaseline, exp_lines_dict

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

        issue_number = self.get_issue_number()
        if issue_number == 'None':
            raise ScriptError('No issue on current branch.')

        build_to_status = self.git_cl.latest_try_jobs(
            builder_names=self._get_try_bots(), patchset=self.patchset)
        _log.debug('Latest try jobs: %r', build_to_status)
        if not build_to_status:
            raise ScriptError('No try job information was collected.')

        # Here we build up a dict of failing test results for all platforms.
        test_expectations = {}
        for build, job_status in build_to_status.items():
            if (job_status.result == 'SUCCESS' and
                    not self.options.include_unexpected_pass):
                continue
            # Temporary logging for https://crbug.com/1154650
            result_dicts = self.get_failing_results_dicts(build, self.test_suite)
            _log.info('Merging failing results dicts for %s', build)
            for result_dict in result_dicts:
                test_expectations = self.merge_dicts(
                    test_expectations, result_dict)

        # At this point, test_expectations looks like: {
        #     'test-with-failing-result': {
        #         config1: SimpleTestResult,
        #         config2: SimpleTestResult,
        #         config3: AnotherSimpleTestResult
        #     }
        # }

        self.add_results_for_configs_without_results(
            test_expectations, self.configs_with_no_results)

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
        exp_lines_dict = self.write_to_test_expectations(test_expectations)
        return tests_to_rebaseline, exp_lines_dict

    def add_results_for_configs_without_results(self, test_expectations,
                                                configs_with_no_results):
        # Handle any platforms with missing results.
        def os_name(port_name):
            # Port names are typically "os-os_version". So we can grab the os by
            # taking everything up to the '-'
            if '-' not in port_name:
                return port_name
            return port_name[:port_name.rfind('-')]

        # When a config has no results, we try to guess at what its results are
        # based on other results. We prefer to use results from other builds on
        # the same OS, but fallback to all other builders otherwise (eg: there
        # is usually only one Linux).
        # In both cases, we union the results across the other builders (whether
        # same OS or all builders), so we are usually over-expecting.
        for config_no_result in configs_with_no_results:
            _log.warning("No results for %s, inheriting from other builds" %
                         str(config_no_result))
            for test_name in test_expectations:
                # The union of all other actual statuses is used when there is
                # no similar OS to inherit from (eg: no results on Linux, and
                # inheriting from Mac and Win).
                union_actual_all = set()
                # The union of statuses on the same OS is used when there are
                # multiple versions of the same OS with results (eg: no results
                # on Mac10.12, and inheriting from Mac10.15 and Mac11)
                union_actual_sameos = set()
                config_result_dict = test_expectations[test_name]
                for config in config_result_dict.keys():
                    result = config_result_dict[config]
                    union_actual_all.add(result.actual)
                    if os_name(config.port_name) == os_name(
                            config_no_result.port_name):
                        union_actual_sameos.add(result.actual)

                statuses = union_actual_sameos or union_actual_all
                union_result = SimpleTestResult(expected="",
                                                actual=" ".join(sorted(statuses)),
                                                bug=self.UMBRELLA_BUG)
                _log.debug("Inheriting result for test %s on config %s. "
                           "Same-os? %s Result: %s." %
                           (test_name, config_no_result,
                            len(union_actual_sameos) > 0, union_result))
                test_expectations[test_name][config_no_result] = union_result

    def get_issue_number(self):
        """Returns current CL number. Can be replaced in unit tests."""
        return self.git_cl.get_issue_number()

    def get_failing_results_dicts(self, build, test_suite):
        """Returns a list of nested dicts of failing test results.

        Retrieves a full list of web test results from a builder result URL.
        Collects the builder name, platform and a list of tests that did not
        run as expected.

        Args:
            build: A Build object.

        Returns:
            A list of dictionaries that have the following structure.

            {
                'test-with-failing-result': {
                    config: SimpleTestResult
                }
            }

            If results could be fetched but none are failing,
            this will return an empty list.
        """

        func = lambda x: (x["variant"]["def"]["test_suite"] == test_suite)
        test_results_list = []
        predicate = {"expectancy": "VARIANTS_WITH_ONLY_UNEXPECTED_RESULTS"}
        rv = self.host.results_fetcher.fetch_results_from_resultdb([build],
                                                                   predicate)
        rv = list(filter(func, rv))
        if not self.options.include_unexpected_pass:
            # if a test first fail then passed unexpectedly
            passed_test_ids = set([r["testId"] for r in rv if r["status"] == "PASS"])
            rv = [r for r in rv if r["testId"] not in passed_test_ids]
            # only create test expectations for tests that had enough retries,
            # so that we don't create excessive test expectations due to bot
            # issues.
            test_ids = [r["testId"] for r in rv]
            rv = [r for r in rv if test_ids.count(r["testId"]) >= 3]
        else:
            passed_test_ids = set([r["testId"] for r in rv if r["status"] == "PASS"])
            test_ids = [r["testId"] for r in rv]
            rv = [r for r in rv if r["testId"] in passed_test_ids or test_ids.count(r["testId"]) >= 3]

        test_results_list.extend(rv)

        has_webdriver_tests = self.host.builders.has_webdriver_tests_for_builder(
            build.builder_name)

        webdriver_test_results = []
        if has_webdriver_tests:
            mst = self.host.builders.main_for_builder(build.builder_name)
            webdriver_test_results.append(
                self.host.results_fetcher.fetch_webdriver_test_results(
                    build, mst))

        webdriver_test_results = filter(None, webdriver_test_results)
        if not test_results_list and not webdriver_test_results:
            _log.warning('No results for build %s', build)
            self.configs_with_no_results.extend(self.get_builder_configs(build))
            return []

        unexpected_test_results = []
        unexpected_test_results.append(
            self.generate_failing_results_dict_from_resultdb(
                build, test_results_list))

        for results_set in webdriver_test_results:
            results_dict = self.generate_failing_results_dict(
                build, results_set)
            if results_dict:
                unexpected_test_results.append(results_dict)
        unexpected_test_results = filter(None, unexpected_test_results)
        return unexpected_test_results

    def _get_web_test_results(self, build):
        """Gets web tests results for a builder.

        Args:
            build: Named tuple containing builder name and number

        Returns:
            List of web tests results for each web test step
            in build.
        """
        return [self.host.results_fetcher.fetch_results(build)]

    def get_builder_configs(self, build, *_):
        return [DesktopConfig(port_name=self.port_name(build))]

    @memoized
    def port_name(self, build):
        return self.host.builders.port_name_for_builder_name(
            build.builder_name)

    def generate_failing_results_dict_from_resultdb(self, build, results_list):
        """Makes a dict with results for one platform.

        Args:
            builder: Builder instance containing builder information..
            results_list: A list of results retrieved from ResultDB

        Returns:
            A dictionary with the structure: {
                'test-name': {
                    ('full-port-name',): SimpleTestResult
                }
            }
        """
        configs = self.get_builder_configs(build)
        if len(configs) > 1:
            raise ScriptError('More than one configs were produced for'
                              ' builder and web tests step combination')
        if not configs:
            raise ScriptError('No configuration was found for builder and web test'
                              ' step combination ')
        config = configs[0]
        test_dict = defaultdict(set)
        for result in results_list:
            test_name = result["testId"][len(self.testid_prefix):]
            if not self._is_wpt_test(test_name):
                continue
            if result["status"] == "SKIP":
                continue
            status = "TIMEOUT" if result["status"] == "ABORT" else result["status"]
            if status == "PASS" and not self.options.include_unexpected_pass:
                continue
            test_dict[test_name].add(status)

        rv = {}
        for test_name, result in test_dict.items():
            rv[test_name] = {
                config:
                # Note: we omit `expected` so that existing expectation lines
                # don't prevent us from merging current results across platform.
                # Eg: if a test FAILs everywhere, it should not matter that it
                # has a pre-existing TIMEOUT expectation on Win7. This code is
                # not currently capable of updating that existing expectation.
                SimpleTestResult(expected="",
                                 actual=" ".join(sorted(result)),
                                 bug=self.UMBRELLA_BUG)
            }
        return rv or None

    def generate_failing_results_dict(self, build, web_test_results):
        """Makes a dict with results for one platform.

        Args:
            builder: Builder instance containing builder information..
            web_test_results: A list of WebTestResult objects.

        Returns:
            A dictionary with the structure: {
                'test-name': {
                    ('full-port-name',): SimpleTestResult
                }
            }
        """
        test_dict = {}
        configs = self.get_builder_configs(build, web_test_results)
        _log.debug(
            'Getting failing results dictionary for %s step in latest %s build',
            web_test_results.step_name(), build.builder_name)

        if len(configs) > 1:
            raise ScriptError('More than one configs were produced for'
                              ' builder and web tests step combination')
        if not configs:
            raise ScriptError('No configuration was found for builder and web test'
                              ' step combination ')
        config = configs[0]
        for result in web_test_results.didnt_run_as_expected_results():
            # TODO(rmhasan) If a test fails unexpectedly then it runs multiple
            # times until, it passes or a retry limit is reached. Even though
            # it passed we there are still flaky failures that we are not
            # creating test expectations for. Maybe we should add a mode
            # which creates expectations for tests that are flaky but still
            # pass in a web test step.

            # Create flaky expectations for flaky tests on Android. In order to
            # do this we should add 'Pass' to all tests with failing
            # expectations that pass in the patchset's try job.
            if result.did_pass() and not self.options.include_unexpected_pass:
                continue

            test_name = result.test_name()
            if not self._is_wpt_test(test_name):
                continue
            test_dict[test_name] = {
                config:
                # Note: we omit `expected` so that existing expectation lines
                # don't prevent us from merging current results across platform.
                # Eg: if a test FAILs everywhere, it should not matter that it
                # has a pre-existing TIMEOUT expectation on Win7. This code is
                # not currently capable of updating that existing expectation.
                SimpleTestResult(expected="",
                                 actual=result.actual_results(),
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
            return {'Skip'}
        expectations = set()
        failure_types = {'TEXT', 'IMAGE+TEXT', 'IMAGE', 'AUDIO', 'FAIL'}
        other_types = {'TIMEOUT', 'CRASH', 'PASS'}
        for actual in actual_results:
            if actual in failure_types:
                expectations.add('Failure')
            if actual in other_types:
                expectations.add(actual.capitalize())
        return expectations

    def remove_configurations(self, configs_to_remove):
        """Removes configs from test expectations files for some tests

        Args:
            configs_to_remove: A dict maps test names to set of os versions:
                {
                    'test-with-failing-result': ['os1', 'os2', ...]
                }
        Returns: None
        """
        # SystemConfigurationRemover now only works on generic test expectations
        # files. This is good enough for now
        path = self.port.path_to_generic_test_expectations_file()
        test_expectations = TestExpectations(
            self.port,
            expectations_dict={
                path: self.host.filesystem.read_text_file(path),
            })
        system_remover = SystemConfigurationRemover(self.host.filesystem, test_expectations)
        for test, versions in configs_to_remove.items():
            system_remover.remove_os_versions(test, versions)
        system_remover.update_expectations()

    def create_line_dict_for_flag_specific(self, merged_results, generic_expectations):
        """Creates list of test expectations lines for flag specific builder.

        Traverses through the given |merged_results| dictionary and parses the
        value to create one test expectations line per key. If a test expectation
        from generic expectations can be inherited, we will reuse that expectation
        so that we can keep the file size small. Flag specific expectations
        does not use platform tag, so we don't need handle conflicts either.

        Test expectation lines have the following format:
            ['BUG_URL TEST_NAME [EXPECTATION(S)]']

        Args:
            merged_results: A dictionary with the format:
                {
                    'test-with-failing-result': {
                        (config1,): SimpleTestResult
                    }
                }

        Returns:
            line_dict: A dictionary from test names to a list of test
                       expectation lines
                       (each SimpleTestResult turns into a line).
            configs_to_remove: An empty dictionary
        """
        line_dict = defaultdict(list)
        for test_name, test_results in sorted(merged_results.items()):
            if not self._is_wpt_test(test_name):
                _log.warning(
                    'Non-WPT test "%s" unexpectedly passed to create_line_dict.',
                    test_name)
                continue
            expectation_line = generic_expectations.get_expectations(test_name)
            expectations = expectation_line.results
            for configs, result in sorted(test_results.items()):
                new_expectations = self.get_expectations(result, test_name)
                if 'Failure' in new_expectations:
                    new_expectations.remove('Failure')
                    new_expectations.add('FAIL')
                if new_expectations != expectations:
                    line_dict[test_name].extend(
                        self._create_lines(test_name, [], result))
                # for flag-specific builders, we always have one config for each
                # test, so quit the loop here
                break

        return line_dict, {}


    def create_line_dict(self, merged_results):
        """Creates list of test expectations lines.

        Traverses through the given |merged_results| dictionary and parses the
        value to create one test expectations line per key.

        Test expectation lines have the following format:
            ['BUG_URL [PLATFORM(S)] TEST_NAME [EXPECTATION(S)]']

        Args:
            merged_results: A dictionary with the format:
                {
                    'test-with-failing-result': {
                        (config1, config2): SimpleTestResult,
                        (config3,): SimpleTestResult
                    }
                }

        Returns:
            line_dict: A dictionary from test names to a list of test
                       expectation lines
                       (each SimpleTestResult turns into a line).
            configs_to_remove: A dictionary from test names to a set
                               of os specifiers
        """
        line_dict = defaultdict(list)
        configs_to_remove = defaultdict(set)
        for test_name, test_results in sorted(merged_results.items()):
            if not self._is_wpt_test(test_name):
                _log.warning(
                    'Non-WPT test "%s" unexpectedly passed to create_line_dict.',
                    test_name)
                continue
            for configs, result in sorted(test_results.items()):
                line_dict[test_name].extend(
                    self._create_lines(test_name, configs, result))
                for config in configs:
                    configs_to_remove[test_name].add(
                        self.host.builders.version_specifier_for_port_name(
                            config.port_name))

        return line_dict, configs_to_remove

    def _create_lines(self, test_name, configs, result):
        """Constructs test expectation line strings.

        Args:
            test_name: The test name string.
            configs: A list of full configs that the line should apply to.
            result: A SimpleTestResult.

        Returns:
            A list of strings which each is a line of test expectation for given
            |test_name|.
        """
        lines = []

        expectations = '[ %s ]' % \
            ' '.join(self.get_expectations(result, test_name))
        for specifier in self.normalized_specifiers(test_name, configs):
            line_parts = []
            if specifier:
                line_parts.append('[ %s ]' % specifier)
            # Escape literal asterisks for typ (https://crbug.com/1036130).
            # TODO(weizhong): consider other escapes we added recently
            line_parts.append(test_name.replace('*', '\\*'))
            line_parts.append(expectations)

            # Only add the bug link if the expectations do not include SKIP.
            if 'Skip' not in expectations and result.bug:
                line_parts.insert(0, result.bug)

            lines.append(' '.join(line_parts))
        return lines

    def normalized_specifiers(self, test_name, configs):
        """Converts and simplifies ports into platform specifiers.

        Args:
            test_name: The test name string.
            configs: A list of full configs that the line should apply to.

        Returns:
            A list of specifier string, e.g. ["Mac", "Win"].
            [''] will be returned if the line should apply to all platforms.
        """
        if not configs:
            return ['']

        specifiers = []
        for config in configs:
            specifiers.append(
                self.host.builders.version_specifier_for_port_name(
                    config.port_name))

        if self.specifiers_can_extend_to_all_platforms(specifiers, test_name):
            return ['']

        specifiers = self.simplify_specifiers(
            specifiers, self.port.configuration_specifier_macros())
        if not specifiers:
            return ['']
        return specifiers

    def specifiers_can_extend_to_all_platforms(self, specifiers, test_name):
        """Tests whether a list of specifiers can be extended to all platforms.

        Tries to add skipped platform specifiers to the list and tests if the
        extended list covers all platforms.
        """
        extended_specifiers = specifiers + self.skipped_specifiers(test_name)
        # If the list is simplified to empty, then all platforms are covered.
        return not self.simplify_specifiers(
            extended_specifiers, self.port.configuration_specifier_macros())

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

    def simplify_specifiers(self, specifiers, specifier_macros):
        """Simplifies the specifier part of an expectation line if possible.

        "Simplifying" means finding the shortest list of platform specifiers
        that is equivalent to the given list of specifiers. This can be done
        because there are "macro specifiers" that stand in for multiple version
        specifiers, and an empty list stands in for "all platforms".

        Args:
            specifiers: A collection of specifiers (case insensitive).
            specifier_macros: A dict mapping "macros" for groups of specifiers
                to lists of version specifiers. e.g. {"win": ["win10", "win11"]}.
                If there are versions in this dict for that have no corresponding
                try bots, they are ignored.

        Returns:
            A shortened list of specifiers (capitalized). For example, ["win10",
            "win11"] would be converted to ["Win"]. If the given list covers
            all supported platforms, then an empty list is returned.
        """
        specifiers = {s.lower() for s in specifiers}
        covered_by_try_bots = self._platform_specifiers_covered_by_try_bots()
        for macro, versions in specifier_macros.items():
            macro = macro.lower()

            # Only consider version specifiers that have corresponding try bots.
            versions = {
                s.lower()
                for s in versions if s.lower() in covered_by_try_bots
            }
            if len(versions) == 0:
                continue
            if versions <= specifiers:
                specifiers -= versions
                specifiers.add(macro)
        if specifiers == {macro.lower() for macro in specifier_macros}:
            return []
        return sorted(specifier.capitalize() for specifier in specifiers)

    def _platform_specifiers_covered_by_try_bots(self):
        all_platform_specifiers = set()
        for builder_name in self._get_try_bots():
            all_platform_specifiers.add(
                self.host.builders.platform_specifier_for_builder(
                    builder_name).lower())
        return frozenset(all_platform_specifiers)

    def write_to_test_expectations(self, test_expectations,
                                   flag_specific=None,
                                   generic_expectations=None):
        """Writes the given lines to the TestExpectations file.

        The place in the file where the new lines are inserted is after a marker
        comment line. If this marker comment line is not found, then everything
        including the marker line is appended to the end of the file.

        All WontFix tests are inserted to NeverFixTests file instead of TextExpectations
        file.

        Args:
            test_expectations: A dictionary mapping test names to a dictionary
            mapping platforms and test results.
        Returns:
            Dictionary mapping test names to lists of test expectation strings.
        """
        if flag_specific:
            line_dict, configs_to_remove = self.create_line_dict_for_flag_specific(
                test_expectations, generic_expectations)
        else:
            line_dict, configs_to_remove = self.create_line_dict(test_expectations)
        if not line_dict:
            _log.info(
                'No lines to write to %s, WebdriverExpectations'
                ' or NeverFixTests.' % (flag_specific or 'TestExpectations')
            )
            return {}

        if configs_to_remove:
            _log.info('Clean up stale expectations that'
                      ' could conflict with new expectations')
            self.remove_configurations(configs_to_remove)

        line_list = []
        wont_fix_list = []
        webdriver_list = []
        for lines in line_dict.values():
            for line in lines:
                if self.finder.webdriver_prefix() in line:
                    webdriver_list.append(line)
                else:
                    line_list.append(line)

        if flag_specific:
            list_to_expectation = {
                self.port.path_to_flag_specific_expectations_file(flag_specific): line_list
            }
        else:
            list_to_expectation = {
                self.port.path_to_generic_test_expectations_file(): line_list,
                self.port.path_to_webdriver_expectations_file(): webdriver_list
            }
        for expectations_file_path, lines in list_to_expectation.items():
            if not lines:
                continue

            _log.info('Lines to write to %s:\n %s', expectations_file_path,
                      '\n'.join(lines))
            # Writes to TestExpectations file.
            file_contents = self.host.filesystem.read_text_file(
                expectations_file_path)

            marker_comment_index = file_contents.find(self.MARKER_COMMENT)
            if marker_comment_index == -1:
                file_contents += '\n%s\n' % self.MARKER_COMMENT
                file_contents += '\n'.join(lines)
            else:
                end_of_marker_line = (file_contents[marker_comment_index:].
                                      find('\n')) + marker_comment_index
                file_contents = (
                    file_contents[:end_of_marker_line + 1] + '\n'.join(lines) +
                    file_contents[end_of_marker_line:])

            self.host.filesystem.write_text_file(expectations_file_path,
                                                 file_contents)

        # only write to NeverFixTests for the generic round
        if wont_fix_list and flag_specific is None:
            _log.info('Lines to write to NeverFixTests:\n %s',
                      '\n'.join(wont_fix_list))
            # Writes to NeverFixTests file.
            wont_fix_path = self.port.path_to_never_fix_tests_file()
            wont_fix_file_content = self.host.filesystem.read_text_file(
                wont_fix_path)
            if not wont_fix_file_content.endswith('\n'):
                wont_fix_file_content += '\n'
            wont_fix_file_content += '\n'.join(wont_fix_list)
            wont_fix_file_content += '\n'
            self.host.filesystem.write_text_file(wont_fix_path,
                                                 wont_fix_file_content)
        return line_dict

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
                if self.finder.is_webdriver_test_path(line.test):
                    _, subtest_suffix = self.port.split_webdriver_test_name(line.test)
                    line.test = self.port.add_webdriver_subtest_suffix(
                        new_file_name, subtest_suffix)
                elif self.port.is_wpt_test(line.test):
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
        then the test no longer exists and the function will return None. If a file
        is webdriver test then it will strip all subtest arguments and return the
        file path. If a test is a legacy web test then it will return the test name.

        Args:
            test_name: Test name which may include test arguments.

        Returns:
            Returns the path of the physical file that backs
            up a test. The path is relative to the web_tests directory.
        """
        if self.finder.is_webdriver_test_path(test_name):
            root_test_file, _ = (
                self.port.split_webdriver_test_name(test_name))
            return root_test_file
        elif self.port.is_wpt_test(test_name):
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
            # Non WPT and non webdriver tests have no file parameters, and
            # the physical file path is the actual name of the test.
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
        if self.is_webdriver_test(test_name):
            return False
        return True

    def is_reference_test(self, test_name):
        """Checks whether a given test is a reference test."""
        return bool(self.port.reference_files(test_name))

    def is_webdriver_test(self, test_name):
        """Checks whether a given test is a WebDriver test."""
        return self.finder.is_webdriver_test_path(test_name)

    @memoized
    def _get_try_bots(self):
        return self.host.builders.filter_builders(
            is_try=True, exclude_specifiers={'android'})
