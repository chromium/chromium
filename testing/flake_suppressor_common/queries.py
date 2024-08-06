# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for querying BigQuery."""

import collections
import json
import os
import subprocess
from typing import Any, Dict, List

from flake_suppressor_common import common_typing as ct
from flake_suppressor_common import results as results_module
from flake_suppressor_common import tag_utils

MAX_ROWS = (2**31) - 1

# A note about the try version of the queries: The submitted builds subquery is
# included in each query instead of running it once by itself and including the
# returned data in other queries because we can end up getting a very large
# number of build IDs, which can push the query over BigQuery's hard query size
# limit. The query runs pretty quickly (within a couple of seconds), so
# duplicating it does not add much runtime.

# Subquery for getting all builds used for CL submission in the past
# |sample_period| days. Will be inserted into other queries.
SUBMITTED_BUILDS_TEMPLATE = """\
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
"""

# Subquery for getting all ci builds that are in sheriff rotations in the past
# |sample_period| days. Will be inserted into other queries.
SHERIFF_ROTATIONS_CI_BUILDS_TEMPLATE = """\
  SELECT DISTINCT builder.builder,
  FROM
    `cr-buildbucket.chrome.builds_30d`
  WHERE
    input.properties LIKE '%sheriff_rotations%'
    AND JSON_VALUE_ARRAY(input.properties, '$.sheriff_rotations')[OFFSET(0)]
      IN ("chromium", "android")
    AND start_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(),
                                    INTERVAL @sample_period DAY)
"""

QueryParameters = Dict[str, Dict[str, Any]]


