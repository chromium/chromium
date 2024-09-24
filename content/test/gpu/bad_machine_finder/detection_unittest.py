# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import decimal
import unittest

from bad_machine_finder import detection
from bad_machine_finder import tasks


class BadMachineListUnittest(unittest.TestCase):

  def testBasic(self):
    """Tests basic functionality of the class."""
    first_list = detection.BadMachineList()
    first_list.AddBadMachine('bot-1', 'reason-1')
    first_list.AddBadMachine('bot-2', 'reason-2')

    second_list = detection.BadMachineList()
    second_list.AddBadMachine('bot-2', 'reason-3')
    second_list.AddBadMachine('bot-2', 'reason-4')

    first_list.Merge(second_list)
    expected_bad_machines = {
        'bot-1': [
            'reason-1',
        ],
        'bot-2': [
            'reason-2',
            'reason-3',
            'reason-4',
        ],
    }

    self.assertEqual(first_list.bad_machines, expected_bad_machines)


class DetectViaStdDevOutlierUnittest(unittest.TestCase):

  def testInputChecking(self):
    """Tests that invalid inputs are checked."""
    # No tasks.
    mixin_stats = tasks.MixinStats()
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaStdDevOutlier(mixin_stats, 2)

    # Negative threshold
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 1, 0)
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaStdDevOutlier(mixin_stats, -1)

  def testSmallGoodFleet(self):
    mixin_stats = tasks.MixinStats()
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2)
    self.assertEqual(bad_machine_list.bad_machines, {})

  def testOneClearlyBadMachineSmallFleet(self):
    """Tests behavior when there is a single clearly bad machine."""
    # We need enough samples that the mean is sufficiently skewed towards the
    # good bots' failure rate for this detection method to work.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 99)
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, 1)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2)
    expected_bad_machines = {
        'bad-bot': [('Had a failure rate of 0.99 despite a fleet-wide average '
                     'of 0.09909090909090909 and a standard deviation of '
                     '0.2817301915422738.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSeveralBadMachinesLargeFleet(self):
    """Tests behavior when there are several bad machines in a large fleet."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, 1)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 100, 15)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 20)
    mixin_stats.AddStatsForBotAndSuite('bot-100', 'suite', 100, 50)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2)
    expected_bad_machines = {
        'bot-98': [('Had a failure rate of 0.15 despite a fleet-wide average '
                    'of 0.018118811881188118 and a standard deviation of '
                    '0.05350511905346074.')],
        'bot-99': [('Had a failure rate of 0.2 despite a fleet-wide average '
                    'of 0.018118811881188118 and a standard deviation of '
                    '0.05350511905346074.')],
        'bot-100': [('Had a failure rate of 0.5 despite a fleet-wide average '
                     'of 0.018118811881188118 and a standard deviation of '
                     '0.05350511905346074.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSmallFlakyFleet(self):
    """Tests behavior when there's a bad machine in a small, flaky fleet."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 50)
    for i in range(9):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, 25)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaStdDevOutlier(mixin_stats, 2)
    expected_bad_machines = {
        'bad-bot': [('Had a failure rate of 0.5 despite a fleet-wide average '
                     'of 0.275 and a standard deviation of 0.075.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)


class DetectViaRandomChanceUnittest(unittest.TestCase):

  def testInputChecking(self):
    """Tests that invalid inputs are checked."""
    # No tasks.
    mixin_stats = tasks.MixinStats()
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaRandomChance(mixin_stats, 0.005)

    # Non-positive probability.
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bot', 'suite', 1, 0)
    mixin_stats.Freeze()
    with self.assertRaises(ValueError):
      detection.DetectViaRandomChance(mixin_stats, 0)

    # >1 probability
    with self.assertRaises(ValueError):
      detection.DetectViaRandomChance(mixin_stats, 1.1)

  def testSmallGoodFleet(self):
    """Tests behavior when there are no bad machines."""
    mixin_stats = tasks.MixinStats()
    for i in range(10):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, i % 5)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    self.assertEqual(bad_machine_list.bad_machines, {})

  def testOneClearlyBadMachineSmallFleet(self):
    """Tests behavior when there is a single clearly bad machine."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 99)
    mixin_stats.AddStatsForBotAndSuite('good-bot', 'suite', 100, 1)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    expected_bad_machines = {
        'bad-bot': [('99 of 100 tasks failed despite a fleet-wide average '
                     'failed task rate of 0.5. The probability of this '
                     'happening randomly is 7.967495142732219e-29.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSeveralBadMachinesLargeFleet(self):
    """Tests behavior when there are several bad machines in a large fleet."""
    mixin_stats = tasks.MixinStats()
    for i in range(98):
      mixin_stats.AddStatsForBotAndSuite(f'bot-{i}', 'suite', 100, 1)
    mixin_stats.AddStatsForBotAndSuite('bot-98', 'suite', 100, 15)
    mixin_stats.AddStatsForBotAndSuite('bot-99', 'suite', 100, 20)
    mixin_stats.AddStatsForBotAndSuite('bot-100', 'suite', 100, 50)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    expected_bad_machines = {
        'bot-98': [('15 of 100 tasks failed despite a fleet-wide average '
                    'failed task rate of 0.01811881188118811881188118812. '
                    'The probability of this happening randomly is '
                    '4.41689373707857e-10.')],
        'bot-99': [('20 of 100 tasks failed despite a fleet-wide average '
                    'failed task rate of 0.01811881188118811881188118812. The '
                    'probability of this happening randomly is '
                    '1.9407812867119233e-15.')],
        'bot-100': [('50 of 100 tasks failed despite a fleet-wide average '
                     'failed task rate of 0.01811881188118811881188118812. The '
                     'probability of this happening randomly is '
                     '3.3205488374477226e-59.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)

  def testSmallFlakyFleet(self):
    """Tests behavior when there's a bad machine in a small, flaky fleet."""
    mixin_stats = tasks.MixinStats()
    mixin_stats.AddStatsForBotAndSuite('bad-bot', 'suite', 100, 50)
    for i in range(9):
      mixin_stats.AddStatsForBotAndSuite(f'good-bot-{i}', 'suite', 100, 25)
    mixin_stats.Freeze()

    bad_machine_list = detection.DetectViaRandomChance(mixin_stats, 0.005)
    expected_bad_machines = {
        'bad-bot': [('50 of 100 tasks failed despite a fleet-wide average '
                     'failed task rate of 0.275. The probability of this '
                     'happening randomly is 1.5273539960703075e-06.')],
    }
    self.assertEqual(bad_machine_list.bad_machines, expected_bad_machines)


class IndependentEventHelpersUnittest(unittest.TestCase):

  def testChanceOfExactlyNIndependentEvents(self):
    """Tests behavior of the N independent events helper."""
    # pylint: disable=protected-access
    func = detection._ChanceOfExactlyNIndependentEvents
    # pylint: enable=protected-access

    # Equivalent to flipping a coin and getting heads.
    self.assertEqual(func(decimal.Decimal(0.5), 1, 1), decimal.Decimal(0.5))

    # Equivalent to flipping two coins and getting zero heads.
    self.assertEqual(func(decimal.Decimal(0.5), 2, 0), decimal.Decimal(0.25))

  def testChanceOfNOrMoreIndependentEvents(self):
    """Tests behavior of the N+ independent events helper."""
    # pylint: disable=protected-access
    func = detection._ChanceOfNOrMoreIndependentEvents
    # pylint: enable=protected-access

    # Probability of getting 0 or more should always be 1.
    self.assertEqual(func(decimal.Decimal(0.5), 10, 0), 1)

    # Equivalent to flipping two coins and getting at least one heads.
    self.assertEqual(func(decimal.Decimal(0.5), 2, 1), 0.75)
