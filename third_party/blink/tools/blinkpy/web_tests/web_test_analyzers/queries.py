# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for querying BigQuery."""

import json
import os
import subprocess
from typing import List, Optional

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import queries as queries_module

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

# Gets overall test slowness from the past |sample_period| days from CI
# bots.
# List of data selected from the database:
# test_name      - the full test name, like external/wpt/rendering/test1
# builder        - name of the builder that runs the test
# slow_count     - total number of pass and failed test runs that are slow
# non_slow_count - total number of pass and failed test runs that are not slow
# avg_duration   - average runtime of slow and non-slow tests
# timeout_count  - total number of timeout test runs
CI_TESTS_OVERALL_SLOWNESS_QUERY = """\
WITH
  {sheriff_rotations_ci_builds}
  tests_slowness AS (
  SELECT
    test_metadata.name as test_name,
    CAST((SELECT value FROM tr.tags
     WHERE key = "web_tests_base_timeout") AS float64) as timeout,
    (SELECT value FROM tr.variant
     WHERE key = "test_suite") as test_suite,
    (SELECT value FROM tr.variant
     WHERE key = "builder") as builder,
    CAST((
          SELECT value
          FROM tr.tags
          WHERE key = "web_tests_test_was_slow") AS bool) as test_was_slow,
    duration,
    status,
  FROM `chrome-luci-data.chromium.blink_web_tests_ci_test_results` tr
  WHERE
    (status = "ABORT" OR status = "PASS" OR status = "FAIL") AND
    {test_path_selector}
    exported.realm = "chromium:ci" AND
    partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                   INTERVAL @sample_period DAY)
)
SELECT
  test_name,
  builder,
  timeout,
  SUM(CASE WHEN test_was_slow = true THEN 1 ELSE 0 END) AS slow_count,
  SUM(CASE WHEN test_was_slow = false THEN 1 ELSE 0 END) AS non_slow_count,
  AVG(CASE WHEN status = "ABORT" THEN NULL ELSE duration END) AS avg_duration,
  SUM(CASE WHEN status = "ABORT" THEN 1 ELSE 0 END) AS timeout_count,
FROM tests_slowness
WHERE
   (REGEXP_CONTAINS(test_suite, "blink_w(pt|eb)_tests")
    AND NOT REGEXP_CONTAINS(test_suite, "^webgpu"))
  {builder_selector}
GROUP BY test_name, builder, timeout
"""

# Gets all web test flaky bugs and their flaky tests from the past |sample_period|
# days.
# List of data selected from the database:
# bug_id - bug id for the flaky tests, like chromium/1493418
# create_time - the creation time of this bug, like 2023-10-16 21:55:00.474-07:00
# test_ids - list of the test ids, like [ninja://:testA,ninja://:testB]
WEB_TEST_FLAKY_BUGS_QUERY = """
WITH
  blink_flaky_bugs AS (
    SELECT
      bug.id,
      create_time,
      rule_id
    FROM `luci-analysis.chromium.failure_association_rules`
    WHERE project = "chromium" AND
      is_active = TRUE AND
    DATE(create_time) > DATE_SUB(CURRENT_DATE(), INTERVAL @sample_period DAY)
  )
  SELECT
    blink_flaky_bugs.id AS bug_id,
    blink_flaky_bugs.create_time,
    ARRAY_AGG(DISTINCT failures_table.test_id LIMIT 10) AS test_ids
  FROM blink_flaky_bugs JOIN `luci-analysis.chromium.clustered_failures` AS failures_table
  ON failures_table.cluster_id = blink_flaky_bugs.rule_id
  WHERE REGEXP_CONTAINS(
          (SELECT value FROM UNNEST(failures_table.variant) WHERE key = 'test_suite' LIMIT 1),
            'blink_w(pt|eb)_tests')
    AND cluster_algorithm LIKE 'rules-%'
  GROUP BY blink_flaky_bugs.id, blink_flaky_bugs.create_time
"""

# Query that inserts all web test analyzer bugs attach result to the database.
WEB_TEST_ANALYZER_RESULT_UPDATE_QUERY = """
INSERT INTO `chrome-unexpected-pass-data.chromium.web_test_analyzer_data` (
  timestamp, analyzer_type, bug_type, bug_id)
VALUES
{values}
"""


