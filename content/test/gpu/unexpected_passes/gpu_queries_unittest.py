#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import subprocess
import unittest
import unittest.mock as mock

from unexpected_passes import gpu_unittest_utils as gpu_uu
from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import expectations
from unexpected_passes_common import unittest_utils as uu


class QueryBuilderUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._patcher = mock.patch.object(subprocess, 'Popen')
    self._popen_mock = self._patcher.start()
    self.addCleanup(self._patcher.stop)

    builders.ClearInstance()
    expectations.ClearInstance()
    uu.RegisterGenericBuildersImplementation()
    uu.RegisterGenericExpectationsImplementation()

  def testSuiteNameTranslation(self) -> None:
    """Tests that the suite passed to the query is auto-translated."""
    # The key is the return value of Name() for a test suite, while the value is
    # the last part of the Python module for the test file (i.e. the name of the
    # file without .py). The former is used when running the tests, while the
    # latter is used by ResultDB for reporting.
    suites_to_modules = {
        'cast_streaming': 'cast_streaming_integration_test',
        'context_lost': 'context_lost_integration_test',
        'expected_color': 'expected_color_test',
        'gpu_process': 'gpu_process_integration_test',
        'hardware_accelerated_feature':
        'hardware_accelerated_feature_integration_test',
        'info_collection': 'info_collection_test',
        'noop_sleep': 'noop_sleep_integration_test',
        'pixel': 'pixel_integration_test',
        'power': 'power_measurement_integration_test',
        'screenshot_sync': 'screenshot_sync_integration_test',
        'trace_test': 'trace_integration_test',
        'webcodecs': 'webcodecs_integration_test',
        'webgl1_conformance': 'webgl1_conformance_integration_test',
        'webgl2_conformance': 'webgl2_conformance_integration_test',
        'webgpu_cts': 'webgpu_cts_integration_test',
    }

    def assertSuiteInQuery(suite: str, call_args: tuple) -> None:
      query = call_args[0][0]
      s = 'gpu_tests\\\\.%s\\\\.' % suite
      self.assertIn(s, query)

    for suite, module in suites_to_modules.items():
      querier = gpu_uu.CreateGenericGpuQuerier(suite=suite)
      with mock.patch.object(querier, '_GetSeriesForQuery',
                             return_value=[]) as query_mock:
        for _ in querier.GetBuilderGroupedQueryResults(
            constants.BuilderTypes.CI, False):
          pass
        query_mock.assert_called_once()
        assertSuiteInQuery(module, query_mock.call_args)


class GeneratedQueryUnittest(unittest.TestCase):
  maxDiff = None

  def setUp(self):
    self._querier = gpu_uu.CreateGenericGpuQuerier(suite='webgl1_conformance',
                                                   num_samples=15)

  def testPublicCi(self):
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
          `chrome-luci-data.chromium.gpu_ci_test_results` AS tr,
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
      IFNULL(
        (
          SELECT value
          FROM tr.tags
          WHERE key = "step_name"),
        (
          SELECT value
          FROM tr.variant
          WHERE key = "test_suite")) as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      AND REGEXP_CONTAINS(
          test_id,
          "gpu_tests\\\\.webgl1_conformance_integration_test\\\\.")
  )
SELECT id, test_id, builder_name, status, step_name, typ_tags
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
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
          `chrome-luci-data.chrome.gpu_ci_test_results` AS tr,
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
      IFNULL(
        (
          SELECT value
          FROM tr.tags
          WHERE key = "step_name"),
        (
          SELECT value
          FROM tr.variant
          WHERE key = "test_suite")) as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chrome.gpu_ci_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      AND REGEXP_CONTAINS(
          test_id,
          "gpu_tests\\\\.webgl1_conformance_integration_test\\\\.")
  )
SELECT id, test_id, builder_name, status, step_name, typ_tags
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
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
    UNION ALL
    SELECT
      CONCAT("build-", CAST(unnested_builds.id AS STRING)) as id
    FROM
      `commit-queue.angle.attempts`,
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
          `chrome-luci-data.chromium.gpu_try_test_results` AS tr,
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
      IFNULL(
        (
          SELECT value
          FROM tr.tags
          WHERE key = "step_name"),
        (
          SELECT value
          FROM tr.variant
          WHERE key = "test_suite")) as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      AND REGEXP_CONTAINS(
          test_id,
          "gpu_tests\\\\.webgl1_conformance_integration_test\\\\.")
  )
SELECT id, test_id, builder_name, status, step_name, typ_tags
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
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
          `chrome-luci-data.chrome.gpu_try_test_results` AS tr,
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
      IFNULL(
        (
          SELECT value
          FROM tr.tags
          WHERE key = "step_name"),
        (
          SELECT value
          FROM tr.variant
          WHERE key = "test_suite")) as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chrome.gpu_try_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      AND REGEXP_CONTAINS(
          test_id,
          "gpu_tests\\\\.webgl1_conformance_integration_test\\\\.")
  )
SELECT id, test_id, builder_name, status, step_name, typ_tags
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC
"""
    self.assertEqual(self._querier._GetInternalTryQuery(), expected_query)


class HelperMethodUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = gpu_uu.CreateGenericGpuQuerier()

  def testStripPrefixFromTestIdValidId(self):
    test_name = 'conformance/programs/program-handling.html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_tests.webgl_conformance_integration_test.'
              'WebGLConformanceIntegrationTest.')
    test_id = prefix + test_name
    self.assertEqual(self.instance._StripPrefixFromTestId(test_id), test_name)

  def testStripPrefixFromTestIdInvalidId(self) -> None:
    test_name = 'conformance/programs/program-handling_html'
    prefix = ('ninja://chrome/test:telemetry_gpu_integration_test/'
              'gpu_testse.webgl_conformance_integration_test.')
    test_id = prefix + test_name
    with self.assertRaises(AssertionError):
      self.instance._StripPrefixFromTestId(test_id)


if __name__ == '__main__':
  unittest.main(verbosity=2)
