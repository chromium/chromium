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

import sys
import unittest

from blinkpy.web_tests.layout_package import bot_test_expectations
from blinkpy.web_tests.builder_list import BuilderList


class BotTestExpectationsFactoryTest(unittest.TestCase):

    # pylint: disable=protected-access

    def fake_builder_list(self):
        return BuilderList({
            'Dummy builder name': {
                'master': 'dummy.master',
                'port_name': 'dummy-port',
                'specifiers': ['dummy', 'release'],
            },
        })

    def fake_results_json_for_builder(self, builder):
        return bot_test_expectations.ResultsJSON(builder, 'Dummy content')

    def test_results_url_for_builder(self):
        factory = bot_test_expectations.BotTestExpectationsFactory(self.fake_builder_list())

        self.assertEqual(factory._results_url_for_builder('Dummy builder name'),
                         'https://test-results.appspot.com/testfile?testtype=webkit_layout_tests'
                         '&name=results-small.json&master=dummy.master&builder=Dummy%20builder%20name')

    def test_expectations_for_builder(self):
        factory = bot_test_expectations.BotTestExpectationsFactory(self.fake_builder_list())
        factory._results_json_for_builder = self.fake_results_json_for_builder

        self.assertIsNotNone(factory.expectations_for_builder('Dummy builder name'))

    def test_expectations_for_port(self):
        factory = bot_test_expectations.BotTestExpectationsFactory(self.fake_builder_list())
        factory._results_json_for_builder = self.fake_results_json_for_builder

        self.assertIsNotNone(factory.expectations_for_port('dummy-port'))