class BigQueryQuerier():
  def __init__(self, sample_period: int, billing_project: str,
               result_processor: results_module.ResultProcessor):
    """Class for making calls to BigQuery.

    Args:
      sample_period: An int denoting the number of days that data should be
          queried over.
      billing_project: A string containing the billing project to use for
          BigQuery queries.
    """
    self._sample_period = sample_period
    self._billing_project = billing_project
    self._result_processor = result_processor

  def GetFlakyOrFailingCiTests(self) -> ct.QueryJsonType:
    """Gets all flaky or failing tests from CI.

    Returns:
      A JSON representation of the BigQuery results containing all found flaky
      or failing test results that came from CI bots.
    """
    return self._GetJsonResultsFromBigQuery(self.GetFlakyOrFailingCiQuery())

  def GetFailingCiBuildCulpritTests(self) -> ct.QueryJsonType:
    """Gets all failing build culprit tests from CI builders.

    Returns:
      A JSON representation of the BigQuery results containing all found
      all failing build culprit results that came from CI bots.
    """
    return self._GetJsonResultsFromBigQuery(
        self.GetFailingBuildCulpritFromCiQuery())

  def GetFlakyOrFailingTestsFromCiBuilders(
      self, builder_names: List[str]) -> ct.QueryJsonType:
    """Gets all flaky or failing tests from input CI builders.

    Returns:
      A JSON representation of the BigQuery results containing all found
      all failing results that came from input CI builders.
    """
    return self._GetJsonResultsFromBigQuery(
        self.GetFlakyOrFailingFromCIBuildersQuery(builder_names))

  def GetFailingBuildCulpritFromCiBuilders(
      self, builder_names: List[str]) -> ct.QueryJsonType:
    """Gets all failing build culprit tests from input CI builders.

    Returns:
      A JSON representation of the BigQuery results containing all found
      all failing results that came from input CI builders.
    """
    return self._GetJsonResultsFromBigQuery(
        self.GetFailingBuildCulpritFromCIBuildersQuery(builder_names))

  def GetFlakyOrFailingTryTests(self) -> ct.QueryJsonType:
    """Gets all flaky or failing tests from the trybots.

    Limits results to those that came from builds used for CL submission.

    Returns:
      A JSON representation of the BigQuery results containing all found flaky
      or failing test results that came from trybots AND came from builds that
      were used for CL submission.
    """
    return self._GetJsonResultsFromBigQuery(self.GetFlakyOrFailingTryQuery())

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
    self._GetResultCountWithQuery(self.GetResultCountCIQuery(), result_counts)
    self._GetResultCountWithQuery(self.GetResultCountTryQuery(), result_counts)
    return result_counts

  def GetResultCountFromCiBuilders(
      self, builder_names: List[str]) -> ct.ResultCountType:
    """Gets the result count for the input CI builders.

    Returns:
      A dict in the format:
      {
        typ_tags (tuple): {
          test_name (str): result_count (int)
        }
      }
    """
    result_counts = collections.defaultdict(
        lambda: collections.defaultdict(int))
    self._GetResultCountWithQuery(
        self.GetResultCountFromCIBuildersQuery(builder_names), result_counts)
    return result_counts

  def GetFlakyOrFailingCiQuery(self) -> str:
    """
    Returns:
      Query string to get all the failing or flaky results from CI bots.
    """
    raise NotImplementedError

  def GetFailingBuildCulpritFromCiQuery(self) -> str:
    """
    Returns:
      Query string to get all failing build culprit results from CI bots.
    """
    raise NotImplementedError

  def GetFlakyOrFailingFromCIBuildersQuery(self,
                                           builder_names: List[str]) -> str:
    """
    Returns:
      Query string to get all the failing or flaky results from input CI
      builders.
    """
    raise NotImplementedError

  def GetFailingBuildCulpritFromCIBuildersQuery(
      self, builder_names: List[str]) -> str:
    """
    Returns:
      Query string to get all failing build culprit results from input CI
      builders.
    """
    raise NotImplementedError

  def GetFlakyOrFailingTryQuery(self) -> str:
    """
    Returns:
      Query string to get all the failing or flaky results from Try bots.
    """
    raise NotImplementedError

  def GetResultCountCIQuery(self) -> str:
    """
    Returns:
      Query string to get the result count for test/tag combination from CI
      bots.
    """
    raise NotImplementedError

  def GetResultCountTryQuery(self) -> str:
    """
    Returns:
      Query string to get result count for test/tag combination from Try
      bots.
    """
    raise NotImplementedError

  def GetResultCountFromCIBuildersQuery(self, builder_names: List[str]) -> str:
    """
    Returns:
      Query string to get the result count for test/tag combination from input
      CI builders.
    """
    raise NotImplementedError

  def _GetJsonResultsFromBigQuery(self, query: str) -> ct.QueryJsonType:
    """Gets the JSON results from a BigQuery query.

    Automatically passes in the "@sample_period" parameterized argument to
    BigQuery.

    Args:
      query: A string containing the SQL query to run in BigQuery.

    Returns:
      The loaded JSON results from running |query|.
    """
    cmd = GenerateBigQueryCommand(
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
      typ_tags = tuple(tag_utils.TagUtils.RemoveIgnoredTags(r['typ_tags']))
      test_name = r['test_name']
      _, test_name = self._result_processor.GetTestSuiteAndNameFromResultDbName(
          test_name)
      count = int(r['result_count'])
      result_counts[typ_tags][test_name] += count


# TODO(crbug.com/343248818): Switch off this and use the bigquery module
# directly.
def GenerateBigQueryCommand(project: str,
                            parameters: QueryParameters,
                            batch: bool = True) -> List[str]:
  """Generate a BigQuery commandline.

  Does not contain the actual query, as that is passed in via stdin.

  Args:
    project: A string containing the billing project to use for BigQuery.
    parameters: A dict specifying parameters to substitute in the query in
        the format {type: {key: value}}. For example, the dict:
        {'INT64': {'num_builds': 5}}
        would result in --parameter=num_builds:INT64:5 being passed to BigQuery.
    batch: Whether to run the query in batch mode or not. Batching adds some
        random amount of overhead since it means the query has to wait for idle
        resources, but also allows for much better parallelism.

  Returns:
    A list containing the BigQuery commandline, suitable to be passed to a
    method from the subprocess module.
  """
  cmd = [
      'bq',
      'query',
      '--max_rows=%d' % MAX_ROWS,
      '--format=json',
      '--project_id=%s' % project,
      '--use_legacy_sql=false',
  ]

  if batch:
    cmd.append('--batch')

  for parameter_type, parameter_pairs in parameters.items():
    for k, v in parameter_pairs.items():
      cmd.append('--parameter=%s:%s:%s' % (k, parameter_type, v))
  return cmd
