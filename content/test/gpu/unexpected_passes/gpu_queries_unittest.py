#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import subprocess
import unittest
import unittest.mock as mock

from unexpected_passes import gpu_queries
from unexpected_passes import gpu_unittest_utils as gpu_uu
from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types
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

  def testSuiteExceptionMap(self) -> None:
    """Tests that the suite passed to the query changes for some suites."""

    def assertSuiteInQuery(suite: str, call_args: tuple) -> None:
      query = call_args[0][0][0]
      s = 'r"gpu_tests\\.%s\\."' % suite
      self.assertIn(s, query)

    # Non-special cased suite.
    querier = gpu_uu.CreateGenericGpuQuerier()
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('pixel_integration_test', query_mock.call_args)

    # Special-cased suites.
    querier = gpu_uu.CreateGenericGpuQuerier(suite='info_collection')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('info_collection_test', query_mock.call_args)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='power')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('power_measurement_integration_test',
                         query_mock.call_args)

    querier = gpu_uu.CreateGenericGpuQuerier(suite='trace_test')
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      _ = querier.QueryBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      assertSuiteInQuery('trace_integration_test', query_mock.call_args)


class GetQueryGeneratorForBuilderUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._querier = gpu_uu.CreateGenericGpuQuerier()
    self._query_patcher = mock.patch.object(
        self._querier, '_RunBigQueryCommandsForJsonOutput')
    self._query_mock = self._query_patcher.start()
    self.addCleanup(self._query_patcher.stop)

  def testNoLargeQueryMode(self) -> None:
    """Tests that the expected clause is returned in normal mode."""
    query_generator = self._querier._GetQueryGeneratorForBuilder(
        data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
    self.assertIsNotNone(query_generator)
    self.assertEqual(len(query_generator.GetClauses()), 1)
    self.assertEqual(
        query_generator.GetClauses()[0], """\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\\.pixel_integration_test\\.")""")
    self.assertIsInstance(query_generator, gpu_queries.GpuFixedQueryGenerator)
    self._query_mock.assert_not_called()
    # Make sure that there aren't any issues with getting the queries.
    q = query_generator.GetQueries()
    self.assertEqual(len(q), 1)

  def testLargeQueryModeNoTests(self) -> None:
    """Tests that a special value is returned if no tests are found."""
    querier = gpu_uu.CreateGenericGpuQuerier(large_query_mode=True)
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput',
                           return_value=[]) as query_mock:
      query_generator = querier._GetQueryGeneratorForBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      self.assertIsNone(query_generator)
      query_mock.assert_called_once()

  def testLargeQueryModeFoundTests(self) -> None:
    """Tests that a clause containing found tests is returned."""
    querier = gpu_uu.CreateGenericGpuQuerier(large_query_mode=True)
    with mock.patch.object(querier,
                           '_RunBigQueryCommandsForJsonOutput') as query_mock:
      query_mock.return_value = [{
          'test_id': 'foo_test'
      }, {
          'test_id': 'bar_test'
      }]
      query_generator = querier._GetQueryGeneratorForBuilder(
          data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False))
      self.assertIsNotNone(query_generator)
      self.assertEqual(query_generator.GetClauses(),
                       ['AND test_id IN UNNEST(["foo_test", "bar_test"])'])
      self.assertIsInstance(query_generator, gpu_queries.GpuSplitQueryGenerator)
      # Make sure that there aren't any issues with getting the queries.
      q = query_generator.GetQueries()
      self.assertEqual(len(q), 1)


class GetActiveBuilderQueryUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.querier = gpu_uu.CreateGenericGpuQuerier()

  def testPublicCi(self) -> None:
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
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)

  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.CI, False),
        expected_query)

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
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
    UNION ALL
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chrome.gpu_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.CI, True),
        expected_query)

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
      `chrome-luci-data.chromium.gpu_try_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)

  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.TRY, False),
        expected_query)

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
      `chrome-luci-data.chromium.gpu_try_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
    UNION ALL
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chrome.gpu_try_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
  )
SELECT DISTINCT builder_name
FROM builders
"""
    self.assertEqual(
        self.querier._GetActiveBuilderQuery(constants.BuilderTypes.TRY, True),
        expected_query)


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
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
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
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(builder_project='chromium',
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
      `chrome-luci-data.chrome.gpu_ci_test_results` tr
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
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(builder_project='chrome',
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
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
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
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(builder_project='chromium',
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
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chrome.gpu_try_test_results` tr,
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
      tfc
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
"""
    self.assertEqual(
        gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(builder_project='chrome',
                                                     test_filter_clause='tfc'),
        expected_query)


class QueryGeneratorImplUnittest(unittest.TestCase):
  def testPublicCi(self) -> None:
    """Tests that public CI builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder', constants.BuilderTypes.CI,
                                           False))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(
        builder_project='chromium', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testInternalCi(self) -> None:
    """Tests that internal CI builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder', constants.BuilderTypes.CI,
                                           True))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_CI_BQ_QUERY_TEMPLATE.format(
        builder_project='chrome', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testPublicTry(self) -> None:
    """Tests that public try builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder',
                                           constants.BuilderTypes.TRY, False))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(
        builder_project='chromium', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testInternalTry(self) -> None:
    """Tests that internal try builders use the correct query."""
    q = gpu_queries.QueryGeneratorImpl(['tfc'],
                                       data_types.BuilderEntry(
                                           'builder',
                                           constants.BuilderTypes.TRY, True))
    self.assertEqual(len(q), 1)
    expected_query = gpu_queries.GPU_TRY_BQ_QUERY_TEMPLATE.format(
        builder_project='chrome', test_filter_clause='tfc')
    self.assertEqual(q[0], expected_query)

  def testUnknownBuilderType(self) -> None:
    """Tests that an exception is raised for unknown builder types."""
    with self.assertRaises(RuntimeError):
      gpu_queries.QueryGeneratorImpl(['tfc'],
                                     data_types.BuilderEntry(
                                         'unknown_builder', 'unknown_type',
                                         False))


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
