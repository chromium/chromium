# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the Google name nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Generates a fake TestExpectations file consisting of flaky tests from the bot
corresponding to the give port.
"""

import json
import logging
import os.path
import urllib
import urllib2

from blinkpy.web_tests.models.test_expectations import TestExpectations, PASS
from blinkpy.web_tests.models.test_expectations import TestExpectationLine


_log = logging.getLogger(__name__)


class ResultsJSON(object):
    """Contains the contents of a results.json file.

    results.json v4 format:
    {
      'version': 4,
      'builder name' : {
        'blinkRevision': [],
        'tests': {
          'directory' { # Each path component is a dictionary.
            'testname.html': {
              'expected' : 'FAIL', # Expectation name.
              'results': [], # Run-length encoded result.
              'times': [],
              'bugs': [], # Bug URLs.
            }
          }
        }
      }
      'buildNumbers': [],
      'secondsSinceEpoch': [],
      'chromeRevision': [],
      'failure_map': {}  # Map from letter code to expectation name.
    }
    """
    TESTS_KEY = 'tests'
    FAILURE_MAP_KEY = 'failure_map'
    RESULTS_KEY = 'results'
    EXPECTATIONS_KEY = 'expected'
    BUGS_KEY = 'bugs'
    RLE_LENGTH = 0
    RLE_VALUE = 1

    # results.json was originally designed to support
    # multiple builders in one json file, so the builder_name
    # is needed to figure out which builder this json file
    # refers to (and thus where the results are stored)
    def __init__(self, builder_name, json_dict):
        self.builder_name = builder_name
        self._json = json_dict

    def _walk_trie(self, trie, parent_path):
        for name, value in trie.items():
            full_path = os.path.join(parent_path, name)

            # FIXME: If we ever have a test directory self.RESULTS_KEY
            # ("results"), this logic will break!
            if self.RESULTS_KEY not in value:
                for path, results in self._walk_trie(value, full_path):
                    yield path, results
            else:
                yield full_path, value

    def walk_results(self, full_path=''):
        tests_trie = self._json[self.builder_name][self.TESTS_KEY]
        return self._walk_trie(tests_trie, parent_path='')

    def expectation_for_type(self, type_char):
        return self._json[self.builder_name][self.FAILURE_MAP_KEY][type_char]

    # Knowing how to parse the run-length-encoded values in results.json
    # is a detail of this class.
    def occurances_and_type_from_result_item(self, item):
        return item[self.RLE_LENGTH], item[self.RLE_VALUE]


class BotTestExpectationsFactory(object):
    RESULTS_URL_FORMAT = (
        'https://test-results.appspot.com/testfile?testtype=webkit_layout_tests'
        '&name=results-small.json&master=%s&builder=%s')

    def __init__(self, builders):
        self.builders = builders

    def _results_json_for_port(self, port_name, builder_category):
        builder = self.builders.builder_name_for_port_name(port_name)
        if not builder:
            return None
        return self._results_json_for_builder(builder)

    def _results_url_for_builder(self, builder):
        return self.RESULTS_URL_FORMAT % (
            urllib.quote(self.builders.master_for_builder(builder)), urllib.quote(builder))

    def _results_json_for_builder(self, builder):
        results_url = self._results_url_for_builder(builder)
        try:
            _log.debug('Fetching flakiness data from appengine: %s', results_url)
            return ResultsJSON(builder, json.load(urllib2.urlopen(results_url)))
        except urllib2.URLError as error:
            _log.warning('Could not retrieve flakiness data from the bot.  url: %s', results_url)
            _log.warning(error)

    def expectations_for_port(self, port_name, builder_category='layout'):
        # FIXME: This only grabs release builder's flakiness data. If we're running debug,
        # when we should grab the debug builder's data.
        # FIXME: What should this do if there is no debug builder for a port, e.g. we have
        # no debug XP builder? Should it use the release bot or another Windows debug bot?
        # At the very least, it should log an error.
        results_json = self._results_json_for_port(port_name, builder_category)
        if not results_json:
            return None
        return BotTestExpectations(results_json, self.builders)

    def expectations_for_builder(self, builder):
        results_json = self._results_json_for_builder(builder)
        if not results_json:
            return None
        return BotTestExpectations(results_json, self.builders)


class BotTestExpectations(object):
    # FIXME: Get this from the json instead of hard-coding it.
    RESULT_TYPES_TO_IGNORE = ['N', 'X', 'Y']  # NO_DATA, SKIP, NOTRUN

    # TODO(ojan): Remove this once crbug.com/514378 is fixed.
    # The JSON can contain results for expectations, not just actual result types.
    NON_RESULT_TYPES = ['S', 'X']  # SLOW, SKIP

    # specifiers arg is used in unittests to avoid the static dependency on builders.
    def __init__(self, results_json, builders, specifiers=None):
        self.results_json = results_json
        self.specifiers = specifiers or set(builders.specifiers_for_builder(results_json.builder_name))

    def _line_from_test_and_flaky_types(self, test_path, flaky_types):
        line = TestExpectationLine()
        line.original_string = test_path
        line.name = test_path
        line.filename = test_path
        line.path = test_path  # FIXME: Should this be normpath?
        line.matching_tests = [test_path]
        line.bugs = ['crbug.com/FILE_A_BUG_BEFORE_COMMITTING_THIS']
        line.expectations = sorted(flaky_types)
        line.specifiers = self.specifiers
        return line

    def flakes_by_path(self, only_ignore_very_flaky):
        """Sets test expectations to bot results if there are at least two distinct results."""
        flakes_by_path = {}
        for test_path, entry in self.results_json.walk_results():
            flaky_types = self._flaky_types_in_results(entry, only_ignore_very_flaky)
            if len(flaky_types) <= 1:
                continue
            flakes_by_path[test_path] = sorted(flaky_types)
        return flakes_by_path

    def unexpected_results_by_path(self):
        """For tests with unexpected results, returns original expectations + results."""
        def exp_to_string(exp):
            return TestExpectations.EXPECTATIONS_TO_STRING.get(exp, None)

        def string_to_exp(string):
            # Needs a bit more logic than the method above,
            # since a PASS is 0 and evaluates to False.
            result = TestExpectations.EXPECTATIONS.get(string.lower(), None)
            if not result is None:
                return result
            raise ValueError(string)

        unexpected_results_by_path = {}
        for test_path, entry in self.results_json.walk_results():
            # Expectations for this test. No expectation defaults to PASS.
            exp_string = entry.get(self.results_json.EXPECTATIONS_KEY, u'PASS')

            # All run-length-encoded results for this test.
            results_dict = entry.get(self.results_json.RESULTS_KEY, {})

            # Set of expectations for this test.
            expectations = set(map(string_to_exp, exp_string.split(' ')))

            # Set of distinct results for this test.
            result_types = self._all_types_in_results(results_dict)

            # Distinct results as non-encoded strings.
            result_strings = map(self.results_json.expectation_for_type, result_types)

            # Distinct resulting expectations.
            result_exp = map(string_to_exp, result_strings)

            expected = lambda e: TestExpectations.result_was_expected(e, expectations)

            additional_expectations = set(e for e in result_exp if not expected(e))

            # Test did not have unexpected results.
            if not additional_expectations:
                continue

            expectations.update(additional_expectations)
            unexpected_results_by_path[test_path] = sorted(map(exp_to_string, expectations))
        return unexpected_results_by_path

    def all_results_by_path(self):
        """Returns all seen result types for each test.

        Returns a dictionary from each test path that has a result to a list of distinct, sorted result
        strings. For example, if the test results are as follows:

            a.html IMAGE IMAGE PASS PASS PASS TIMEOUT PASS TEXT
            b.html PASS PASS PASS PASS PASS PASS PASS PASS
            c.html

        This method will return:
            {
                'a.html': ['IMAGE', 'TEXT', 'TIMEOUT', 'PASS'],
                'b.html': ['PASS'],
            }
        """
        results_by_path = {}
        for test_path, entry in self.results_json.walk_results():
            results_dict = entry.get(self.results_json.RESULTS_KEY, {})

            result_types = self._all_types_in_results(results_dict)

            if not result_types:
                continue

            # Distinct results as non-encoded strings.
            result_strings = map(self.results_json.expectation_for_type, result_types)

            results_by_path[test_path] = sorted(result_strings)
        return results_by_path

    def expectation_lines(self, only_ignore_very_flaky):
        lines = []
        for test_path, entry in self.results_json.walk_results():
            flaky_types = self._flaky_types_in_results(entry, only_ignore_very_flaky)
            if len(flaky_types) > 1:
                line = self._line_from_test_and_flaky_types(test_path, flaky_types)
                lines.append(line)
        return lines

    def _all_types_in_results(self, run_length_encoded_results):
        results = set()

        for result_item in run_length_encoded_results:
            _, result_types = self.results_json.occurances_and_type_from_result_item(result_item)

            for result_type in result_types:
                if result_type not in self.RESULT_TYPES_TO_IGNORE:
                    results.add(result_type)

        return results

    def _result_to_enum(self, result):
        return TestExpectations.EXPECTATIONS[result.lower()]

    def _flaky_types_in_results(self, results_entry, only_ignore_very_flaky):
        flaky_results = set()

        # Always include pass as an expected result. Passes will never turn the bot red.
        # This fixes cases where the expectations have an implicit Pass, e.g. [ Slow ].
        latest_expectations = [PASS]
        if self.results_json.EXPECTATIONS_KEY in results_entry:
            expectations_list = results_entry[self.results_json.EXPECTATIONS_KEY].split(' ')
            latest_expectations += [self._result_to_enum(expectation) for expectation in expectations_list]

        for result_item in results_entry[self.results_json.RESULTS_KEY]:
            _, result_types_str = self.results_json.occurances_and_type_from_result_item(result_item)

            result_types = []
            for result_type in result_types_str:
                # TODO(ojan): Remove this if-statement once crbug.com/514378 is fixed.
                if result_type not in self.NON_RESULT_TYPES:
                    result_types.append(self.results_json.expectation_for_type(result_type))

            # It didn't flake if it didn't retry.
            if len(result_types) <= 1:
                continue

            # If the test ran as expected after only one retry, it's not very flaky.
            # It's only very flaky if it failed the first run and the first retry
            # and then ran as expected in one of the subsequent retries.
            # If there are only two entries, then that means it failed on the first
            # try and ran as expected on the second because otherwise we'd have
            # a third entry from the next try.
            if only_ignore_very_flaky and len(result_types) == 2:
                continue

            has_unexpected_results = False
            for result_type in result_types:
                result_enum = self._result_to_enum(result_type)
                # TODO(ojan): We really should be grabbing the expected results from the time
                # of the run instead of looking at the latest expected results. That's a lot
                # more complicated though. So far we've been looking at the aggregated
                # results_small.json off test_results.appspot, which has all the information
                # for the last 100 runs. In order to do this, we'd need to look at the
                # individual runs' full_results.json, which would be slow and more complicated.
                # The only thing we lose by not fixing this is that a test that was flaky
                # and got fixed will still get printed out until 100 runs have passed.
                if not TestExpectations.result_was_expected(result_enum, latest_expectations):
                    has_unexpected_results = True
                    break

            if has_unexpected_results:
                flaky_results = flaky_results.union(set(result_types))

        return flaky_results
