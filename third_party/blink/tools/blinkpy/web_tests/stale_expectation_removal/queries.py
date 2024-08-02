# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Web test-specific impl of the unexpected passes' queries module."""

import datetime
import os
import posixpath
import typing
from typing import List, Optional

from blinkpy.web_tests.stale_expectation_removal import constants
from blinkpy.web_tests.stale_expectation_removal import data_types

from unexpected_passes_common import constants as common_constants
from unexpected_passes_common import data_types as common_data_types
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
          `chrome-luci-data.{project}.blink_web_tests_ci_test_results` AS tr,
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
          `chrome-luci-data.{project}.blink_web_tests_try_test_results` AS tr,
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
      `chrome-luci-data.{project}.blink_web_tests_{ci_or_try}_test_results` tr,
      builds b
    WHERE
      DATE(tr.partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
      AND exported.id = build_inv_id
      AND status != "SKIP"
  )"""

# Selects the relevant columns from results that had a non-Pass expectation when
# they were run, ordered by builder name.
FINAL_SELECTOR_QUERY = """\
SELECT id, test_id, builder_name, status, duration, step_name, timeout, typ_tags, expectation_files
FROM results
WHERE
  "Failure" IN UNNEST(typ_expectations)
  OR "Crash" IN UNNEST(typ_expectations)
  OR "Timeout" IN UNNEST(typ_expectations)
  OR "Slow" IN UNNEST(typ_expectations)
ORDER BY builder_name DESC"""

# Gets the Buildbucket IDs for all the public trybots that:
#   1. Run Blink web tests
#   2. Were used for CL submission (i.e. weren't for intermediate patchsets)
PUBLIC_TRY_SUBMITTED_BUILDS_SUBQUERY = """\
  submitted_builds AS (
{chromium_builds_subquery}
  )""".format(chromium_builds_subquery=queries_module.
              PARTITIONED_SUBMITTED_BUILDS_TEMPLATE.format(
                  project_view='chromium'))

# The same as PUBLIC_TRY_SUBMITTED_BUILDS_SUBQUERY, but for internal trybots.
INTERNAL_TRY_SUBMITTED_BUILDS_SUBQUERY = """\
  submitted_builds AS (
{chrome_builds_subquery}
  )""".format(chrome_builds_subquery=queries_module.
              PARTITIONED_SUBMITTED_BUILDS_TEMPLATE.format(
                  project_view='chrome'))

KNOWN_TEST_ID_PREFIXES = [
    'ninja://:blink_web_tests/',
    'ninja://:blink_wpt_tests/',
    'ninja://:chrome_wpt_tests/',
    'ninja://:webgpu_blink_web_tests/',
]

# The default timeout of most web tests is 6 seconds, so use that if we happen
# to get a result that doesn't report its own timeout.
DEFAULT_TIMEOUT = datetime.timedelta(seconds=6)


class WebTestBigQueryQuerier(queries_module.BigQueryQuerier):

    def _GetPublicCiQuery(self) -> str:
        return """\
WITH
{builds_subquery},
{results_subquery}
{final_selector_query}
""".format(builds_subquery=CI_BUILDS_SUBQUERY.format(
            project='chromium', num_builds=self._num_samples),
           results_subquery=RESULTS_SUBQUERY.format(project='chromium',
                                                    ci_or_try='ci'),
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
                                                    ci_or_try='ci'),
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
                                                    ci_or_try='try'),
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
                                                    ci_or_try='try'),
           final_selector_query=FINAL_SELECTOR_QUERY)

    def _ConvertBigQueryRowToResultObject(
            self, row: queries_module.QueryResult) -> data_types.WebTestResult:
        result = super()._ConvertBigQueryRowToResultObject(row)
        # The actual returned data type is set at runtime, so we need to force
        # pytype to treat this as the correct type during its static analysis,
        # which doesn't set the the data type.
        result = typing.cast(data_types.WebTestResult, result)
        duration = float(row.duration)
        duration = datetime.timedelta(seconds=duration)
        timeout = row.timeout
        timeout = (datetime.timedelta(
            seconds=float(timeout)) if timeout else DEFAULT_TIMEOUT)
        result.SetDuration(duration, timeout)
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
        return 'wpt_internal/webgpu/' in result['test_id']

    def _StripPrefixFromTestId(self, test_id: str) -> str:
        # Web test IDs provided by ResultDB are the test name known by the test
        # runner prefixed by one of the following:
        #   "ninja://:blink_web_tests/"
        #   "ninja://:webgpu_blink_web_tests/"
        for prefix in KNOWN_TEST_ID_PREFIXES:
            if test_id.startswith(prefix):
                return test_id.replace(prefix, '')
        raise RuntimeError('Unable to strip prefix from test ID %s' % test_id)
