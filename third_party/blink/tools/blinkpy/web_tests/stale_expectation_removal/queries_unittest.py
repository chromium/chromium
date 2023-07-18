#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import unittest
import six

if six.PY3:
    import unittest.mock as mock

from blinkpy.web_tests.stale_expectation_removal import constants
from blinkpy.web_tests.stale_expectation_removal import data_types
from blinkpy.web_tests.stale_expectation_removal import queries
from blinkpy.web_tests.stale_expectation_removal import unittest_utils as wt_uu
from unexpected_passes_common import constants as common_constants
from unexpected_passes_common import data_types as common_data_types
from unexpected_passes_common import expectations as common_expectations
from unexpected_passes_common import unittest_utils as common_uu


class ConvertJsonResultToResultObjectUnittest(unittest.TestCase):
    def setUp(self) -> None:
        common_data_types.SetResultImplementation(data_types.WebTestResult)
        common_expectations.ClearInstance()
        common_uu.RegisterGenericExpectationsImplementation()

    def tearDown(self) -> None:
        common_data_types.SetResultImplementation(common_data_types.BaseResult)

    def testDurationIsSet(self) -> None:
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
        self.assertEqual(result._duration, datetime.timedelta(seconds=10))


class GetRelevantExpectationFilesForQueryResultUnittest(unittest.TestCase):
    def testNoFiles(self) -> None:
        """Tests that no reported expectation files are handled properly."""
        query_result = {}
        querier = wt_uu.CreateGenericWebTestQuerier()
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(query_result),
            [])

    def testAbsolutePath(self) -> None:
        """Tests that absolute paths are ignored."""
        query_result = {
            'expectation_files': ['/posix/path', '/c:/windows/path']
        }
        querier = wt_uu.CreateGenericWebTestQuerier()
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(query_result),
            [])

    def testRelativePath(self) -> None:
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
    def setUp(self) -> None:
        self._query_patcher = mock.patch(
            'blinkpy.web_tests.stale_expectation_removal.queries.'
            'WebTestBigQueryQuerier._RunBigQueryCommandsForJsonOutput')
        self._query_mock = self._query_patcher.start()
        self.addCleanup(self._query_patcher.stop)

    def testNoLargeQueryMode(self) -> None:
        """Tests that the expected clause is returned in normal mode."""
        querier = wt_uu.CreateGenericWebTestQuerier()
        query_generator = querier._GetQueryGeneratorForBuilder(
            common_data_types.BuilderEntry('builder',
                                           common_constants.BuilderTypes.CI,
                                           False))
        self.assertIsNotNone(query_generator)
        self.assertEqual(len(query_generator.GetClauses()), 1)
        self.assertEqual(query_generator.GetClauses()[0], '')
        self.assertIsInstance(query_generator,
                              queries.WebTestFixedQueryGenerator)
        self._query_mock.assert_not_called()
        # Make sure that there aren't any issues with getting the queries.
        q = query_generator.GetQueries()
        self.assertEqual(len(q), 1)

    def testLargeQueryModeNoTests(self) -> None:
        """Tests that a special value is returned if no tests are found."""
        querier = wt_uu.CreateGenericWebTestQuerier(large_query_mode=True)
        self._query_mock.return_value = []
        query_generator = querier._GetQueryGeneratorForBuilder(
            common_data_types.BuilderEntry('builder',
                                           common_constants.BuilderTypes.CI,
                                           False))
        self.assertIsNone(query_generator)
        self._query_mock.assert_called_once()

    def testLargeQueryModeFoundTests(self) -> None:
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
        query_generator = querier._GetQueryGeneratorForBuilder(
            common_data_types.BuilderEntry('builder',
                                           common_constants.BuilderTypes.CI,
                                           False))
        self.assertIsNotNone(query_generator)
        self.assertEqual(query_generator.GetClauses(),
                         ['AND test_id IN UNNEST(["foo_test", "bar_test"])'])
        self.assertIsInstance(query_generator,
                              queries.WebTestSplitQueryGenerator)
        # Make sure that there aren't any issues with getting the queries.
        q = query_generator.GetQueries()
        self.assertEqual(len(q), 1)


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class GetActiveBuilderQueryUnittest(unittest.TestCase):
    def setUp(self) -> None:
        self.querier = wt_uu.CreateGenericWebTestQuerier()

    def testPublicCi(self):
        """Tests that the active query for public CI is as expected."""
        expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)

  )
