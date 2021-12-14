# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' queries module."""

import os
import posixpath

from blinkpy.web_tests.stale_expectation_removal import constants

from unexpected_passes_common import constants as common_constants
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
      `chrome-luci-data.chromium.blink_web_tests_{builder_type}_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      {{test_filter_clause}}
  )"""

FINAL_SELECTOR_QUERY = """\
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)"""

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
      `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
    WHERE
      exported.realm = "chromium:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
{results_subquery}
{final_selector_query}
""".format(results_subquery=RESULTS_SUBQUERY.format(
    builder_type=common_constants.BuilderTypes.CI),
           final_selector_query=FINAL_SELECTOR_QUERY)

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
      `chrome-luci-data.chromium.blink_web_tests_try_test_results` tr,
      submitted_builds sb
    WHERE
      exported.realm = "chromium:try"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
      AND exported.id = sb.id
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
{results_subquery}
{final_selector_query}
""".format(submitted_builds_subquery=queries_module.SUBMITTED_BUILDS_SUBQUERY,
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
      `chrome-luci-data.chromium.blink_web_tests_{builder_type}_test_results` tr
    WHERE
      exported.realm = "chromium:{builder_type}"
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
      `chrome-luci-data.chromium.blink_web_tests_{builder_type}_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
  )
SELECT DISTINCT r.test_id
FROM results r
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
"""

ACTIVE_BUILDER_QUERY_TEMPLATE = """\
WITH
  builders AS (
    SELECT
      (
        SELECT value
        FROM tr.variant
        WHERE key = "builder") as builder_name
    FROM
      `chrome-luci-data.chromium.blink_web_tests_{builder_type}_test_results` tr
  )
SELECT DISTINCT builder_name
FROM builders
"""

KNOWN_TEST_ID_PREFIXES = [
    'ninja://:blink_web_tests/',
    'ninja://:webgpu_blink_web_tests',
]

# The default timeout of most web tests is 6 seconds, so use that if we happen
# to get a result that doesn't report its own timeout.
DEFAULT_TIMEOUT = 6


class WebTestBigQueryQuerier(queries_module.BigQueryQuerier):
    def _ConvertJsonResultToResultObject(self, json_result):
        result = super(WebTestBigQueryQuerier,
                       self)._ConvertJsonResultToResultObject(json_result)
        result.SetDuration(json_result['duration'], json_result['timeout']
                           or DEFAULT_TIMEOUT)
        return result

    def _GetRelevantExpectationFilesForQueryResult(self, query_result):
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

    def _ShouldSkipOverResult(self, result):
        # WebGPU web tests are currently unsupported for various reasons.
        return 'webgpu/cts.html' in result['test_id']

    def _GetQueryGeneratorForBuilder(self, builder, builder_type):
        # Look for all tests.
        if not self._large_query_mode:
            return WebTestFixedQueryGenerator(builder_type, '')

        query = TEST_FILTER_QUERY_TEMPLATE.format(builder_type=builder_type)
        query_results = self._RunBigQueryCommandsForJsonOutput(
            query, {'': {
                'builder_name': builder
            }})
        test_ids = ['"%s"' % r['test_id'] for r in query_results]

        if not test_ids:
            return None

        # Only consider specific test cases that were found to have active
        # expectations in the above query. Also perform any initial query
        # splitting.
        target_num_ids = (queries_module.TARGET_RESULTS_PER_QUERY /
                          self._num_samples)
        return WebTestSplitQueryGenerator(builder_type, test_ids,
                                          target_num_ids)

    def _StripPrefixFromTestId(self, test_id):
        # Web test IDs provided by ResultDB are the test name known by the test
        # runner prefixed by one of the following:
        #   "ninja://:blink_web_tests/"
        #   "ninja://:webgpu_blink_web_tests/"
        for prefix in KNOWN_TEST_ID_PREFIXES:
            if test_id.startswith(prefix):
                return test_id.replace(prefix, '')
        raise RuntimeError('Unable to strip prefix from test ID %s' % test_id)

    def _GetActiveBuilderQuery(self, builder_type):
        return ACTIVE_BUILDER_QUERY_TEMPLATE.format(builder_type=builder_type)


class WebTestFixedQueryGenerator(queries_module.FixedQueryGenerator):
    def GetQueries(self):
        return QueryGeneratorImpl(self.GetClauses(), self._builder_type)


class WebTestSplitQueryGenerator(queries_module.SplitQueryGenerator):
    def GetQueries(self):
        return QueryGeneratorImpl(self.GetClauses(), self._builder_type)


def QueryGeneratorImpl(test_filter_clauses, builder_type):
    queries = []
    query_template = None
    if builder_type == common_constants.BuilderTypes.CI:
        query_template = CI_BQ_QUERY_TEMPLATE
    elif builder_type == common_constants.BuilderTypes.TRY:
        query_template = TRY_BQ_QUERY_TEMPLATE
    else:
        raise RuntimeError('Unknown builder type %s' % builder_type)
    for tfc in test_filter_clauses:
        queries.append(query_template.format(test_filter_clause=tfc))
    return queries
