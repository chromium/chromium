# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import json
import unittest
import unittest.mock as mock

from blinkpy.web_tests.flake_suppressor import web_tests_expectations
from blinkpy.web_tests.flake_suppressor import web_tests_queries as queries
from blinkpy.web_tests.flake_suppressor import web_tests_tag_utils as tag_utils
from blinkpy.web_tests.flake_suppressor import web_tests_results as results_module
from flake_suppressor_common import unittest_utils as uu
from flake_suppressor_common import tag_utils as common_tag_utils


class WebTestQueriesUnittest(unittest.TestCase):
    def setUp(self) -> None:
        common_tag_utils.SetTagUtilsImplementation(tag_utils.WebTestsTagUtils)
        expectation_processor = (
            web_tests_expectations.WebTestsExpectationProcessor())
        result_processor = results_module.WebTestsResultProcessor(
            expectation_processor)
        self._querier_instance = queries.WebTestsBigQueryQuerier(
            1, 'project', result_processor)
        self._querier_instance._submitted_builds = set(
            ['build-1234', 'build-2345'])
        self._subprocess_patcher = mock.patch(
            'flake_suppressor_common.queries.subprocess.run')
        self._subprocess_mock = self._subprocess_patcher.start()
        self.addCleanup(self._subprocess_patcher.stop)

    def testIgnoredTags(self) -> None:
        """Tests that ignored tags are removed and their counts merged."""

        def SideEffect(*_, **kwargs) -> uu.FakeProcess:
            query = kwargs['input']
            if 'submitted_builds' in query:
                # Try results.
                query_result = [
                    {
                        'typ_tags': ['linux'],
                        'test_name': 'foo/bar/linux',
                        'result_count': '25',
                    },
                    {
                        'typ_tags': ['linux', 'x86'],
                        'test_name': 'foo/bar/linux',
                        'result_count': '50',
                    },
                ]
            else:
                # CI results.
                query_result = [{
                    'typ_tags': ['win', 'x86'],
                    'test_name': 'foo/bar/windows',
                    'result_count': '100',
                }, {
                    'typ_tags': ['win'],
                    'test_name': 'foo/bar/windows',
                    'result_count': '50',
                }, {
                    'typ_tags': ['mac'],
                    'test_name': 'foo/bar/mac',
                    'result_count': '200',
                }, {
                    'typ_tags': ['linux'],
                    'test_name': 'foo/bar/linux',
                    'result_count': '300',
                }]
            return uu.FakeProcess(stdout=json.dumps(query_result))

        self._subprocess_mock.side_effect = SideEffect
        result_counts = self._querier_instance.GetResultCounts()
        for rc, val in result_counts.items():
            print(rc)
            print(val)
        expected_result_counts = {
            tuple(['win']): {
                'foo/bar/windows': 150,
            },
            tuple(['mac']): {
                'foo/bar/mac': 200,
            },
            tuple(['linux']): {
                'foo/bar/linux': 375,
            },
        }
        self.assertEqual(result_counts, expected_result_counts)
        self.assertEqual(self._subprocess_mock.call_count, 2)
