# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
import unittest.mock as mock

from blinkpy.web_tests.fuzzy_diff_analyzer import queries
from flake_suppressor_common import unittest_utils as uu

QUERY_DATA = [{
    'id': 1,
    'name': 'foo/bar/windows',
    'typ_tags': ['win'],
    'typ_expectations': ['PASS'],
    'step_name': ['blink_wpt_tests on Windows-10'],
    'test_type': ['image', 'text'],
    'image_diff_max_difference': 10,
    'image_diff_total_pixels': 1,
}, {
    'id': 1,
    'name': 'foo/bar/mac',
    'typ_tags': ['mac'],
    'typ_expectations': ['FAILURE'],
    'step_name': ['blink_wpt_tests on MAC-12'],
    'test_type': ['image'],
    'image_diff_max_difference': 100,
    'image_diff_total_pixels': 10,
}]

QUERY_DATA_2 = [{
    'bug_id': 'chromium/1',
    'create_time': '2023-10-16 21:55:00.474-07:00',
    'test_ids': ['testA', 'testB'],
}, {
    'bug_id': 'chromium/2',
    'create_time': '2023-10-15 21:55:00.474-07:00',
    'test_ids': ['testC', 'testD', 'testE'],
}]


class FuzzyDiffAnalyzerQueriesUnittest(unittest.TestCase):
    def setUp(self) -> None:
        self._querier_instance = queries.FuzzyDiffAnalyzerQuerier(1, 'project')
        self._subprocess_patcher = mock.patch(
            'flake_suppressor_common.queries.subprocess.run')
        self._subprocess_mock = self._subprocess_patcher.start()
        self.addCleanup(self._subprocess_patcher.stop)

    def testGetFailedCIImageTests(self) -> None:
        """Tests that Fuzzy Diff Analyzer queries is sending the sql."""
        def side_effect(*_, **kwargs) -> uu.FakeProcess:
            query = kwargs['input']
            self.assertEqual(
                query,queries.CI_FAILED_IMAGE_COMPARISON_TEST_QUERY.format(
                    test_path_selector=''))
            query_result = QUERY_DATA
            return uu.FakeProcess(stdout=json.dumps(query_result))

        self._subprocess_mock.side_effect = side_effect
        result_query = \
            self._querier_instance.get_failed_image_comparison_ci_tests()
        self.assertEqual(result_query, QUERY_DATA)
        self.assertEqual(self._subprocess_mock.call_count, 1)

    def testGetWebTestFlakyBugs(self) -> None:
        """Tests that Fuzzy Diff Analyzer queries is sending the sql."""
        def side_effect(*_, **kwargs) -> uu.FakeProcess:
            query = kwargs['input']
            self.assertEqual(query, queries.WEB_TEST_FLAKY_BUGS_QUERY)
            query_result = QUERY_DATA_2
            return uu.FakeProcess(stdout=json.dumps(query_result))

        self._subprocess_mock.side_effect = side_effect
        result_query = \
            self._querier_instance.get_web_test_flaky_bugs()
        self.assertEqual(result_query, QUERY_DATA_2)
        self.assertEqual(self._subprocess_mock.call_count, 1)
