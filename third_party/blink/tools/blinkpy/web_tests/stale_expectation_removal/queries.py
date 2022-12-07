# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' queries module."""

import os
import posixpath
import typing
from typing import List, Optional

from blinkpy.web_tests.stale_expectation_removal import constants
from blinkpy.web_tests.stale_expectation_removal import data_types

from unexpected_passes_common import constants as common_constants
from unexpected_passes_common import data_types as common_data_types
from unexpected_passes_common import queries as queries_module

RESULTS_SUBQUERY = """\
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
      `chrome-luci-data.{{builder_project}}.blink_web_tests_{builder_type}_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      {{test_filter_clause}}
  )"""

FINAL_SELECTOR_QUERY = """\
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)"""

# This query gets us all results for tests from CI that have had results with a
# Failure, Timeout, or Crash expectation in the past |@num_builds| builds on
# |@builder_name|.
CI_BQ_QUERY_TEMPLATE = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.{{builder_project}}.blink_web_tests_ci_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "{{builder_project}}:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
{results_subquery}
{final_selector_query}
""".format(results_subquery=RESULTS_SUBQUERY.format(
    builder_type=common_constants.BuilderTypes.CI),
           final_selector_query=FINAL_SELECTOR_QUERY)

SUBMITTED_BUILDS_SUBQUERY = """\
  submitted_builds AS (
{chromium_builds}
  ),""".format(chromium_builds=queries_module.SUBMITTED_BUILDS_TEMPLATE.format(
    project_view='chromium'))

# Same as CI_BQ_QUERY_TEMPLATE, but for tryjobs. Only data from builds that
# were used for CL submission is considered.
TRY_BQ_QUERY_TEMPLATE = """\
WITH
{submitted_builds_subquery}
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.{{builder_project}}.blink_web_tests_try_test_results` tr,
      submitted_builds sb
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "{{builder_project}}:try"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
      AND exported.id = sb.id
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
{results_subquery}
{final_selector_query}
""".format(submitted_builds_subquery=SUBMITTED_BUILDS_SUBQUERY,
           results_subquery=RESULTS_SUBQUERY.format(
               builder_type=common_constants.BuilderTypes.TRY),
           final_selector_query=FINAL_SELECTOR_QUERY)

# Very similar to above, but used to get the names of tests that are of
# interest for use as a filter.
TEST_FILTER_QUERY_TEMPLATE = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.{builder_project}.blink_web_tests_{builder_type}_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.realm = "{builder_project}:{builder_type}"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT 50
  ),
  results AS (
    SELECT
      exported.id,
      test_id,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.{builder_project}.blink_web_tests_{builder_type}_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
  )
SELECT DISTINCT r.test_id
FROM results r
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
"""

ALL_BUILDERS_FROM_TABLE_SUBQUERY = """\
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.{builder_project}.blink_web_tests_{builder_type}_test_results` tr
    WHERE
      DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)"""

ACTIVE_BUILDER_QUERY_TEMPLATE = """\
WITH
  builders AS (
{all_builders_from_table_subquery}
{{active_internal_builder_subquery}}
  )
