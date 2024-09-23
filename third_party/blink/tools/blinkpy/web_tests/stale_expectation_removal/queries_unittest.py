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
from unexpected_passes_common import queries as common_queries
from unexpected_passes_common import unittest_utils as common_uu


class ConvertBigQueryRowToResultObjectUnittest(unittest.TestCase):

    def setUp(self) -> None:
        common_data_types.SetResultImplementation(data_types.WebTestResult)
        common_expectations.ClearInstance()
        common_uu.RegisterGenericExpectationsImplementation()

    def tearDown(self) -> None:
        common_data_types.SetResultImplementation(common_data_types.BaseResult)

    def testDurationIsSet(self) -> None:
        """Tests that the duration is set appropriately on the result."""
        row = wt_uu.FakeQueryResult(builder_name='builder_name',
                                    id_='build-1234',
                                    test_id='ninja://:blink_web_tests/test',
                                    status='PASS',
                                    typ_tags=['debug'],
                                    step_name='step_name',
                                    duration='10',
                                    timeout='3')
        querier = wt_uu.CreateGenericWebTestQuerier()
        result = querier._ConvertBigQueryRowToResultObject(row)
        self.assertTrue(result.is_slow_result)
        self.assertEqual(result._duration, datetime.timedelta(seconds=10))

class GetRelevantExpectationFilesForQueryResultUnittest(unittest.TestCase):
    def testNoFiles(self) -> None:
        """Tests that no reported expectation files are handled properly."""
        row = common_queries.QueryResult(data={})
        querier = wt_uu.CreateGenericWebTestQuerier()
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(row), [])

    def testAbsolutePath(self) -> None:
        """Tests that absolute paths are ignored."""
        row = common_queries.QueryResult(
            data={'expectation_files': ['/posix/path', '/c:/windows/path']})
        querier = wt_uu.CreateGenericWebTestQuerier()
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(row), [])

    def testRelativePath(self) -> None:
        """Tests that relative paths are properly reconstructed."""
        row = common_queries.QueryResult(data={
            'expectation_files':
            ['TestExpectations', 'flag-specific/someflag']
        })
        querier = wt_uu.CreateGenericWebTestQuerier()
        expected_files = [
            os.path.join(constants.WEB_TEST_ROOT_DIR, 'TestExpectations'),
            os.path.join(constants.WEB_TEST_ROOT_DIR, 'flag-specific',
                         'someflag'),
        ]
        self.assertEqual(
            querier._GetRelevantExpectationFilesForQueryResult(row),
            expected_files)


@unittest.skipIf(six.PY2, 'Script and unittest are Python 3-only')
class GeneratedQueryUnittest(unittest.TestCase):
    maxDiff = None

    def setUp(self):
        self._querier = wt_uu.CreateGenericWebTestQuerier(num_samples=15)

    def testPublicCi(self) -> None:
        """Tests that the generated public CI query is as expected."""
        expected_query = """\
WITH
  builds AS (
    WITH
      all_builds AS (
        SELECT
          DISTINCT exported.id AS build_inv_id,
          variant.*,
          partition_time
        FROM
          `chrome-luci-data.chromium.blink_web_tests_ci_test_results` AS tr,
          UNNEST(variant) AS variant
        WHERE
          DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
          AND exported.realm = "chromium:ci"
          AND key = "builder"
      ),
      grouped_builds AS (
        SELECT
          build_inv_id,
          value AS builder,
          partition_time,
          RANK() OVER (PARTITION BY value ORDER BY partition_time DESC) AS rank_idx,
        FROM all_builds
      )
    SELECT
      build_inv_id,
      builder,
      partition_time
    FROM grouped_builds
    WHERE rank_idx <= 15
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder"
      ) as builder_name,
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
  )
SELECT id, test_id, builder_name, status, duration, step_name, timeout, typ_tags, expectation_files
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC
"""
        self.assertEqual(self._querier._GetPublicCiQuery(), expected_query)

    def testInternalCi(self) -> None:
        """Tests that the generated internal CI query is as expected."""
        expected_query = """\
WITH
  builds AS (
    WITH
      all_builds AS (
        SELECT
          DISTINCT exported.id AS build_inv_id,
          variant.*,
          partition_time
        FROM
          `chrome-luci-data.chrome.blink_web_tests_ci_test_results` AS tr,
          UNNEST(variant) AS variant
        WHERE
          DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
          AND exported.realm = "chrome:ci"
          AND key = "builder"
      ),
      grouped_builds AS (
        SELECT
          build_inv_id,
          value AS builder,
          partition_time,
          RANK() OVER (PARTITION BY value ORDER BY partition_time DESC) AS rank_idx,
        FROM all_builds
      )
    SELECT
      build_inv_id,
      builder,
      partition_time
    FROM grouped_builds
    WHERE rank_idx <= 15
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder"
      ) as builder_name,
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
  )
SELECT id, test_id, builder_name, status, duration, step_name, timeout, typ_tags, expectation_files
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC
"""
        self.assertEqual(self._querier._GetInternalCiQuery(), expected_query)

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
    WITH
      all_builds AS (
        SELECT
          DISTINCT exported.id AS build_inv_id,
          variant.*,
          partition_time
        FROM
          `chrome-luci-data.chromium.blink_web_tests_try_test_results` AS tr,
          UNNEST(variant) AS variant,
          submitted_builds sb
        WHERE
          DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
          AND exported.realm = "chromium:try"
          AND key = "builder"
          AND exported.id = sb.id
      ),
      grouped_builds AS (
        SELECT
          build_inv_id,
          value AS builder,
          partition_time,
          RANK() OVER (PARTITION BY value ORDER BY partition_time DESC) AS rank_idx,
        FROM all_builds
      )
    SELECT
      build_inv_id,
      builder,
      partition_time
    FROM grouped_builds
    WHERE rank_idx <= 15
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder"
      ) as builder_name,
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
  )
SELECT id, test_id, builder_name, status, duration, step_name, timeout, typ_tags, expectation_files
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC
"""
        self.assertEqual(self._querier._GetPublicTryQuery(), expected_query)

    def testInternalTry(self) -> None:
        """Tests that the generated internal try query is as expected."""
        expected_query = """\
WITH
  submitted_builds AS (
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.chrome.attempts`,
      UNNEST(builds) as unnested_builds,
      UNNEST(gerrit_changes) as unnested_changes
    WHERE
      unnested_builds.host = "cr-buildbucket.appspot.com"
      AND unnested_changes.submit_status = "SUCCESS"
      AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                     INTERVAL 30 DAY)
  ),
  builds AS (
    WITH
      all_builds AS (
        SELECT
          DISTINCT exported.id AS build_inv_id,
          variant.*,
          partition_time
        FROM
          `chrome-luci-data.chrome.blink_web_tests_try_test_results` AS tr,
          UNNEST(variant) AS variant,
          submitted_builds sb
        WHERE
          DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
          AND exported.realm = "chrome:try"
          AND key = "builder"
          AND exported.id = sb.id
      ),
      grouped_builds AS (
        SELECT
          build_inv_id,
          value AS builder,
          partition_time,
          RANK() OVER (PARTITION BY value ORDER BY partition_time DESC) AS rank_idx,
        FROM all_builds
      )
    SELECT
      build_inv_id,
      builder,
      partition_time
    FROM grouped_builds
    WHERE rank_idx <= 15
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder"
      ) as builder_name,
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
  )
SELECT id, test_id, builder_name, status, duration, step_name, timeout, typ_tags, expectation_files
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC
"""
        self.assertEqual(self._querier._GetInternalTryQuery(), expected_query)


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
