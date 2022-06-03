# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' queries module."""

import os
import posixpath

from blinkpy.web_tests.stale_expectation_removal import constants

from unexpected_passes_common import queries as queries_module

# The target number of results/rows per query when running in large query mode.
# Higher values = longer individual query times and higher chances of running
# out of memory in BigQuery. Lower values = more parallelization overhead and
# more issues with rate limit errors.
TARGET_RESULTS_PER_QUERY = 20000

# This query gets us all results for tests that have had results with a
# Failure, Timeout, or Crash expectation in the past |@num_samples| builds on
# |@builder_name|. Whether these are CI or try results depends on whether
# |builder_type| is "ci" or "try".
BQ_QUERY_TEMPLATE = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM `chrome-luci-data.chromium.blink_web_tests_{builder_type}_test_results` tr
    WHERE
      exported.realm = "chromium:{builder_type}"
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
      `chrome-luci-data.chromium.blink_web_tests_{builder_type}_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      {test_filter_clause}
  )
SELECT *
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
"""

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
        target_num_ids = TARGET_RESULTS_PER_QUERY / self._num_samples
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
    for tfc in test_filter_clauses:
        queries.append(
            BQ_QUERY_TEMPLATE.format(builder_type=builder_type,
                                     test_filter_clause=tfc))
    return queries