SELECT DISTINCT builder_name
FROM builders
""".format(all_builders_from_table_subquery=ALL_BUILDERS_FROM_TABLE_SUBQUERY)

ACTIVE_INTERNAL_BUILDER_SUBQUERY = """\
    UNION ALL
{all_builders_from_table_subquery}""".format(
    all_builders_from_table_subquery=ALL_BUILDERS_FROM_TABLE_SUBQUERY)

KNOWN_TEST_ID_PREFIXES = [
    'ninja://:blink_web_tests/',
    'ninja://:blink_wpt_tests/',
    'ninja://:webgpu_blink_web_tests/',
]

# The default timeout of most web tests is 6 seconds, so use that if we happen
# to get a result that doesn't report its own timeout.
DEFAULT_TIMEOUT = 6


class WebTestBigQueryQuerier(queries_module.BigQueryQuerier):
    def _ConvertJsonResultToResultObject(
            self, json_result: queries_module.QueryResult
    ) -> data_types.WebTestResult:
        result = super(WebTestBigQueryQuerier,
                       self)._ConvertJsonResultToResultObject(json_result)
        # The actual returned data type is set at runtime, so we need to force
        # pytype to treat this as the correct type during its static analysis,
        # which doesn't set the the data type.
        result = typing.cast(data_types.WebTestResult, result)
        result.SetDuration(json_result['duration'], json_result['timeout']
                           or DEFAULT_TIMEOUT)
        return result

    def _GetRelevantExpectationFilesForQueryResult(
            self, query_result: queries_module.QueryResult) -> List[str]:
        # Files in the query are either relative to the web tests directory or
        # are an absolute path. The paths are always POSIX-style. We don't
        # handle absolute paths since those typically point to temporary files
        # which will not exist locally.
        filepaths = []
        for f in query_result.get('expectation_files', []):
            if posixpath.isabs(f):
                continue
            f = f.replace('/', os.sep)
            f = os.path.join(constants.WEB_TEST_ROOT_DIR, f)
            filepaths.append(f)
        return filepaths

    def _ShouldSkipOverResult(self,
                              result: queries_module.QueryResult) -> bool:
        # WebGPU web tests are currently unsupported for various reasons.
        return 'webgpu/' in result['test_id']

    def _GetQueryGeneratorForBuilder(
            self, builder: common_data_types.BuilderEntry
    ) -> Optional[queries_module.BaseQueryGenerator]:
        builder_type = builder.builder_type
        # Look for all tests.
        if not self._large_query_mode:
            return WebTestFixedQueryGenerator(builder, '')

        query = TEST_FILTER_QUERY_TEMPLATE.format(
            builder_project=builder.project, builder_type=builder.builder_type)
        query_results = self._RunBigQueryCommandsForJsonOutput(
            query, {'': {
                'builder_name': builder.name
            }})
        test_ids = ['"%s"' % r['test_id'] for r in query_results]

        if not test_ids:
            return None

        # Only consider specific test cases that were found to have active
        # expectations in the above query. Also perform any initial query
        # splitting.
        target_num_ids = (queries_module.TARGET_RESULTS_PER_QUERY //
                          self._num_samples)
        return WebTestSplitQueryGenerator(builder, test_ids, target_num_ids)

    def _StripPrefixFromTestId(self, test_id: str) -> str:
        # Web test IDs provided by ResultDB are the test name known by the test
        # runner prefixed by one of the following:
        #   "ninja://:blink_web_tests/"
        #   "ninja://:webgpu_blink_web_tests/"
        for prefix in KNOWN_TEST_ID_PREFIXES:
            if test_id.startswith(prefix):
                return test_id.replace(prefix, '')
        raise RuntimeError('Unable to strip prefix from test ID %s' % test_id)

    def _GetActiveBuilderQuery(self, builder_type: str,
                               include_internal_builders: bool) -> str:
        if include_internal_builders:
            subquery = ACTIVE_INTERNAL_BUILDER_SUBQUERY.format(
                builder_project='chrome', builder_type=builder_type)
        else:
            subquery = ''
        return ACTIVE_BUILDER_QUERY_TEMPLATE.format(
            builder_project='chromium',
            builder_type=builder_type,
            active_internal_builder_subquery=subquery)


class WebTestFixedQueryGenerator(queries_module.FixedQueryGenerator):
    def GetQueries(self) -> List[str]:
        return QueryGeneratorImpl(self.GetClauses(), self._builder)


class WebTestSplitQueryGenerator(queries_module.SplitQueryGenerator):
    def GetQueries(self) -> List[str]:
        return QueryGeneratorImpl(self.GetClauses(), self._builder)


def QueryGeneratorImpl(test_filter_clauses: List[str],
                       builder: common_data_types.BuilderEntry) -> List[str]:
    queries = []
    query_template = None
    if builder.builder_type == common_constants.BuilderTypes.CI:
        query_template = CI_BQ_QUERY_TEMPLATE
    elif builder.builder_type == common_constants.BuilderTypes.TRY:
        query_template = TRY_BQ_QUERY_TEMPLATE
    else:
        raise RuntimeError('Unknown builder type %s' % builder.builder_type)
    for tfc in test_filter_clauses:
        queries.append(
            query_template.format(builder_project=builder.project,
                                  test_filter_clause=tfc))
    return queries