@unittest.skipIf(sys.platform == 'win32', 'fails on Windows')
class BotTestExpectationsTest(unittest.TestCase):
    # FIXME: Find a way to import this map from Tools/TestResultServer/model/jsonresults.py.
    FAILURE_MAP = {'C': 'CRASH', 'F': 'FAIL', 'N': 'NO DATA', 'P': 'PASS', 'T': 'TIMEOUT',
                   'Y': 'NOTRUN', 'X': 'SKIP'}

    # All result_string's in this file represent retries from a single run.
    # The left-most entry is the first try, the right-most is the last.

    def _assert_is_flaky(self, results_string, should_be_flaky, only_ignore_very_flaky, expected=None):
        results_json = self._results_json_from_test_data({})
        expectations = bot_test_expectations.BotTestExpectations(results_json, BuilderList({}), set('test'))

        results_entry = self._results_from_string(results_string)
        if expected:
            results_entry[bot_test_expectations.ResultsJSON.EXPECTATIONS_KEY] = expected

        num_actual_results = len(expectations._flaky_types_in_results(  # pylint: disable=protected-access
            results_entry, only_ignore_very_flaky))
        if should_be_flaky:
            self.assertGreater(num_actual_results, 1)
        else:
            self.assertLessEqual(num_actual_results, 1)

    def test_basic_flaky(self):
        self._assert_is_flaky('P', should_be_flaky=False, only_ignore_very_flaky=False)
        self._assert_is_flaky('P', should_be_flaky=False, only_ignore_very_flaky=True)
        self._assert_is_flaky('F', should_be_flaky=False, only_ignore_very_flaky=False)
        self._assert_is_flaky('F', should_be_flaky=False, only_ignore_very_flaky=True)
        self._assert_is_flaky('FP', should_be_flaky=True, only_ignore_very_flaky=False)
        self._assert_is_flaky('FP', should_be_flaky=False, only_ignore_very_flaky=True)
        self._assert_is_flaky('FFP', should_be_flaky=True, only_ignore_very_flaky=False)
        self._assert_is_flaky('FFP', should_be_flaky=True, only_ignore_very_flaky=True)
        self._assert_is_flaky('FFT', should_be_flaky=True, only_ignore_very_flaky=False)
        self._assert_is_flaky('FFT', should_be_flaky=True, only_ignore_very_flaky=True)
        self._assert_is_flaky('FFF', should_be_flaky=False, only_ignore_very_flaky=False)
        self._assert_is_flaky('FFF', should_be_flaky=False, only_ignore_very_flaky=True)

        self._assert_is_flaky('FT', should_be_flaky=True, only_ignore_very_flaky=False, expected='TIMEOUT')
        self._assert_is_flaky('FT', should_be_flaky=False, only_ignore_very_flaky=True, expected='TIMEOUT')
        self._assert_is_flaky('FFT', should_be_flaky=True, only_ignore_very_flaky=False, expected='TIMEOUT')
        self._assert_is_flaky('FFT', should_be_flaky=True, only_ignore_very_flaky=True, expected='TIMEOUT')

    def _results_json_from_test_data(self, test_data):
        test_data[bot_test_expectations.ResultsJSON.FAILURE_MAP_KEY] = self.FAILURE_MAP
        json_dict = {
            'builder': test_data,
        }
        return bot_test_expectations.ResultsJSON('builder', json_dict)

    def _results_from_string(self, results_string):
        return {'results': [[1, results_string]]}

    def _assert_expectations(self, test_data, expectations_string, only_ignore_very_flaky):
        results_json = self._results_json_from_test_data(test_data)
        expectations = bot_test_expectations.BotTestExpectations(results_json, BuilderList({}), set('test'))
        self.assertEqual(expectations.flakes_by_path(only_ignore_very_flaky), expectations_string)

    def _assert_unexpected_results(self, test_data, expectations_string):
        results_json = self._results_json_from_test_data(test_data)
        expectations = bot_test_expectations.BotTestExpectations(results_json, BuilderList({}), set('test'))
        self.assertEqual(expectations.unexpected_results_by_path(), expectations_string)

    def test_all_results_by_path(self):
        test_data = {
            'tests': {
                'foo': {
                    'multiple_pass.html': {'results': [[4, 'P'], [1, 'P'], [2, 'P']]},
                    'fail.html': {'results': [[2, 'F']]},
                    'all_types.html': {
                        'results': [[1, 'C'], [2, 'F'],[1, 'N'], [1, 'P'], [1, 'T'],
                                    [1, 'Y'], [10, 'X']]
                    },
                    'not_run.html': {'results': []},
                }
            }
        }

        results_json = self._results_json_from_test_data(test_data)
        expectations = bot_test_expectations.BotTestExpectations(results_json, BuilderList({}), set('test'))
        results_by_path = expectations.all_results_by_path()

        expected_output = {
            'foo/multiple_pass.html': ['PASS'],
            'foo/fail.html': ['FAIL'],
            'foo/all_types.html': ['CRASH', 'FAIL', 'PASS', 'TIMEOUT']
        }

        self.assertEqual(results_by_path, expected_output)

    def test_basic(self):
        test_data = {
            'tests': {
                'foo': {
                    'veryflaky.html': self._results_from_string('FFP'),
                    'maybeflaky.html': self._results_from_string('FP'),
                    'notflakypass.html': self._results_from_string('P'),
                    'notflakyfail.html': self._results_from_string('F'),
                    # Even if there are no expected results, it's not very flaky if it didn't do multiple retries.
                    # This accounts for the latest expectations not necessarily matching the expectations
                    # at the time of the given run.
                    'notverflakynoexpected.html': self._results_from_string('FT'),
                    # If the test is flaky, but marked as such, it shouldn't get printed out.
                    'notflakyexpected.html': {'results': [[2, 'FFFP']], 'expected': 'PASS FAIL'},
                }
            }
        }
        self._assert_expectations(test_data, {
            'foo/veryflaky.html': sorted(['FAIL', 'PASS']),
        }, only_ignore_very_flaky=True)

        self._assert_expectations(test_data, {
            'foo/veryflaky.html': sorted(['FAIL', 'PASS']),
            'foo/notverflakynoexpected.html': ['FAIL', 'TIMEOUT'],
            'foo/maybeflaky.html': sorted(['FAIL', 'PASS']),
        }, only_ignore_very_flaky=False)

    def test_unexpected_results_no_unexpected(self):
        test_data = {
            'tests': {
                'foo': {
                    'pass1.html': {'results': [[4, 'P']]},
                    'pass2.html': {'results': [[2, 'F']], 'expected': 'PASS FAIL'},
                    'fail.html': {'results': [[2, 'P'], [1, 'F']], 'expected': 'PASS FAIL'},
                    'not_run.html': {'results': []},
                    'crash.html': {'results': [[2, 'F'], [1, 'C']], 'expected': 'CRASH FAIL SKIP'},
                }
            }
        }
        self._assert_unexpected_results(test_data, {})

    def test_unexpected_results_all_unexpected(self):
        test_data = {
            'tests': {
                'foo': {
                    'pass1.html': {'results': [[4, 'P']], 'expected': 'FAIL'},
                    'pass2.html': {'results': [[2, 'P']], 'expected': 'FAIL'},
                    'fail.html': {'results': [[4, 'F']]},
                    'f_p.html': {'results': [[1, 'F'], [2, 'P']]},
                    'crash.html': {'results': [[2, 'F'], [1, 'C']], 'expected': 'SKIP'},
                    'image.html': {'results': [[3, 'F']], 'expected': 'CRASH FAIL'},
                    'i_f.html': {'results': [[6, 'F']], 'expected': 'PASS'},
                    'all.html': self._results_from_string('FPFPCNCNTXTXFFFFFCFCYY'),
                }
            }
        }
        self._assert_unexpected_results(test_data, {
            'foo/pass1.html': sorted(['FAIL', 'PASS']),
            'foo/pass2.html': sorted(['FAIL', 'PASS']),
            'foo/fail.html': sorted(['FAIL', 'PASS']),
            'foo/f_p.html': sorted(['FAIL', 'PASS']),
            'foo/crash.html': sorted(['SKIP', 'CRASH', 'FAIL']),
            'foo/i_f.html': sorted(['FAIL', 'PASS']),
            'foo/all.html': sorted(['PASS', 'FAIL', 'TIMEOUT', 'CRASH']),
        })
