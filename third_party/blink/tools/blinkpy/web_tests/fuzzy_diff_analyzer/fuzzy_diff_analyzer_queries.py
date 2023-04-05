# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for querying BigQuery."""

import json
import os
import subprocess
from flake_suppressor_common import common_typing as ct
from unexpected_passes_common import queries as upc_queries

# Gets all image comparison failures from the past |sample_period| days from CI
# bots.
# TODO(crbug.com/1396136): Support Flag Specific Tests in the step check.
CI_FAILED_IMAGE_COMPARISON_TEST_QUERY = """
WITH
  failed_tests AS (
  SELECT
    exported.id,
    test_metadata.name,
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
          WHERE key = "step_name") as step_names,
    (SELECT value FROM tr.tags
     WHERE key = "web_tests_test_type") as test_type,
    (SELECT value FROM tr.tags
     WHERE key = "web_tests_image_diff_max_difference") as image_diff_max_difference,
    (SELECT value FROM tr.tags
     WHERE key = "web_tests_image_diff_total_pixels") as image_diff_total_pixels,

  FROM `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status = "FAIL" AND
    exported.realm = "chromium:ci" AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY) AND
    DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
)
SELECT *
FROM failed_tests ft
WHERE
  (STARTS_WITH(ARRAY_TO_STRING(step_names, ''), 'blink_wpt_tests') OR
   STARTS_WITH(ARRAY_TO_STRING(step_names, ''), 'blink_web_tests')) AND
  test_type = "image" AND
  image_diff_max_difference IS NOT NULL AND
  image_diff_total_pixels IS NOT NULL
"""


class FuzzyDiffAnalyzerQuerier:
    def __init__(self, sample_period: int, billing_project: str):
        """Class for making calls to BigQuery for Fuzzy Diff Analyzer.

        Args:
          sample_period: An int denoting the number of days that data should be
              queried over.
          billing_project: A string containing the billing project to use for
              BigQuery queries.
        """
        self._sample_period = sample_period
        self._billing_project = billing_project

    def get_failed_image_comparison_ci_tests(self) -> ct.QueryJsonType:
        """Gets all failed image comparison tests from CI.

        Returns:
          A JSON representation of the BigQuery results containing all found
          failed test results that came from CI bots.
        """
        return self._get_json_results(CI_FAILED_IMAGE_COMPARISON_TEST_QUERY)

    def _get_json_results(self, query: str) -> ct.QueryJsonType:
        """Gets the JSON results from an input BigQuery query.

        Automatically passes in the "@sample_period" parameterized argument to
        BigQuery.

        Args:
          query: A string containing the SQL query to run in BigQuery.

        Returns:
          The loaded JSON results from running |query|.
        """
        cmd = upc_queries.GenerateBigQueryCommand(
            self._billing_project,
            {'INT64': {
                'sample_period': self._sample_period
            }},
            batch=False)

        with open(os.devnull, 'w') as devnull:
            completed_process = subprocess.run(cmd,
                                               input=query,
                                               stdout=subprocess.PIPE,
                                               stderr=devnull,
                                               check=True,
                                               text=True)

        return json.loads(completed_process.stdout)
