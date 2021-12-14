# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' queries module."""

from __future__ import print_function

from unexpected_passes_common import constants
from unexpected_passes_common import queries as queries_module

RESULTS_SUBQUERY = """\
  results AS (
    SELECT
      exported.id,
      test_id,
      status,
      (
        SELECT value
        FROM tr.tags
        WHERE key = "step_name") as step_name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_{builder_type}_test_results` tr,
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
  OR "RetryOnFailure" IN UNNEST(typ_expectations)"""

# This query gets us all results for tests from CI that have had results with a
# RetryOnFailure or Failure expectation in the past |@num_builds| builds on
# |@builder_name| for the test |suite| (contained within |test_filter_clause|)
# type we're looking at.
GPU_CI_BQ_QUERY_TEMPLATE = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      exported.realm = "chromium:ci"
      AND STRUCT("builder", @builder_name) IN UNNEST(variant)
    ORDER BY partition_time DESC
    LIMIT @num_builds
  ),
{results_subquery}
{final_selector_query}
""".format(results_subquery=RESULTS_SUBQUERY.format(
    builder_type=constants.BuilderTypes.CI),
           final_selector_query=FINAL_SELECTOR_QUERY)

# Same as GPU_CI_BQ_QUERY_TEMPLATE, but for tryjobs. Only data from builds that
# were used for CL submission is considered.
GPU_TRY_BQ_QUERY_TEMPLATE = """\
WITH
{submitted_builds_subquery}
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
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
               builder_type=constants.BuilderTypes.TRY),
           final_selector_query=FINAL_SELECTOR_QUERY)

# Very similar to above, but used for getting the names of tests that are of
# interest for use as a filter.
TEST_FILTER_QUERY_TEMPLATE = """\
WITH
  builds AS (
    SELECT
      DISTINCT exported.id build_inv_id,
      partition_time
    FROM `chrome-luci-data.chromium.gpu_{builder_type}_test_results` tr
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
        WHERE key = "typ_tag") as typ_tags,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_{builder_type}_test_results` tr,
      builds b
    WHERE
      exported.id = build_inv_id
      AND status != "SKIP"
      AND REGEXP_CONTAINS(
        test_id,
        r"gpu_tests\.{suite}\.")
  )
SELECT DISTINCT r.test_id
FROM results r
WHERE
  (
    "Failure" IN UNNEST(typ_expectations)
    OR "RetryOnFailure" IN UNNEST(typ_expectations))
  {suite_filter_clause}
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
      `chrome-luci-data.chromium.gpu_{builder_type}_test_results` tr
  )
SELECT DISTINCT builder_name
FROM builders
"""

# The suite reported to Telemetry for selecting which suite to run is not
# necessarily the same one that is reported to typ/ResultDB, so map any special
# cases here.
TELEMETRY_SUITE_TO_RDB_SUITE_EXCEPTION_MAP = {
    'info_collection': 'info_collection_test',
    'power': 'power_measurement_integration_test',
    'trace_test': 'trace_integration_test',
}


