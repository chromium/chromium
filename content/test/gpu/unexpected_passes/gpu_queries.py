# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' queries module."""

from typing import Iterable, Optional

from gpu_tests import gpu_integration_test

from unexpected_passes_common import queries as queries_module


# This query gets us the most recent |num_builds| CI builds from the past month
# for each builder.
CI_BUILDS_SUBQUERY = """\
  builds AS (
    WITH
      all_builds AS (
        SELECT
          DISTINCT exported.id AS build_inv_id,
          variant.*,
          partition_time
        FROM
          `chrome-luci-data.{project}.gpu_ci_test_results` AS tr,
          UNNEST(variant) AS variant
        WHERE
          DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
          AND exported.realm = "{project}:ci"
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
    WHERE rank_idx <= {num_builds}
  )"""

# The same as CI_BUILDS_SUBQUERY, but takes into account submitted builds for
# tryjobs.
TRY_BUILDS_SUBQUERY = """\
  builds AS (
    WITH
      all_builds AS (
        SELECT
          DISTINCT exported.id AS build_inv_id,
          variant.*,
          partition_time
        FROM
          `chrome-luci-data.{project}.gpu_try_test_results` AS tr,
          UNNEST(variant) AS variant,
          submitted_builds sb
        WHERE
          DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
          AND exported.realm = "{project}:try"
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
    WHERE rank_idx <= {num_builds}
  )"""


# step_name can be either the step_name tag or test_suite variant because of the
# way Skylab builders work. In normal Chromium test tasks, the step name is
# reported to the task-level RDB invocation, which then gets applied to every
# result that task reports. Skylab test steps are actually separate
# Buildbucket builds that report results a bit differently. They do not report
# a step_name tag, but do put the same information in for the test_suite
# variant. So, we will look for step_name first to cover most builders and fall
# back to test_suite for Skylab builders.
RESULTS_SUBQUERY = """\
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
      `chrome-luci-data.{project}.gpu_{ci_or_try}_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
      AND REGEXP_CONTAINS(
          test_id,
          "gpu_tests\\\\.{suite}\\\\.")
  )"""


# Selects the relevant columns from results that had either a Failure or a
# RetryOnFailure expectation when they were run, ordered by builder name.
FINAL_SELECTOR_QUERY = """\
SELECT id, test_id, builder_name, status, step_name, typ_tags
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "RetryOnFailure" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC"""

# Gets the Buildbucket IDs for all the public trybots that:
#   1. Run GPU tests
#   2. Were used for CL submission (i.e. weren't for intermediate patchsets)
PUBLIC_TRY_SUBMITTED_BUILDS_SUBQUERY = """\
  submitted_builds AS (
{chromium_builds_subquery}
    UNION ALL
{angle_builds_subquery}
  )""".format(
    chromium_builds_subquery=queries_module.
    PARTITIONED_SUBMITTED_BUILDS_TEMPLATE.format(project_view='chromium'),
    angle_builds_subquery=queries_module.PARTITIONED_SUBMITTED_BUILDS_TEMPLATE.
    format(project_view='angle'))

# The same as PUBLIC_TRY_SUBMITTED_BUILDS_SUBQUERY, but for internal trybots.
# There are no internal ANGLE tryjobs, so no need to look for attempts there.
INTERNAL_TRY_SUBMITTED_BUILDS_SUBQUERY = """\
  submitted_builds AS (
{chrome_builds_subquery}
  )""".format(chrome_builds_subquery=queries_module.
              PARTITIONED_SUBMITTED_BUILDS_TEMPLATE.format(
                  project_view='chrome'))


class GpuBigQueryQuerier(queries_module.BigQueryQuerier):
  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)

    name_mapping = gpu_integration_test.GenerateTestNameMapping()
    # The suite name we use for identification (return value of Name()) is not
    # the same as the one used by ResultDB (Python module), so convert here.
    self._suite = name_mapping[self._suite].__module__.split('.')[-1]

  def _GetPublicCiQuery(self) -> str:
    return """\
WITH
{builds_subquery},
{results_subquery}
{final_selector_query}
""".format(builds_subquery=CI_BUILDS_SUBQUERY.format(
        project='chromium', num_builds=self._num_samples),
           results_subquery=RESULTS_SUBQUERY.format(project='chromium',
                                                    ci_or_try='ci',
                                                    suite=self._suite),
           final_selector_query=FINAL_SELECTOR_QUERY)

  def _GetInternalCiQuery(self) -> str:
    return """\
WITH
{builds_subquery},
{results_subquery}
{final_selector_query}
""".format(builds_subquery=CI_BUILDS_SUBQUERY.format(
        project='chrome', num_builds=self._num_samples),
           results_subquery=RESULTS_SUBQUERY.format(project='chrome',
                                                    ci_or_try='ci',
                                                    suite=self._suite),
           final_selector_query=FINAL_SELECTOR_QUERY)

  def _GetPublicTryQuery(self) -> str:
    return """\
WITH
{submitted_builds_subquery},
{builds_subquery},
{results_subquery}
{final_selector_query}
""".format(submitted_builds_subquery=PUBLIC_TRY_SUBMITTED_BUILDS_SUBQUERY,
           builds_subquery=TRY_BUILDS_SUBQUERY.format(
               project='chromium', num_builds=self._num_samples),
           results_subquery=RESULTS_SUBQUERY.format(project='chromium',
                                                    ci_or_try='try',
                                                    suite=self._suite),
           final_selector_query=FINAL_SELECTOR_QUERY)

  def _GetInternalTryQuery(self) -> str:
    return """\
WITH
{submitted_builds_subquery},
{builds_subquery},
{results_subquery}
{final_selector_query}
""".format(submitted_builds_subquery=INTERNAL_TRY_SUBMITTED_BUILDS_SUBQUERY,
           builds_subquery=TRY_BUILDS_SUBQUERY.format(
               project='chrome', num_builds=self._num_samples),
           results_subquery=RESULTS_SUBQUERY.format(project='chrome',
                                                    ci_or_try='try',
                                                    suite=self._suite),
           final_selector_query=FINAL_SELECTOR_QUERY)

  def _GetRelevantExpectationFilesForQueryResult(
      self, _: queries_module.QueryResult) -> Optional[Iterable[str]]:
    # Only one expectation file is ever used for the GPU tests, so just use
    # whichever one we've read in.
    return None

  def _StripPrefixFromTestId(self, test_id: str) -> str:
    # GPU test IDs provided by ResultDB are the test name as known by the test
    # runner prefixed by
    # "ninja://<target>/gpu_tests.<suite>_integration_test.<class>.", e.g.
    #     "ninja://chrome/test:telemetry_gpu_integration_test/
    #      gpu_tests.pixel_integration_test.PixelIntegrationTest."
    split_id = test_id.split('.', 3)
    assert len(split_id) == 4
    return split_id[-1]