class Querier:
    def __init__(self, sample_period: int, billing_project: str):
        """Class for making calls to BigQuery.

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

    def get_overall_slowness_ci_tests(
            self,
            test_path: Optional[str] = None,
            only_check_sheriff_builds: Optional[bool] = True
    ) -> ct.QueryJsonType:
        """Gets overall test slowness data from CI under the test path.

        Args:
          test_path: A string of test path that contains the tests to do
              analysis. If the value is empty, this will check all tests.
          only_check_sheriff_builds: A bool var to indicate if only
              check sheriff builders or all builders.

        Returns:
          A JSON representation of the BigQuery results containing all found
          overall test slowness that came from CI bots under the test path.
        """
        if test_path:
            test_path_selector = ('STARTS_WITH(test_metadata.name, "%s") AND' %
                                  test_path)
        else:
            test_path_selector = ''

        if only_check_sheriff_builds:
            chromium_builds = queries_module.SHERIFF_ROTATIONS_CI_BUILDS_TEMPLATE
            sheriff_rotations_ci_builds = \
                f'sheriff_rotations_ci_builds AS ({chromium_builds}),'
            builder_selector = ('AND builder IN (SELECT builder '
                                'FROM sheriff_rotations_ci_builds)')
        else:
            sheriff_rotations_ci_builds = ''
            builder_selector = ''

        return self._get_json_results(
            CI_TESTS_OVERALL_SLOWNESS_QUERY.format(
                sheriff_rotations_ci_builds=sheriff_rotations_ci_builds,
                test_path_selector=test_path_selector,
                builder_selector=builder_selector))

    def get_web_test_flaky_bugs(self) -> ct.QueryJsonType:
        """Gets all web test flaky bugs from the database.

        Returns:
          A JSON representation of the BigQuery results containing all found
          web test flaky bug information.
        """

        return self._get_json_results(WEB_TEST_FLAKY_BUGS_QUERY)

    def insert_web_test_analyzer_result(self, analyzer_type: str,
                                        bug_type: str, bug_list: List[str]):
        """Insert all web test analyzer bugs attach result to the database.

        Args:
          analyzer_type: The type of analyzer.
          bug_type: The type of this bug.
          bug_list: List of all bug ids for this type.
        """
        values = []
        for bug_id in bug_list:
            values.append(f"(CURRENT_TIMESTAMP, '{analyzer_type}',"
                          f" '{bug_type}', '{bug_id}')")
        value_string = ','.join(values)
        self._execute_query(
            WEB_TEST_ANALYZER_RESULT_UPDATE_QUERY.format(values=value_string))

    def _get_json_results(self, query: str) -> ct.QueryJsonType:
        """Gets the JSON results from an input BigQuery query.

        Automatically passes in the "@sample_period" parameterized argument to
        BigQuery.

        Args:
          query: A string containing the SQL query to run in BigQuery.

        Returns:
          The loaded JSON results from running |query|.
        """
        cmd = queries_module.GenerateBigQueryCommand(
            self._billing_project,
            {'INT64': {
                'sample_period': self._sample_period
            }},
            batch=False)

        with open(os.devnull, 'w') as devnull:
            try:
                completed_process = subprocess.run(cmd,
                                                   input=query,
                                                   stdout=subprocess.PIPE,
                                                   stderr=devnull,
                                                   check=True,
                                                   text=True)
            except subprocess.CalledProcessError as error:
                print("Failed to query result, run 'gcloud auth login'"
                      " first.")
                raise error

        return json.loads(completed_process.stdout)

    def _execute_query(self, query: str):
        """Execute an input BigQuery query.

        Args:
          query: A string containing the SQL query to run in BigQuery.
        """
        cmd = queries_module.GenerateBigQueryCommand(self._billing_project, {},
                                                     batch=False)

        with open(os.devnull, 'w') as devnull:
            try:
                subprocess.run(cmd,
                               input=query,
                               stdout=devnull,
                               stderr=devnull,
                               check=True,
                               text=True)
            except subprocess.CalledProcessError as error:
                print("Failed to execute query, run 'gcloud auth login'"
                      " might fix the issue.")
                raise error
