# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from typing import Generator
import unittest

from bad_machine_finder import bigquery
from bad_machine_finder import swarming
from bad_machine_finder import test_specs

import pandas  # pylint: disable=import-error

FakeRow = collections.namedtuple(
    'FakeRow', ['mixin', 'bot_id', 'total_tasks', 'failed_tasks', 'test_suite'])


class FakeQuerier(bigquery.Querier):

  def __init__(self):
    super().__init__('')
    self.rows = []
    self.last_run_query = ''

  def GetSeriesForQuery(self,
                        query: str) -> Generator[pandas.Series, None, None]:
    self.last_run_query = query
    for r in self.rows:
      yield r


class QueryParsingUnittest(unittest.TestCase):

  def testSingleMixin(self):
    """Tests query result parsing behavior with a single mixin."""
    dimensions = test_specs.DimensionSet({'os': 'mac'})
    dimensions_by_mixin = {'mixin_name': dimensions}

    querier = FakeQuerier()
    querier.rows = [
        FakeRow('mixin_name', 'bot-1', 10, 5, 'pixel'),
        FakeRow('mixin_name', 'bot-1', 10, 0, 'webgl'),
        FakeRow('mixin_name', 'bot-2', 20, 10, 'pixel'),
    ]

    all_mixin_stats = swarming.GetTaskStatsForMixins(querier,
                                                     dimensions_by_mixin, 5)
    self.assertEqual(list(all_mixin_stats.keys()), ['mixin_name'])
    mixin_stats = all_mixin_stats['mixin_name']
    self.assertEqual(mixin_stats.total_tasks, 40)
    self.assertEqual(mixin_stats.failed_tasks, 15)
    for bot_id, bot_stats in mixin_stats.IterBots():
      self.assertIn(bot_id, ('bot-1', 'bot-2'))
      if bot_id == 'bot-1':
        self.assertEqual(bot_stats.total_tasks, 20)
        self.assertEqual(bot_stats.failed_tasks, 5)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('pixel'), 10)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('pixel'), 5)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('webgl'), 10)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('webgl'), 0)
      else:
        self.assertEqual(bot_stats.total_tasks, 20)
        self.assertEqual(bot_stats.failed_tasks, 10)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('pixel'), 20)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('pixel'), 10)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('webgl'), 0)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('webgl'), 0)

  def testMultiMixin(self):
    """Tests query result parsing behavior with multiple mixins."""
    dimensions_by_mixin = {
        'amd_mixin': test_specs.DimensionSet({'gpu': '1002'}),
        'nvidia_mixin': test_specs.DimensionSet({'gpu': '10de'}),
    }

    querier = FakeQuerier()
    querier.rows = [
        FakeRow('amd_mixin', 'bot-1', 10, 5, 'pixel'),
        FakeRow('nvidia_mixin', 'bot-2', 20, 10, 'webgl'),
    ]

    all_mixin_stats = swarming.GetTaskStatsForMixins(querier,
                                                     dimensions_by_mixin, 5)
    self.assertEqual(set(all_mixin_stats.keys()), {'amd_mixin', 'nvidia_mixin'})

    amd_mixin_stats = all_mixin_stats['amd_mixin']
    self.assertEqual(amd_mixin_stats.total_tasks, 10)
    self.assertEqual(amd_mixin_stats.failed_tasks, 5)

    nvidia_mixin_stats = all_mixin_stats['nvidia_mixin']
    self.assertEqual(nvidia_mixin_stats.total_tasks, 20)
    self.assertEqual(nvidia_mixin_stats.failed_tasks, 10)

  def testNoTestSuiteRow(self):
    """Tests behavior when a row without a test suite is encountered."""
    dimensions = test_specs.DimensionSet({'os': 'mac'})
    dimensions_by_mixin = {'mixin_name': dimensions}

    querier = FakeQuerier()
    querier.rows = [
        FakeRow('mixin_name', 'bot-1', 10, 5, 'pixel'),
        FakeRow('mixin_name', 'bot-1', 20, 10, None),
    ]

    with self.assertLogs(level='WARNING') as log_manager:
      all_mixin_stats = swarming.GetTaskStatsForMixins(querier,
                                                       dimensions_by_mixin, 5)
      for line in log_manager.output:
        if ('Skipping row with 20 total tasks and 10 failed tasks that did not '
            'have a test suite set. This is normal if these tasks were '
            'manually triggered.') in line:
          break
      else:
        self.fail('Expected log line not found')

    self.assertEqual(list(all_mixin_stats.keys()), ['mixin_name'])
    mixin_stats = all_mixin_stats['mixin_name']
    self.assertEqual(mixin_stats.total_tasks, 10)
    self.assertEqual(mixin_stats.failed_tasks, 5)


