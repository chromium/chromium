# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for querying BigQuery."""

import json
import os
import subprocess
from typing import Optional

from flake_suppressor_common import common_typing as ct
from unexpected_passes_common import queries as upc_queries

# Gets all image comparison failures from the past |sample_period| days from CI
# bots.
# List of data selected from the database:
# exported.id - build id of this test, like build-12345
# test_metadata.name - the full test name, like external/wpt/rendering/test1
# typ_tags - list of the test build tag, like [linux,release,x86_64]
# typ_expectations - list of expectations, like [Failure,Pass,Timeout]
# step_names - list of step name from test task, like [blink_wpt_tests on Mac]
# test_type - test type of this test, like image
# image_diff_max_difference - the color channel diff in int, like 100
# image_diff_total_pixels - the pixel number diff in int, like 300
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
    ARRAY(
          SELECT value
          FROM tr.tags
          WHERE key = "web_tests_test_type") as test_types,
    (SELECT value FROM tr.tags
     WHERE key = "web_tests_image_diff_max_difference") as image_diff_max_difference,
    (SELECT value FROM tr.tags
     WHERE key = "web_tests_image_diff_total_pixels") as image_diff_total_pixels,

  FROM `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    status = "FAIL" AND
    exported.realm = "chromium:ci" AND
    {test_path_selector}
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY) AND
    DATE(partition_time) > DATE_SUB(CURRENT_DATE(), INTERVAL 30 DAY)
)
SELECT *
FROM failed_tests ft
WHERE
  'image' IN UNNEST(test_types) AND
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

    def get_failed_image_comparison_ci_tests(
            self, test_path: Optional[str] = None) -> ct.QueryJsonType:
        """Gets all failed image comparison tests from CI under the test path.

        Args:
          test_path: A string of test path that contains the tests to do
              analysis. If the value is empty, this will check all tests.

        Returns:
          A JSON representation of the BigQuery results containing all found
          failed test results that came from CI bots under the test path.
        """
        if test_path:
            test_path_selector = (
                'STARTS_WITH(test_metadata.name, "%s") AND' % test_path)
        else:
            test_path_selector = ''
        return self._get_json_results(
            CI_FAILED_IMAGE_COMPARISON_TEST_QUERY.format(
                test_path_selector=test_path_selector))

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