class GpuBigQueryQuerier(queries_module.BigQueryQuerier):
  def __init__(self, suite, project, num_samples, large_query_mode):
    super(GpuBigQueryQuerier, self).__init__(suite, project, num_samples,
                                             large_query_mode)

    self._check_webgl_version = None
    self._webgl_version_tag = None
    # WebGL 1 and 2 tests are technically the same suite, but have different
    # expectation files. This leads to us getting both WebGL 1 and 2 results
    # when we only have expectations for one of them, which causes all the
    # results from the other to be reported as not having a matching
    # expectation.
    # TODO(crbug.com/1140283): Remove this once WebGL expectations are merged
    # and there's no need to differentiate them.
    if 'webgl_conformance' in self._suite:  # pylint: disable=access-member-before-definition
      webgl_version = self._suite[-1]  # pylint: disable=access-member-before-definition
      self._suite = 'webgl_conformance'
      self._webgl_version_tag = 'webgl-version-%s' % webgl_version
      self._check_webgl_version =\
          lambda tags: self._webgl_version_tag in tags
    else:
      self._check_webgl_version = lambda tags: True

    # Most test names are |suite|_integration_test, but there are several that
    # are not reported that way in typ, and by extension ResultDB, so adjust
    # that here.
    self._suite = TELEMETRY_SUITE_TO_RDB_SUITE_EXCEPTION_MAP.get(
        self._suite, self._suite + '_integration_test')

  def _ShouldSkipOverResult(self, result):
    # Skip over the result if the WebGL version does not match the one we're
    # looking for.
    return not self._check_webgl_version(result['typ_tags'])

  def _GetQueryGeneratorForBuilder(self, builder, builder_type):
    if not self._large_query_mode:
      # Look for all tests that match the given suite.
      return GpuFixedQueryGenerator(
          builder_type, """\
        AND REGEXP_CONTAINS(
          test_id,
          r"gpu_tests\.%s\.")""" % self._suite)

    query = TEST_FILTER_QUERY_TEMPLATE.format(
        builder_type=builder_type,
        suite=self._suite,
        suite_filter_clause=self._GetSuiteFilterClause())
    query_results = self._RunBigQueryCommandsForJsonOutput(
        query, {'': {
            'builder_name': builder
        }})
    test_ids = ['"%s"' % r['test_id'] for r in query_results]

    if not test_ids:
      return None

    # Only consider specific test cases that were found to have active
    # expectations in the above query. Also perform any initial query splitting.
    target_num_ids = queries_module.TARGET_RESULTS_PER_QUERY / self._num_samples
    return GpuSplitQueryGenerator(builder_type, test_ids, target_num_ids)

  def _GetRelevantExpectationFilesForQueryResult(self, _):
    # Only one expectation file is ever used for the GPU tests, so just use
    # whichever one we've read in.
    return None

  def _GetSuiteFilterClause(self):
    """Returns a SQL clause to only include relevant suites.

    Meant for cases where suites are differentiated by typ tag rather than
    reported suite name, e.g. WebGL 1 vs. 2 conformance.

    Returns:
      A string containing a valid SQL clause. Will be an empty string if no
      filtering is possible/necessary.
    """
    if not self._webgl_version_tag:
      return ''

    return 'AND "%s" IN UNNEST(typ_tags)' % self._webgl_version_tag

  def _StripPrefixFromTestId(self, test_id):
    # GPU test IDs provided by ResultDB are the test name as known by the test
    # runner prefixed by
    # "ninja://<target>/gpu_tests.<suite>_integration_test.<class>.", e.g.
    #     "ninja://chrome/test:telemetry_gpu_integration_test/
    #      gpu_tests.pixel_integration_test.PixelIntegrationTest."
    split_id = test_id.split('.', 3)
    assert len(split_id) == 4
    return split_id[-1]

  def _GetActiveBuilderQuery(self, builder_type):
    return ACTIVE_BUILDER_QUERY_TEMPLATE.format(builder_type=builder_type)


class GpuFixedQueryGenerator(queries_module.FixedQueryGenerator):
  def GetQueries(self):
    return QueryGeneratorImpl(self.GetClauses(), self._builder_type)


class GpuSplitQueryGenerator(queries_module.SplitQueryGenerator):
  def GetQueries(self):
    return QueryGeneratorImpl(self.GetClauses(), self._builder_type)


def QueryGeneratorImpl(test_filter_clauses, builder_type):
  queries = []
  query_template = None
  if builder_type == constants.BuilderTypes.CI:
    query_template = GPU_CI_BQ_QUERY_TEMPLATE
  elif builder_type == constants.BuilderTypes.TRY:
    query_template = GPU_TRY_BQ_QUERY_TEMPLATE
  else:
    raise RuntimeError('Unknown builder type %s' % builder_type)
  for tfc in test_filter_clauses:
    queries.append(query_template.format(test_filter_clause=tfc))

  return queries