class GenerateQueryUnittest(unittest.TestCase):

  def testSingleMixinSimpleDimensions(self):
    """Tests behavior when a single mixin is provided with simple dimensions."""
    dimensions = test_specs.DimensionSet({'os': 'mac', 'gpu': '1002'})
    dimensions_by_mixin = {'mixin_name': dimensions}
    querier = FakeQuerier()

    expected_query = """\
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
      DATE(end_time) >  DATE_SUB(CURRENT_DATE(), INTERVAL 5 DAY)
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
  mixin_name_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "gpu"
          AND ("1002" IN UNNEST(dimensions.values))
      )
      AND
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "os"
          AND ("mac" IN UNNEST(dimensions.values))
      )
  ),
  mixin_name_stats AS (
    SELECT
      "mixin_name" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      mixin_name_tasks t
    GROUP BY bot_id, test_suite
  ),
  combined_stats AS (
    SELECT
      *
    FROM
      mixin_name_stats
  )
SELECT
  *
FROM
  combined_stats
ORDER BY mixin, bot_id, test_suite
"""

    _ = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin, 5)
    self.assertEqual(querier.last_run_query, expected_query)

  def testSingleMixinComplexDimensions(self):
    """Tests behavior when a single mixin is provided w/ complex dimensions."""
    dimensions = test_specs.DimensionSet({
        'os': 'mac',
        'gpu': '1002:2345|1002:3456'
    })
    dimensions_by_mixin = {'mixin_name': dimensions}
    querier = FakeQuerier()

    # pylint: disable=line-too-long
    expected_query = """\
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
      DATE(end_time) >  DATE_SUB(CURRENT_DATE(), INTERVAL 5 DAY)
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
  mixin_name_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "gpu"
          AND ("1002:2345" IN UNNEST(dimensions.values) OR "1002:3456" IN UNNEST(dimensions.values))
      )
      AND
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "os"
          AND ("mac" IN UNNEST(dimensions.values))
      )
  ),
  mixin_name_stats AS (
    SELECT
      "mixin_name" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      mixin_name_tasks t
    GROUP BY bot_id, test_suite
  ),
  combined_stats AS (
    SELECT
      *
    FROM
      mixin_name_stats
  )
SELECT
  *
FROM
  combined_stats
ORDER BY mixin, bot_id, test_suite
"""
    # pylint: enable=line-too-long

    _ = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin, 5)
    self.assertEqual(querier.last_run_query, expected_query)

  def testMultipleMixinsSimpleDimensions(self):
    """Tests behavior when multiple mixins are provided w/ simple dimensions."""
    amd_dimensions = test_specs.DimensionSet({'os': 'mac', 'gpu': '1002'})
    nvidia_dimensions = test_specs.DimensionSet({'os': 'mac', 'gpu': '10de'})
    dimensions_by_mixin = {
        'amd_mixin': amd_dimensions,
        'nvidia_mixin': nvidia_dimensions
    }
    querier = FakeQuerier()

    expected_query = """\
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
      DATE(end_time) >  DATE_SUB(CURRENT_DATE(), INTERVAL 5 DAY)
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
  amd_mixin_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "gpu"
          AND ("1002" IN UNNEST(dimensions.values))
      )
      AND
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "os"
          AND ("mac" IN UNNEST(dimensions.values))
      )
  ),
  amd_mixin_stats AS (
    SELECT
      "amd_mixin" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      amd_mixin_tasks t
    GROUP BY bot_id, test_suite
  ),
  nvidia_mixin_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "gpu"
          AND ("10de" IN UNNEST(dimensions.values))
      )
      AND
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "os"
          AND ("mac" IN UNNEST(dimensions.values))
      )
  ),
  nvidia_mixin_stats AS (
    SELECT
      "nvidia_mixin" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      nvidia_mixin_tasks t
    GROUP BY bot_id, test_suite
  ),
  combined_stats AS (
    SELECT
      *
    FROM
      amd_mixin_stats
    UNION ALL
    SELECT
      *
    FROM
      nvidia_mixin_stats
  )
SELECT
  *
FROM
  combined_stats
ORDER BY mixin, bot_id, test_suite
"""

    _ = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin, 5)
    self.assertEqual(querier.last_run_query, expected_query)

  def testMultipleMixinsComplexDimensions(self):
    """Tests behavior w/ multiple mixins are provided w/ complex dimensions."""
    amd_dimensions = test_specs.DimensionSet({
        'os': 'mac',
        'gpu': '1002:2345|1002:3456'
    })
    nvidia_dimensions = test_specs.DimensionSet({
        'os': 'mac|win',
        'gpu': '10de'
    })
    dimensions_by_mixin = {
        'amd_mixin': amd_dimensions,
        'nvidia_mixin': nvidia_dimensions
    }
    querier = FakeQuerier()

    # pylint: disable=line-too-long
    expected_query = """\
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
      DATE(end_time) >  DATE_SUB(CURRENT_DATE(), INTERVAL 5 DAY)
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
  amd_mixin_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "gpu"
          AND ("1002:2345" IN UNNEST(dimensions.values) OR "1002:3456" IN UNNEST(dimensions.values))
      )
      AND
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "os"
          AND ("mac" IN UNNEST(dimensions.values))
      )
  ),
  amd_mixin_stats AS (
    SELECT
      "amd_mixin" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      amd_mixin_tasks t
    GROUP BY bot_id, test_suite
  ),
  nvidia_mixin_tasks AS (
    SELECT
      *
    FROM
      recent_tasks r
    WHERE
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "gpu"
          AND ("10de" IN UNNEST(dimensions.values))
      )
      AND
      EXISTS(
        SELECT
          *
        FROM
          r.dimensions
        WHERE
          dimensions.key = "os"
          AND ("mac" IN UNNEST(dimensions.values) OR "win" IN UNNEST(dimensions.values))
      )
  ),
  nvidia_mixin_stats AS (
    SELECT
      "nvidia_mixin" as mixin,
      bot_id,
      COUNT(bot_id) as total_tasks,
      COUNT(IF(task_successful = False, bot_id, NULL)) AS failed_tasks,
      test_suite
    FROM
      nvidia_mixin_tasks t
    GROUP BY bot_id, test_suite
  ),
  combined_stats AS (
    SELECT
      *
    FROM
      amd_mixin_stats
    UNION ALL
    SELECT
      *
    FROM
      nvidia_mixin_stats
  )
SELECT
  *
FROM
  combined_stats
ORDER BY mixin, bot_id, test_suite
"""
    # pylint: enable=line-too-long

    _ = swarming.GetTaskStatsForMixins(querier, dimensions_by_mixin, 5)
    self.assertEqual(querier.last_run_query, expected_query)