SELECT DISTINCT builder_name
FROM builders
"""
        self.assertEqual(
            self.querier._GetActiveBuilderQuery(
                common_constants.BuilderTypes.CI, False), expected_query)

    def testInternalCi(self) -> None:
        """Tests that the active query for internal CI is as expected."""
        expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
    UNION ALL
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chrome.blink_web_tests_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
  )
SELECT DISTINCT builder_name
FROM builders
"""
        self.assertEqual(
            self.querier._GetActiveBuilderQuery(
                common_constants.BuilderTypes.CI, True), expected_query)

    def testPublicTry(self) -> None:
        """Tests that the active query for public try is as expected."""
        expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)

  )
SELECT DISTINCT builder_name
FROM builders
"""
        self.assertEqual(
            self.querier._GetActiveBuilderQuery(
                common_constants.BuilderTypes.TRY, False), expected_query)

    def testInternalTry(self) -> None:
        """Tests that the active query for internal try is as expected."""
        expected_query = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
    UNION ALL
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chrome.blink_web_tests_try_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
  )
SELECT DISTINCT builder_name
FROM builders
"""
        self.assertEqual(
            self.querier._GetActiveBuilderQuery(
                common_constants.BuilderTypes.TRY, True), expected_query)


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class GeneratedQueryUnittest(unittest.TestCase):
    maxDiff = None

    def testPublicCi(self) -> None:
        """Tests that the generated public CI query is as expected."""
        expected_query = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "chromium:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      duration,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_base_timeout") as timeout,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_used_expectations_file") as expectation_files
    FROM
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
"""
        self.assertEqual(
            queries.CI_BQ_QUERY_TEMPLATE.format(builder_project='chromium',
                                                test_filter_clause='tfc'),
            expected_query)

    def testInternalCi(self) -> None:
        """Tests that the generated internal CI query is as expected."""
        expected_query = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chrome.blink_web_tests_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "chrome:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      duration,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_base_timeout") as timeout,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_used_expectations_file") as expectation_files
    FROM
      `chrome-luci-data.chrome.blink_web_tests_ci_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
"""
        self.assertEqual(
            queries.CI_BQ_QUERY_TEMPLATE.format(builder_project='chrome',
                                                test_filter_clause='tfc'),
            expected_query)

    def testPublicTry(self) -> None:
        """Tests that the generated public try query is as expected."""
        expected_query = """\
WITH
  submitted_builds AS (
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.chromium.attempts`,
      UNNEST(builds) as unnested_builds,
      UNNEST(gerrit_changes) as unnested_changes
    WHERE
      unnested_builds.host = "cr-buildbucket.appspot.com"
      AND unnested_changes.submit_status = "SUCCESS"
      AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                     INTERVAL 30 DAY)
  ),
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr,
      submitted_builds sb
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "chromium:try"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
      AND exported.id = sb.id
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      duration,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_base_timeout") as timeout,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_used_expectations_file") as expectation_files
    FROM
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
"""
        self.assertEqual(
            queries.TRY_BQ_QUERY_TEMPLATE.format(builder_project='chromium',
                                                 test_filter_clause='tfc'),
            expected_query)

    def testInternalTry(self) -> None:
        """Tests that the generated internal try query is as expected."""
        expected_query = """\
WITH
  submitted_builds AS (
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.chromium.attempts`,
      UNNEST(builds) as unnested_builds,
      UNNEST(gerrit_changes) as unnested_changes
    WHERE
      unnested_builds.host = "cr-buildbucket.appspot.com"
      AND unnested_changes.submit_status = "SUCCESS"
      AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                     INTERVAL 30 DAY)
  ),
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chrome.blink_web_tests_try_test_results` tr,
      submitted_builds sb
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "chrome:try"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
      AND exported.id = sb.id
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      duration,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_base_timeout") as timeout,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "web_tests_used_expectations_file") as expectation_files
    FROM
      `chrome-luci-data.chrome.blink_web_tests_try_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
"""
        self.assertEqual(
            queries.TRY_BQ_QUERY_TEMPLATE.format(builder_project='chrome',
                                                 test_filter_clause='tfc'),
            expected_query)


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class QueryGeneratorImplUnittest(unittest.TestCase):
    def testPublicCi(self) -> None:
        """Tests that public CI builders use the correct query."""
        q = queries.QueryGeneratorImpl(['tfc'],
                                       common_data_types.BuilderEntry(
                                           'builder',
                                           common_constants.BuilderTypes.CI,
                                           False))
        self.assertEqual(len(q), 1)
        expected_query = queries.CI_BQ_QUERY_TEMPLATE.format(
            builder_project='chromium', test_filter_clause='tfc')
        self.assertEqual(q[0], expected_query)

    def testInternalCi(self) -> None:
        """Tests that internal CI builders use the correct query."""
        q = queries.QueryGeneratorImpl(['tfc'],
                                       common_data_types.BuilderEntry(
                                           'builder',
                                           common_constants.BuilderTypes.CI,
                                           True))
        self.assertEqual(len(q), 1)
        expected_query = queries.CI_BQ_QUERY_TEMPLATE.format(
            builder_project='chrome', test_filter_clause='tfc')
        self.assertEqual(q[0], expected_query)

    def testPublicTry(self) -> None:
        """Tests that public try builders use the correct query."""
        q = queries.QueryGeneratorImpl(['tfc'],
                                       common_data_types.BuilderEntry(
                                           'builder',
                                           common_constants.BuilderTypes.TRY,
                                           False))
        self.assertEqual(len(q), 1)
        expected_query = queries.TRY_BQ_QUERY_TEMPLATE.format(
            builder_project='chromium', test_filter_clause='tfc')
        self.assertEqual(q[0], expected_query)

    def testInternalTry(self) -> None:
        """Tests that internal try builders use the correct query."""
        q = queries.QueryGeneratorImpl(['tfc'],
                                       common_data_types.BuilderEntry(
                                           'builder',
                                           common_constants.BuilderTypes.TRY,
                                           True))
        self.assertEqual(len(q), 1)
        expected_query = queries.TRY_BQ_QUERY_TEMPLATE.format(
            builder_project='chrome', test_filter_clause='tfc')
        self.assertEqual(q[0], expected_query)

    def testUnknownBuilderType(self) -> None:
        """Tests that an exception is raised for unknown builder types."""
        with self.assertRaises(RuntimeError):
            queries.QueryGeneratorImpl(['tfc'],
                                       common_data_types.BuilderEntry(
                                           'unknown_builder', 'unknown_type',
                                           False))


class StripPrefixFromTestIdUnittest(unittest.TestCase):
    def testUnknownPrefix(self) -> None:
        """Tests that an error is raised if an unknown prefix is found."""
        querier = wt_uu.CreateGenericWebTestQuerier()
        with self.assertRaises(RuntimeError):
            querier._StripPrefixFromTestId('foobar')

    def testKnownPrefixes(self) -> None:
        """Tests that all known prefixes are properly stripped."""
        querier = wt_uu.CreateGenericWebTestQuerier()
        test_ids = [prefix + 'a' for prefix in queries.KNOWN_TEST_ID_PREFIXES]
        for t in test_ids:
            stripped = querier._StripPrefixFromTestId(t)
            self.assertEqual(stripped, 'a')


if __name__ == '__main__':
    unittest.main(verbosity=2)
