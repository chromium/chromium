# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for querying BigQuery."""

import collections
import json
import os
import subprocess

from flake_suppressor import common_typing as ct
from flake_suppressor import results as results_module
from flake_suppressor import tag_utils

from unexpected_passes_common import queries as upc_queries

MAX_ROWS = (2**31) - 1

# A note about the try version of the queries: The submitted builds subquery is
# included in each query instead of running it once by itself and including the
# returned data in other queries because we can end up getting a very large
# number of build IDs, which can push the query over BigQuery's hard query size
# limit. The query runs pretty quickly (within a couple of seconds), so
# duplicating it does not add much runtime.

# Subquery for getting all builds used for CL submission in the past
# |sample_period| days. Will be inserted into other queries.
SUBMITTED_BUILDS_SUBQUERY = """\
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
                                     INTERVAL @sample_period DAY)
  ),
"""

# Gets all failures from the past |sample_period| days from CI bots that did not
# already have an associated test suppression when the test ran.
CI_FAILED_TEST_QUERY = """\
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
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      status = "FAIL"
      AND exported.realm = "chromium:ci"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
  )
SELECT *
FROM failed_tests ft
WHERE
  ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass"
"""

# Gets all failures from the past |sample_period| days from trybots that did not
# already have an associated test suppresssion when the test ran, only including
# data from builds that were used for CL submission.
TRY_FAILED_TEST_QUERY = """\
WITH
  {submitted_builds_subquery}
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
        WHERE key = "raw_typ_expectation") as typ_expectations
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
      submitted_builds sb
    WHERE
      status = "FAIL"
      AND exported.realm = "chromium:try"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
      AND exported.id = sb.id
  )
SELECT *
FROM failed_tests ft
WHERE
  ARRAY_TO_STRING(ft.typ_expectations, '') = "Pass"
""".format(submitted_builds_subquery=SUBMITTED_BUILDS_SUBQUERY)

# Gets the count of all results in the past |sample_period| days for distinct
# test/tag combinations from CI bots.
CI_RESULT_COUNT_QUERY = """\
WITH
  grouped_results AS (
    SELECT
      exported.id as id,
      test_metadata.name as name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags
    FROM
      `chrome-luci-data.chromium.gpu_ci_test_results` tr
    WHERE
      exported.realm = "chromium:ci"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
  )
SELECT
  COUNT(gr.id) as result_count,
  ANY_VALUE(gr.name) as test_name,
  ANY_VALUE(gr.typ_tags) as typ_tags
FROM grouped_results gr
GROUP BY gr.name, ARRAY_TO_STRING(gr.typ_tags, '')
"""

# Gets the count of all results in the past |sample_period| days for distinct
# test/tag combinations from trybots, only including data from builds that were
# used for CL submission.
TRY_RESULT_COUNT_QUERY = """\
WITH
  {submitted_builds_subquery}
  grouped_results AS (
    SELECT
      exported.id as id,
      test_metadata.name as name,
      ARRAY(
        SELECT value
        FROM tr.tags
        WHERE key = "typ_tag") as typ_tags
    FROM
      `chrome-luci-data.chromium.gpu_try_test_results` tr,
      submitted_builds sb
    WHERE
      exported.realm = "chromium:try"
      AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                         INTERVAL @sample_period DAY)
      AND exported.id = sb.id
  )
SELECT
  COUNT(gr.id) as result_count,
  ANY_VALUE(gr.name) as test_name,
  ANY_VALUE(gr.typ_tags) as typ_tags
FROM grouped_results gr
GROUP BY gr.name, ARRAY_TO_STRING(gr.typ_tags, '')
""".format(submitted_builds_subquery=SUBMITTED_BUILDS_SUBQUERY)


class BigQueryQuerier():
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

  def GetFlakyOrFailingCiTests(self) -> ct.QueryJsonType:
    """Gets all flaky or failing GPU tests from CI.

    Returns:
      A JSON representation of the BigQuery results containing all found flaky
      or failing test results that came from CI bots.
    """
    return self._GetJsonResultsFromBigQuery(CI_FAILED_TEST_QUERY)

  def GetFlakyOrFailingTryTests(self) -> ct.QueryJsonType:
    """Gets all flaky or failing GPU tests from the trybots.

    Limits results to those that came from builds used for CL submission.

    Returns:
      A JSON representation of the BigQuery results containing all found flaky
      or failing test results that came from trybots AND came from builds that
      were used for CL submission.
    """
    return self._GetJsonResultsFromBigQuery(TRY_FAILED_TEST_QUERY)

  def GetResultCounts(self) -> ct.ResultCountType:
    """Gets the result count for each test/config combination.

    Returns:
      A dict in the format:
      {
        typ_tags (tuple): {
          test_name (str): result_count (int)
        }
      }
    """
    # A default dict of default dicts of ints.
    result_counts = collections.defaultdict(lambda: collections.defaultdict(int)
                                            )
    self._GetResultCountWithQuery(CI_RESULT_COUNT_QUERY, result_counts)
    self._GetResultCountWithQuery(TRY_RESULT_COUNT_QUERY, result_counts)
    return result_counts

  def _GetJsonResultsFromBigQuery(self, query: str) -> ct.QueryJsonType:
    """Gets the JSON results from a BigQuery query.

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

  def _GetResultCountWithQuery(self, query: str,
                               result_counts: ct.ResultCountType) -> None:
    """Helper to get result counts using a particular query.

    Args:
      query: A string containing a SQL query to run.
      result_counts: A defaultdict of defaultdict of ints that will be modified
          in place to tally result counts.
    """
    json_results = self._GetJsonResultsFromBigQuery(query)

    for r in json_results:
      typ_tags = tuple(tag_utils.RemoveMostIgnoredTags(r['typ_tags']))
      test_name = r['test_name']
      _, test_name = results_module.GetTestSuiteAndNameFromResultDbName(
          test_name)
      count = int(r['result_count'])
      result_counts[typ_tags][test_name] += count
