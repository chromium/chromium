# Copyright 2016 The Chromium Authors. All rights reserved.
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
from collections import defaultdict, namedtuple

from blinkpy.common.memoized import memoized
from blinkpy.common.net.git_cl import GitCL
from blinkpy.common.path_finder import PathFinder
from blinkpy.common.system.executive import ScriptError
from blinkpy.common.system.log_utils import configure_logging

_log = logging.getLogger(__name__)

MARKER_COMMENT = '# ====== New tests from wpt-importer added here ======'
UMBRELLA_BUG = 'crbug.com/626703'

# TODO(robertma): Investigate reusing web_tests.models.test_expectations and
# alike in this module.

SimpleTestResult = namedtuple('SimpleTestResult', ['expected', 'actual', 'bug'])


class WPTExpectationsUpdater(object):

    def __init__(self, host):
        self.host = host
        self.port = self.host.port_factory.get()
        self.git_cl = GitCL(host)
        self.finder = PathFinder(self.host.filesystem)
        self.ports_with_no_results = set()
        self.ports_with_all_pass = set()
        self.patchset = None

    def run(self, args=None):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('--patchset', default=None,
                            help='Patchset number to fetch new baselines from.')
        parser.add_argument('-v', '--verbose', action='store_true', help='More verbose logging.')
        args = parser.parse_args(args)

        log_level = logging.DEBUG if args.verbose else logging.INFO
        configure_logging(logging_level=log_level, include_time=True)

        self.patchset = args.patchset
        self.update_expectations()

        return 0

    def update_expectations(self):
        """Downloads text new baselines and adds test expectations lines.

        Returns:
            A pair: A set of tests that are rebaselined, and a dictionary
            mapping tests that couldn't be rebaselined to lists of expectation
            lines written to TestExpectations.
        """
        issue_number = self.get_issue_number()
        if issue_number == 'None':
            raise ScriptError('No issue on current branch.')

        build_to_status = self.get_latest_try_jobs()
        _log.debug('Latest try jobs: %r', build_to_status)
        if not build_to_status:
            raise ScriptError('No try job information was collected.')

        # Here we build up a dict of failing test results for all platforms.
        test_expectations = {}
        for build, job_status in build_to_status.iteritems():
            if job_status.result == 'SUCCESS':
                self.ports_with_all_pass.add(self.port_name(build))
            port_results = self.get_failing_results_dict(build)
            test_expectations = self.merge_dicts(test_expectations, port_results)

        # And then we merge results for different platforms that had the same results.
        for test_name, platform_result in test_expectations.iteritems():
            # platform_result is a dict mapping platforms to results.
            test_expectations[test_name] = self.merge_same_valued_keys(platform_result)

        # At this point, test_expectations looks like: {
        #     'test-with-failing-result': {
        #         ('port-name1', 'port-name2'): SimpleTestResult,
        #         'port-name3': AnotherSimpleTestResult
        #     }
        # }

        rebaselined_tests, test_expectations = self.download_text_baselines(test_expectations)
        test_expectation_lines = self.create_line_dict(test_expectations)
        self.write_to_test_expectations(test_expectation_lines)
        return rebaselined_tests, test_expectation_lines

    def get_issue_number(self):
        """Returns current CL number. Can be replaced in unit tests."""
        return self.git_cl.get_issue_number()

    def get_latest_try_jobs(self):
        """Returns the latest finished try jobs as Build objects."""
        return self.git_cl.latest_try_jobs(builder_names=self._get_try_bots(), patchset=self.patchset)

    def get_failing_results_dict(self, build):
        """Returns a nested dict of failing test results.

        Retrieves a full list of web test results from a builder result URL.
        Collects the builder name, platform and a list of tests that did not
        run as expected.

        Args:
            build: A Build object.

        Returns:
            A dictionary with the structure: {
                'test-with-failing-result': {
                    'full-port-name': SimpleTestResult
                }
            }
            If results could be fetched but none are failing,
            this will return an empty dictionary.
        """
        port_name = self.port_name(build)
        if port_name in self.ports_with_all_pass:
            # All tests passed, so there should be no failing results.
            return {}

        test_result_list = [self.host.results_fetcher.fetch_results(build)]
        has_webdriver_tests = self.host.builders.has_webdriver_tests_for_builder(
            build.builder_name)
        if has_webdriver_tests:
            master = self.host.builders.master_for_builder(
                build.builder_name)
            test_result_list.append(
                self.host.results_fetcher.fetch_webdriver_test_results(build, master))

        test_result_list = filter(None, test_result_list)
        if not test_result_list:
            _log.warning('No results for build %s', build)
            self.ports_with_no_results.add(self.port_name(build))
            return {}

        failing_test_results = []
        for test_result in test_result_list:
            failing_test_results += [
                result for result in test_result.didnt_run_as_expected_results() if not result.did_pass()]

        return self.generate_results_dict(self.port_name(build), failing_test_results)

    @memoized
    def port_name(self, build):
        return self.host.builders.port_name_for_builder_name(build.builder_name)

    def generate_results_dict(self, full_port_name, web_test_results):
        """Makes a dict with results for one platform.

        Args:
            full_port_name: The fully-qualified port name, e.g. "win-win10".
            web_test_results: A list of WebTestResult objects.

        Returns:
            A dictionary with the structure: {
                'test-name': {
                    'full-port-name': SimpleTestResult
                }
            }
        """
        test_dict = {}
        for result in web_test_results:
            test_name = result.test_name()

            if not self.port.is_wpt_test(test_name):
                continue

            test_dict[test_name] = {
                full_port_name: SimpleTestResult(
                    expected=result.expected_results(),
                    actual=result.actual_results(),
                    bug=UMBRELLA_BUG
                )
            }
        return test_dict

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
                if (isinstance(target[key], dict)) and isinstance(source[key], dict):
                    self.merge_dicts(target[key], source[key], path + [str(key)])
                elif target[key] == source[key]:
                    pass
                else:
                    raise ValueError('The key: %s already exist in the target dictionary.' % '.'.join(path))
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
                merged_dict[current_key] = dictionary[current_key]
                keys.remove(current_key)
                break

            for next_item in keys[1:]:
                if dictionary[current_key] == dictionary[next_item]:
                    found_match = True
                    matching_value_keys.update([current_key, next_item])

                if next_item == keys[-1]:
                    if found_match:
                        merged_dict[tuple(matching_value_keys)] = dictionary[current_key]
                        keys = [k for k in keys if k not in matching_value_keys]
                    else:
                        merged_dict[current_key] = dictionary[current_key]
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
        # We also want to skip any new manual tests that are not automated;
        # see crbug.com/708241 for context.
        if 'MISSING' in actual_results:
            return {'Skip'}
        if '-manual.' in test_name and 'TIMEOUT' in actual_results:
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
                        ('port-name1', 'port-name2'): SimpleTestResult,
                        'port-name3': SimpleTestResult
                    }
                }

        Returns:
            A dictionary from test names to a list of test expectation lines
            (each SimpleTestResult turns into a line).
        """
        line_dict = defaultdict(list)
        for test_name, port_results in sorted(merged_results.iteritems()):
            if not self.port.is_wpt_test(test_name):
                _log.warning('Non-WPT test "%s" unexpectedly passed to create_line_dict.', test_name)
                continue
            for port_names, result in sorted(port_results.iteritems()):
                line_dict[test_name].extend(self._create_lines(test_name, port_names, result))
        return line_dict

    def _create_lines(self, test_name, port_names, result):
        """Constructs test expectation line strings.

        Args:
            test_name: The test name string.
            port_names: A list of full port names that the line should apply to.
            result: A SimpleTestResult.

        Returns:
            A list of strings which each is a line of test expectation for given
            |test_name|.
        """
        lines = []
        port_names = self.tuple_or_value_to_list(port_names)

        # The set of ports with no results is assumed to have have no
        # overlap with the set of port names passed in here.
        assert (set(port_names) & self.ports_with_no_results) == set()

        # The ports with no results are generally ports of builders that
        # failed, maybe for unrelated reasons. At this point, we add ports
        # with no results to the list of platforms because we're guessing
        # that this new expectation might be cross-platform and should
        # also apply to any ports that we weren't able to get results for.
        port_names.extend(self.ports_with_no_results)

        expectations = '[ %s ]' % ' '.join(self.get_expectations(result, test_name))
        for specifier in self.normalized_specifiers(test_name, port_names):
            line_parts = []
            if specifier:
                line_parts.append('[ %s ]' % specifier)
            line_parts.append(test_name)
            line_parts.append(expectations)

            # Only add the bug link if the expectations do not include WontFix.
            if 'WontFix' not in expectations:
                line_parts.insert(0, result.bug)

            lines.append(' '.join(line_parts))
        return lines

    def normalized_specifiers(self, test_name, port_names):
        """Converts and simplifies ports into platform specifiers.

        Args:
            test_name: The test name string.
            port_names: A list of full port names that the line should apply to.

        Returns:
            A list of specifier string, e.g. ["Mac", "Win"].
            [''] will be returned if the line should apply to all platforms.
        """
        specifiers = []
        for name in sorted(port_names):
            specifiers.append(self.host.builders.version_specifier_for_port_name(name))

        if self.specifiers_can_extend_to_all_platforms(specifiers, test_name):
            return ['']

        specifiers = self.simplify_specifiers(specifiers, self.port.configuration_specifier_macros())
        if not specifiers:
            return ['']
        return specifiers

    @staticmethod
    def tuple_or_value_to_list(tuple_or_value):
        """Converts a tuple to a list, and a string value to a one-item list."""
        if isinstance(tuple_or_value, tuple):
            return list(tuple_or_value)
        return [tuple_or_value]

    def specifiers_can_extend_to_all_platforms(self, specifiers, test_name):
        """Tests whether a list of specifiers can be extended to all platforms.

        Tries to add skipped platform specifiers to the list and tests if the
        extended list covers all platforms.
        """
        extended_specifiers = specifiers + self.skipped_specifiers(test_name)
        # If the list is simplified to empty, then all platforms are covered.
        return not self.simplify_specifiers(extended_specifiers, self.port.configuration_specifier_macros())

    def skipped_specifiers(self, test_name):
        """Returns a list of platform specifiers for which the test is skipped."""
        specifiers = []
        for port in self.all_try_builder_ports():
            if port.skips_test(test_name):
                specifiers.append(self.host.builders.version_specifier_for_port_name(port.name()))
        return specifiers

    @memoized
    def all_try_builder_ports(self):
        """Returns a list of Port objects for all try builders."""
        return [self.host.port_factory.get_from_builder_name(name) for name in self._get_try_bots()]

    def simplify_specifiers(self, specifiers, specifier_macros):
        """Simplifies the specifier part of an expectation line if possible.

        "Simplifying" means finding the shortest list of platform specifiers
        that is equivalent to the given list of specifiers. This can be done
        because there are "macro specifiers" that stand in for multiple version
        specifiers, and an empty list stands in for "all platforms".

        Args:
            specifiers: A collection of specifiers (case insensitive).
            specifier_macros: A dict mapping "macros" for groups of specifiers
                to lists of version specifiers. e.g. {"win": ["win7", "win10"]}.
                If there are versions in this dict for that have no corresponding
                try bots, they are ignored.

        Returns:
            A shortened list of specifiers (capitalized). For example, ["win7",
            "win10"] would be converted to ["Win"]. If the given list covers
            all supported platforms, then an empty list is returned.
        """
        specifiers = {s.lower() for s in specifiers}
        covered_by_try_bots = self._platform_specifiers_covered_by_try_bots()
        for macro, versions in specifier_macros.iteritems():
            macro = macro.lower()

            # Only consider version specifiers that have corresponding try bots.
            versions = {s.lower() for s in versions if s.lower() in covered_by_try_bots}
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
                self.host.builders.platform_specifier_for_builder(builder_name).lower())
        return frozenset(all_platform_specifiers)

    def write_to_test_expectations(self, line_dict):
        """Writes the given lines to the TestExpectations file.

        The place in the file where the new lines are inserted is after a marker
        comment line. If this marker comment line is not found, then everything
        including the marker line is appended to the end of the file.

        All WontFix tests are inserted to NeverFixTests file instead of TextExpectations
        file.

        Args:
            line_dict: A dictionary from test names to a list of test expectation lines.
        """
        if not line_dict:
            _log.info(
                'No lines to write to TestExpectations, WebdriverExpectations or NeverFixTests.')
            return

        line_list = []
        wont_fix_list = []
        webdriver_list = []
        for lines in line_dict.itervalues():
            for line in lines:
                if 'Skip' in line and '-manual.' in line:
                    wont_fix_list.append(line)
                elif self.finder.webdriver_prefix() in line:
                    webdriver_list.append(line)
                else:
                    line_list.append(line)

        list_to_expectation = {
            self.port.path_to_generic_test_expectations_file(): line_list,
            self.port.path_to_webdriver_expectations_file(): webdriver_list
        }
        for expectations_file_path, lines in list_to_expectation.iteritems():
            if not lines:
                continue

            _log.info('Lines to write to %s:\n %s', expectations_file_path, '\n'.join(lines))
            # Writes to TestExpectations file.
            file_contents = self.host.filesystem.read_text_file(expectations_file_path)

            marker_comment_index = file_contents.find(MARKER_COMMENT)
            if marker_comment_index == -1:
                file_contents += '\n%s\n' % MARKER_COMMENT
                file_contents += '\n'.join(lines)
            else:
                end_of_marker_line = (file_contents[marker_comment_index:].find('\n')) + marker_comment_index
                file_contents = file_contents[:end_of_marker_line + 1] + '\n'.join(lines) + file_contents[end_of_marker_line:]

            self.host.filesystem.write_text_file(expectations_file_path, file_contents)

        if wont_fix_list:
            _log.info('Lines to write to NeverFixTests:\n %s', '\n'.join(wont_fix_list))
            # Writes to NeverFixTests file.
            wont_fix_path = self.port.path_to_never_fix_tests_file()
            wont_fix_file_content = self.host.filesystem.read_text_file(wont_fix_path)
            if not wont_fix_file_content.endswith('\n'):
                wont_fix_file_content += '\n'
            wont_fix_file_content += '\n'.join(wont_fix_list)
            wont_fix_file_content += '\n'
            self.host.filesystem.write_text_file(wont_fix_path, wont_fix_file_content)

    # TODO(robertma): Unit test this method.
    def download_text_baselines(self, test_results):
        """Fetches new baseline files for tests that should be rebaselined.

        Invokes `blink_tool.py rebaseline-cl` in order to download new baselines
        (-expected.txt files) for testharness.js tests that did not crash or
        time out. Then, the platform-specific test is removed from the overall
        failure test dictionary and the resulting dictionary is returned.

        Args:
            test_results: A dictionary of failing test results, mapping test
                names to lists of platforms to SimpleTestResult.

        Returns:
            A pair: A set of tests that are rebaselined, and a modified copy of
            the test_results dictionary containing only tests that couldn't be
            rebaselined.
        """
        tests_to_rebaseline, test_results = self.get_tests_to_rebaseline(test_results)
        if not tests_to_rebaseline:
            _log.info('No tests to rebaseline.')
            return tests_to_rebaseline, test_results
        _log.info('Tests to rebaseline:')
        for test in tests_to_rebaseline:
            _log.info('  %s', test)

        blink_tool = self.finder.path_from_blink_tools('blink_tool.py')
        command = [
            'python',
            blink_tool,
            'rebaseline-cl',
            '--verbose',
            '--no-trigger-jobs',
            '--fill-missing',
        ]
        if self.patchset:
            command.append('--patchset=' + str(self.patchset))
        command += tests_to_rebaseline
        self.host.executive.run_command(command)
        return tests_to_rebaseline, test_results

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
            for platforms, result in test_results[test_name].iteritems():
                if self.can_rebaseline(test_name, result):
                    del new_test_results[test_name][platforms]
                    tests_to_rebaseline.add(test_name)
        return sorted(tests_to_rebaseline), new_test_results

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

    def _get_try_bots(self):
        return self.host.builders.all_try_builder_names()
