# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from bad_machine_finder import tasks


class BotStatsUnittest(unittest.TestCase):

  def testInputValidation(self):
    """Tests that inputs are properly validated."""
    bot_stats = tasks.BotStats()
    with self.assertRaisesRegex(ValueError, 'total_tasks must be positive'):
      bot_stats.AddStatsForSuite('suite', 0, 0)
    with self.assertRaisesRegex(ValueError,
                                'failed_tasks must be non-negative'):
      bot_stats.AddStatsForSuite('suite', 1, -1)
    with self.assertRaisesRegex(ValueError,
                                'total_tasks must be >= failed_tasks'):
      bot_stats.AddStatsForSuite('suite', 5, 10)
    bot_stats.AddStatsForSuite('suite_name', 10, 5)
    with self.assertRaisesRegex(
        ValueError, 'Stats for test suite suite_name were already provided .*'):
      bot_stats.AddStatsForSuite('suite_name', 5, 0)

  def testOnlyReadableWhenFrozen(self):
    """Tests that data is only readable once the object is frozen."""
    bot_stats = tasks.BotStats()
    with self.assertRaises(AssertionError):
      _ = bot_stats.total_tasks
    with self.assertRaises(AssertionError):
      _ = bot_stats.failed_tasks
    with self.assertRaises(AssertionError):
      _ = bot_stats.overall_failure_rate
    with self.assertRaises(AssertionError):
      bot_stats.GetTotalTasksForSuite('suite')
    with self.assertRaises(AssertionError):
      bot_stats.GetFailedTasksForSuite('suite')

  def testOnlyWritableWhenNotFrozen(self):
    """Tests that data is only writable when the object is not frozen."""
    bot_stats = tasks.BotStats()
    bot_stats.Freeze()
    with self.assertRaises(AssertionError):
      bot_stats.AddStatsForSuite('suite', 10, 5)

  def testStatTracking(self):
    """Tests that the tracked stats are correct."""
    bot_stats = tasks.BotStats()
    bot_stats.AddStatsForSuite('pixel', 10, 5)
    bot_stats.AddStatsForSuite('webgl', 20, 0)
    bot_stats.Freeze()

    self.assertEqual(bot_stats.total_tasks, 30)
    self.assertEqual(bot_stats.failed_tasks, 5)
    self.assertEqual(bot_stats.overall_failure_rate, float(5) / 30)
    self.assertEqual(bot_stats.GetTotalTasksForSuite('pixel'), 10)
    self.assertEqual(bot_stats.GetFailedTasksForSuite('pixel'), 5)
    self.assertEqual(bot_stats.GetTotalTasksForSuite('webgl'), 20)
    self.assertEqual(bot_stats.GetFailedTasksForSuite('webgl'), 0)
    self.assertEqual(bot_stats.GetTotalTasksForSuite('non-existent'), 0)
    self.assertEqual(bot_stats.GetFailedTasksForSuite('non-existent'), 0)


class MixinStatsUnittest(unittest.TestCase):

  def testInputValidation(self):
    """Tests that inputs are properly validated."""
    mixin_stats = tasks.MixinStats()
    with self.assertRaisesRegex(ValueError, 'total_tasks must be positive'):
      mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 0, 0)
    with self.assertRaisesRegex(ValueError,
                                'failed_tasks must be non-negative'):
      mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 10, -1)

  def testOnlyReadableWhenFrozen(self):
    """Tests that data is only readable once the object is frozen."""
    mixin_stats = tasks.MixinStats()
    with self.assertRaises(AssertionError):
      _ = mixin_stats.total_tasks
    with self.assertRaises(AssertionError):
      _ = mixin_stats.failed_tasks
    with self.assertRaises(AssertionError):
      list(mixin_stats.IterBots())
    with self.assertRaises(AssertionError):
      mixin_stats.GetOverallFailureRates()

  def testOnlyWritableWhenNotFrozen(self):
    """Tests that data is only writable when the object is not frozen."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.Freeze()
    with self.assertRaises(AssertionError):
      mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 10, 0)

  def testStatTracking(self):
    """Tests that the tracked stats are correct."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot-1', 'suite-1', 10, 0)
    mixin_stats.AddStatsForBotAndSuite('bot-1', 'suite-2', 20, 5)
    mixin_stats.AddStatsForBotAndSuite('bot-2', 'suite-1', 30, 30)
    mixin_stats.AddStatsForBotAndSuite('bot-2', 'suite-2', 10, 5)
    mixin_stats.Freeze()

    self.assertEqual(mixin_stats.total_tasks, 70)
    self.assertEqual(mixin_stats.failed_tasks, 40)
    failure_rates = mixin_stats.GetOverallFailureRates()
    self.assertEqual(len(failure_rates), 2)
    self.assertEqual(set(failure_rates), {float(5) / 30, float(35) / 40})
    for bot_id, bot_stats in mixin_stats.IterBots():
      self.assertIn(bot_id, ('bot-1', 'bot-2'))
      if bot_id == 'bot-1':
        self.assertEqual(bot_stats.total_tasks, 30)
        self.assertEqual(bot_stats.failed_tasks, 5)
        self.assertEqual(bot_stats.overall_failure_rate, float(5) / 30)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('suite-1'), 10)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('suite-1'), 0)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('suite-2'), 20)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('suite-2'), 5)
      else:
        self.assertEqual(bot_stats.total_tasks, 40)
        self.assertEqual(bot_stats.failed_tasks, 35)
        self.assertEqual(bot_stats.overall_failure_rate, float(35) / 40)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('suite-1'), 30)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('suite-1'), 30)
        self.assertEqual(bot_stats.GetTotalTasksForSuite('suite-2'), 10)
        self.assertEqual(bot_stats.GetFailedTasksForSuite('suite-2'), 5)
