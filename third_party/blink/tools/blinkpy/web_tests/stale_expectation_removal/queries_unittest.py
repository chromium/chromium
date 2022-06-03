#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest
import six

if six.PY3:
    import unittest.mock as mock

from blinkpy.web_tests.stale_expectation_removal import constants
from blinkpy.web_tests.stale_expectation_removal import data_types
from blinkpy.web_tests.stale_expectation_removal import queries
from blinkpy.web_tests.stale_expectation_removal import unittest_utils as wt_uu
from unexpected_passes_common import data_types as common_data_types


class ConvertJsonResultToResultObjectUnittest(unittest.TestCase):
    def setUp(self):
        common_data_types.SetResultImplementation(data_types.WebTestResult)

    def tearDown(self):
        common_data_types.SetResultImplementation(common_data_types.BaseResult)

    def testDurationIsSet(self):
        """Tests that the duration is set appropriately on the result."""
        json_result = {
            'id': 'build-1234',
            'test_id': 'ninja://:blink_web_tests/test',
            'status': 'PASS',
            'typ_tags': ['debug'],
            'step_name': 'step_name',
            'duration': '10',
            'timeout': '3',
        }
        querier = wt_uu.CreateGenericWebTestQuerier()
        result = querier._ConvertJsonResultToResultObject(json_result)
        self.assertTrue(result.is_slow_result)
        self.assertEqual(result._duration, 10)


class GetRelevantExpectationFilesForQueryResultUnittest(unittest.TestCase):
    def testNoFiles(self):
        """Tests that no reported expectation files are handled properly."""
        query_result = {}
        querier = wt_uu.CreateGenericWebTestQuerier()
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(query_result),
            [])

    def testAbsolutePath(self):
        """Tests that absolute paths are ignored."""
        query_result = {
            'expectation_files': ['/posix/path', '/c:/windows/path']
        }
        querier = wt_uu.CreateGenericWebTestQuerier()
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(query_result),
            [])

    def testRelativePath(self):
        """Tests that relative paths are properly reconstructed."""
        query_result = {
            'expectation_files':
            ['TestExpectations', 'flag-specific/someflag']
        }
        querier = wt_uu.CreateGenericWebTestQuerier()
        expected_files = [
            os.path.join(constants.WEB_TEST_ROOT_DIR, 'TestExpectations'),
            os.path.join(constants.WEB_TEST_ROOT_DIR, 'flag-specific',
                         'someflag'),
        ]
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(query_result),
            expected_files)


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class GetQueryGeneratorForBuilderUnittest(unittest.TestCase):
    def setUp(self):
        self._query_patcher = mock.patch(
            'blinkpy.web_tests.stale_expectation_removal.queries.'
            'WebTestBigQueryQuerier._RunBigQueryCommandsForJsonOutput')
        self._query_mock = self._query_patcher.start()
        self.addCleanup(self._query_patcher.stop)

    def testNoLargeQueryMode(self):
        """Tests that the expected clause is returned in normal mode."""
        querier = wt_uu.CreateGenericWebTestQuerier()
        query_generator = querier._GetQueryGeneratorForBuilder('', '')
        self.assertEqual(len(query_generator.GetClauses()), 1)
        self.assertEqual(query_generator.GetClauses()[0], '')
        self.assertIsInstance(query_generator,
                              queries.WebTestFixedQueryGenerator)
        self._query_mock.assert_not_called()

    def testLargeQueryModeNoTests(self):
        """Tests that a special value is returned if no tests are found."""
        querier = wt_uu.CreateGenericWebTestQuerier(large_query_mode=True)
        self._query_mock.return_value = []
        query_generator = querier._GetQueryGeneratorForBuilder('', '')
        self.assertIsNone(query_generator)
        self._query_mock.assert_called_once()

    def testLargeQueryModeFoundTests(self):
        """Tests that a clause containing found tests is returned."""
        querier = wt_uu.CreateGenericWebTestQuerier(large_query_mode=True)
        self._query_mock.return_value = [
            {
                'test_id': 'foo_test',
            },
            {
                'test_id': 'bar_test',
            },
        ]
        query_generator = querier._GetQueryGeneratorForBuilder('', '')
        self.assertEqual(query_generator.GetClauses(),
                         ['AND test_id IN UNNEST(["foo_test", "bar_test"])'])
        self.assertIsInstance(query_generator,
                              queries.WebTestSplitQueryGenerator)


class StripPrefixFromTestIdUnittest(unittest.TestCase):
    def testUnknownPrefix(self):
        """Tests that an error is raised if an unknown prefix is found."""
        querier = wt_uu.CreateGenericWebTestQuerier()
        with self.assertRaises(RuntimeError):
            querier._StripPrefixFromTestId('foobar')

    def testKnownPrefixes(self):
        """Tests that all known prefixes are properly stripped."""
        querier = wt_uu.CreateGenericWebTestQuerier()
        test_ids = [prefix + 'a' for prefix in queries.KNOWN_TEST_ID_PREFIXES]
        for t in test_ids:
            stripped = querier._StripPrefixFromTestId(t)
            self.assertEqual(stripped, 'a')


if __name__ == '__main__':
    unittest.main(verbosity=2)
