# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with Swarming."""

import collections
import logging
from typing import Dict, List

from bad_machine_finder import bigquery
from bad_machine_finder import tasks
from bad_machine_finder import test_specs

# Given one or more mixin and its associated dimensions, retrieves the total
# and failed task counts for each distinct mixin/bot id/test suite
# combination. Cases where 0 total tasks were run for a particular combination
# are omitted.
SWARMING_TASK_COUNTS_QUERY_TEMPLATE = """\
WITH
  recent_tasks AS (
    SELECT
      bot.bot_id AS bot_id,
      bot.dimensions as dimensions,
      state,
      (
        SELECT
          SUBSTR(t, 6)
        FROM
          result.request.tags t
        WHERE
          STARTS_WITH(t, "name:")
      ) AS test_suite,
      IF(state = "COMPLETED" AND exit_code = 0, TRUE, FALSE) AS task_successful
    FROM
      `chromium-swarm.swarming.task_results_summary` result
    WHERE
      DATE(end_time) >  DATE_SUB(CURRENT_DATE(), INTERVAL {sample_period} DAY)
      AND EXISTS(
        SELECT
          *
        FROM
          result.request.tags t
        WHERE
          t = "bucket:ci"
      )
      AND EXISTS(
        SELECT
          *
        FROM
          result.request.tags t
        WHERE
          t = "realm:chromium:ci"
          OR t = "realm:angle:ci"
      )
      AND state_category IN UNNEST(["CATEGORY_EXECUTION_DONE",
                                    "CATEGORY_TRANSIENT_DONE"])
      AND state IN UNNEST(["COMPLETED",
                           "RAN_INTERNAL_FAILURE",
                           "TIMED_OUT",
                           "TIMED_OUT_SILENCE"])
  ),
{mixin_selector_and_stat_queries}
{combined_stats_query}
SELECT
  *
FROM
  combined_stats
ORDER BY mixin, bot_id, test_suite
"""

# Template for taking rows from the |recent_tasks| subquery and filtering them
# to those that apply to a particular mixin.
MIXIN_TASK_SELECTOR_QUERY_TEMPLATE = """\
  {mixin_name}_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
{dimension_filter}
  ),
"""

# Template for taking rows for a particular mixin subquery and generating
# total/failed task counts for each Swarming bot and test suite combination.
# The mixin name is included to distinguish between different mixins if multiple
# sets of stats are being collected in a single query.
MIXIN_STATS_QUERY_TEMPLATE = """\
  {mixin_name}_stats AS (
    SELECT
      "{mixin_name}" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      {mixin_name}_tasks t
    GROUP BY bot_id, test_suite
  ),
"""


def _GenerateDimensionFilter(dimensions: test_specs.DimensionSet) -> str:
  """Generates the string for |dimension_filter| in
  MIXIN_TASK_SELECTOR_QUERY_TEMPLATE.
  """
  filter_components = []
  for dimension_name, valid_values in dimensions.Pairs():
    value_checkers = []
    for v in valid_values:
      value_checkers.append(f'"{v}" IN UNNEST(dimensions.values)')
    value_check_str = ' OR '.join(value_checkers)
    filter_string = f"""\
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "{dimension_name}"
          AND ({value_check_str})
      )"""
    filter_components.append(filter_string)
  return '\n      AND\n'.join(filter_components)


def _GenerateMixinTaskSelectorQuery(mixin_name: str,
                                    dimensions: test_specs.DimensionSet) -> str:
  """Generates a complete subquery using MIXIN_TASK_SELECTOR_QUERY_TEMPLATE."""
  dimension_filter = _GenerateDimensionFilter(dimensions)
  return MIXIN_TASK_SELECTOR_QUERY_TEMPLATE.format(
      mixin_name=mixin_name, dimension_filter=dimension_filter)


def _GenerateMixinStatsQuery(mixin_name: str) -> str:
  """Generates a complete subquery using MIXIN_STATS_QUERY_TEMPLATE."""
  return MIXIN_STATS_QUERY_TEMPLATE.format(mixin_name=mixin_name)


def _GenerateMixinSelectorAndStatQueries(
    mixin_name: str, dimensions: test_specs.DimensionSet) -> str:
  """Generates the string for |mixin_selector_and_stat_queries| in
  SWARMING_TASK_COUNTS_QUERY_TEMPLATE for a single mixin.
  """
  task_selector_query = _GenerateMixinTaskSelectorQuery(mixin_name, dimensions)
  stats_query = _GenerateMixinStatsQuery(mixin_name)
  return f'{task_selector_query}{stats_query}'


def _GenerateCombinedStatsQuery(mixin_names: List[str]) -> str:
  """Generates the string for |combined_stats_query| in
  SWARMING_TASK_COUNTS_QUERY_TEMPLATE."""
  components = []
  for m in mixin_names:
    mixin_component = f"""\
    SELECT
      *
    FROM
      {m}_stats"""
    components.append(mixin_component)
  union = '\n    UNION ALL\n'.join(components)
  combined_stats_query = f"""\
  combined_stats AS (
{union}
  )"""
  return combined_stats_query


def _GenerateQuery(dimensions_by_mixin: Dict[str, test_specs.DimensionSet],
                   sample_period: int) -> str:
  """Generates a complete query using SWARMING_TASK_COUNTS_QUERY_TEMPLATE."""
  combined_stats_query = _GenerateCombinedStatsQuery(
      list(dimensions_by_mixin.keys()))
  mixin_queries = []
  for mixin_name in sorted(list(dimensions_by_mixin.keys())):
    dimensions = dimensions_by_mixin[mixin_name]
    mixin_queries.append(
        _GenerateMixinSelectorAndStatQueries(mixin_name, dimensions))
  mixin_selector_and_stat_queries = ''.join(mixin_queries)
  # Remove the trailing newline.
  mixin_selector_and_stat_queries = mixin_selector_and_stat_queries.rstrip()
  return SWARMING_TASK_COUNTS_QUERY_TEMPLATE.format(
      mixin_selector_and_stat_queries=mixin_selector_and_stat_queries,
      combined_stats_query=combined_stats_query,
      sample_period=sample_period)


def GetTaskStatsForMixins(querier: bigquery.Querier,
                          dimensions_by_mixin: Dict[str,
                                                    test_specs.DimensionSet],
                          sample_period: int) -> Dict[str, tasks.MixinStats]:
  """Queries BigQuery for total/failed task counts.

  Args:
    querier: A bigquery.Querier instance to use when actually running queries.
    dimensions_by_mixin: A dict mapping mixin names to their corresponding
        dimension key/value pairs.
    sample_period: How many days of data to query.

  Returns:
    A dict mapping mixin names to tasks.MixinStats objects containing the
    queried task stats for that mixin. The keys are guaranteed to be a subset of
    the keys from |dimensions_by_mixin|, but may not be identical if no data
    is found for one or more mixins.
  """
  mixin_stats = collections.defaultdict(tasks.MixinStats)
  query = _GenerateQuery(dimensions_by_mixin, sample_period)
  for row in querier.GetSeriesForQuery(query):
    if not row.test_suite:
      logging.warning(
          'Skipping row with %d total tasks and %d failed tasks that did not '
          'have a test suite set. This is normal if these tasks were manually '
          'triggered.', row.total_tasks, row.failed_tasks)
      continue
    mixin_stats[row.mixin].AddStatsForBotAndSuite(row.bot_id, row.test_suite,
                                                  row.total_tasks,
                                                  row.failed_tasks)
  for stats in mixin_stats.values():
    stats.Freeze()
  return mixin_stats
