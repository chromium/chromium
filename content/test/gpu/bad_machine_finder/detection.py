# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for detecting bad machines from Swarming task data."""

import collections
import decimal
import functools
import logging
import math
import statistics
from typing import Generator, Iterable, Optional, Set, Tuple

from bad_machine_finder import tasks


class BadMachineList:
  """Stores bad Swarming bots and the reasons for why they were deemed bad."""

  def __init__(self):
    self.bad_machines = collections.defaultdict(list)

  def AddBadMachine(self, bot_id: str, reason: str) -> None:
    """Adds a bad machine to the list with the given reason.

    Args:
      bot_id: The bad machine name
      reason: The reason why the machine was deemed bad
    """
    self.bad_machines[bot_id].append(reason)

  def Merge(self, other: 'BadMachineList') -> None:
    """Merges another BadMachineList into this one.

    Args:
      other: The BadMachineList to get data from.
    """
    for bot_id, reasons in other.bad_machines.items():
      self.bad_machines[bot_id].extend(reasons)

  def RemoveLowConfidenceMachines(self, num_detections: int) -> None:
    """Removes machines that weren't flagged at least |num_detections| times.

    Args:
      num_detections: The minimum number times a machine needs to be flagged as
          bad in order to be kept.
    """
    trimmed_machines = collections.defaultdict(list)
    for bot_id, reasons in self.bad_machines.items():
      if len(reasons) >= num_detections:
        trimmed_machines[bot_id] = reasons
      else:
        logging.debug(
            'Bot %s removed because it was only flagged by %d detection '
            'method(s)', bot_id, len(reasons))
    self.bad_machines = trimmed_machines

  def IterMarkdown(self) -> Generator[Tuple[str, str], None, None]:
    """Iterates over contents, producing Markdown for each machine.

    Yields:
      A tuple (bot_id, markdown). |bot_id| is the ID of the Swarming bot,
      |markdown| is a Markdown string describing why the bot is bad.
    """
    # Keep a consistent order.
    bot_ids = sorted(list(self.bad_machines.keys()))
    for b in bot_ids:
      markdown_components = []
      markdown_components.append(f'  * {b}')
      reasons = self.bad_machines[b]
      for r in reasons:
        markdown_components.append(f'    * {r}')
      yield b, '\n'.join(markdown_components)


class MixinGroupedBadMachines:
  """Stores BadMachineLists for multiple mixins."""

  def __init__(self):
    self._bad_machines_by_mixin = {}

  def AddMixinData(self, mixin_name: str,
                   bad_machine_list: 'BadMachineList') -> None:
    """Adds a BadMachineList for |mixin_name|.

    Args:
      mixin_name: The name of the mixin
      bad_machine_list: The BadMachineList for the mixin
    """
    if mixin_name in self._bad_machines_by_mixin:
      raise ValueError(
          f'Bad machines for mixin {mixin_name} were already added')
    self._bad_machines_by_mixin[mixin_name] = bad_machine_list

  def GetAllBadMachineNames(self) -> Set[str]:
    """Gets all unique bad machine names across all stored mixins.

    Returns:
      A set containing all bad machine names.
    """
    bad_machine_names = set()
    for _, bad_machine_list in self._bad_machines_by_mixin.items():
      for bot_id in bad_machine_list.bad_machines.keys():
        bad_machine_names.add(bot_id)
    return bad_machine_names

  def GenerateMarkdown(self,
                       bots_to_skip: Optional[Iterable[str]] = None) -> str:
    """Generates a Markdown string describing the object's contents.

    Args:
      bots_to_skip: An optional iterable of bot names to not include in the
          Markdown content. If not provided, all bots will be included.

    Returns:
      A Markdown string describing each bad machine for each mixin.
    """
    bots_to_skip = bots_to_skip or []
    markdown_components = []
    # Guarantee a consistent ordering.
    for mixin_name in sorted(self._bad_machines_by_mixin.keys()):
      bad_machine_list = self._bad_machines_by_mixin[mixin_name]
      mixin_report_components = [
          f'Bad machines for {mixin_name}',
      ]
      for bot_id, markdown in bad_machine_list.IterMarkdown():
        if bot_id in bots_to_skip:
          continue
        mixin_report_components.append(markdown)
      if len(mixin_report_components) > 1:
        markdown_components.append('\n'.join(mixin_report_components))

    return '\n\n'.join(markdown_components)


def DetectViaStdDevOutlier(mixin_stats: tasks.MixinStats,
                           stddev_multiplier: float,
                           min_failures: int) -> 'BadMachineList':
  """Detects bad machines by looking for machines whose task failure rate is
  more than the given number of standard deviations from the fleet-wide mean.

  Args:
    mixin_stats: A tasks.MixinStats containing the task stats for the hardware
        fleet being checked.
    stddev_multiplier: A multiplier to apply to the standard deviation. If a
        machine's failure rate is greater than (fleet-wide mean) + (stddev *
        stddev_multiplier), it is considered bad.
    min_failures: The minimum number of failures a bot must have in order to be
        reported. This helps avoid cases where healthy bots are erroneously
        reported due to getting a small number of random failures in a small
        number of total tasks.

  Returns:
    A BadMachineList containing any bad machines found.
  """
  if mixin_stats.total_tasks <= 0:
    raise ValueError('mixin_stats needs to contain data')
  if stddev_multiplier < 0:
    raise ValueError('stddev_multiplier must be non-negative')

  bad_machines = BadMachineList()

  failure_rates = mixin_stats.GetOverallFailureRates()
  mean = statistics.mean(failure_rates)
  stddev = statistics.pstdev(failure_rates)
  threshold = mean + stddev_multiplier * stddev

  for bot_id, bot_stats in mixin_stats.IterBots():
    bot_failure_rate = bot_stats.overall_failure_rate
    if bot_failure_rate <= threshold:
      continue
    if bot_stats.failed_tasks < min_failures:
      logging.debug(
          'Bot %s skipped in DetectViaStdDevOutlier due to only having %d '
          'failed tasks', bot_id, bot_stats.failed_tasks)
      continue
    reason = (f'Had a failure rate of {bot_failure_rate} despite a fleet-wide '
              f'average of {mean} and a standard deviation of {stddev}.')
    bad_machines.AddBadMachine(bot_id, reason)

  return bad_machines


def DetectViaRandomChance(mixin_stats: tasks.MixinStats,
                          probability_threshold: float) -> 'BadMachineList':
  """Detects bad machines by looking for cases where it is very unlikely that
  a machine got as many failed tasks as it did through random chance. If it is
  unlikely that the failures happened due to random chance, then that means that
  the machine is bad and contributing to failures.

  Args:
    mixin_stats: A tasks.MixinStats containing the task stats for the hardware
        fleet being checked.
    probability_threshold: How unlikely it can be that a machine got its
        failures randomly and still be considered good.

  Returns:
    A BadMachineList containing any bad machines found.
  """
  if mixin_stats.total_tasks <= 0:
    raise ValueError('mixin_stats needs to contain data')
  if probability_threshold <= 0:
    raise ValueError('probability_threshold must be positive')
  if probability_threshold > 1:
    raise ValueError('probability_threshold must be <= 1')

  bad_machines = BadMachineList()

  average_failure_rate = (decimal.Decimal(mixin_stats.failed_tasks) /
                          decimal.Decimal(mixin_stats.total_tasks))
  for bot_id, bot_stats in mixin_stats.IterBots():
    p = _ChanceOfNOrMoreIndependentEvents(average_failure_rate,
                                          bot_stats.total_tasks,
                                          bot_stats.failed_tasks)
    if p >= probability_threshold:
      continue
    reason = (f'{bot_stats.failed_tasks} of {bot_stats.total_tasks} tasks '
              f'failed despite a fleet-wide average failed task rate of '
              f'{average_failure_rate}. The probability of this happening '
              f'randomly is {p}.')
    bad_machines.AddBadMachine(bot_id, reason)

  return bad_machines


def DetectViaInterquartileRange(mixin_stats: tasks.MixinStats, mixin_name: str,
                                iqr_multiplier: float,
                                min_failures: int) -> 'BadMachineList':
  """Detects bad machines by looking for for bots whose failure rate is above
  Q3 + |iqr_multiplier| * IQR, which is a standard way of looking for outliers
  in data.

  See https://en.wikipedia.org/wiki/Interquartile_range#Outliers.

  Args:
    mixin_stats: A tasks.MixinStats containing the task stats for the hardware
        fleet being checked.
    mixin_name: The name of the mixin being checked. For debugging purposes.
    iqr_multiplier: How many multiples of the IQR above the third quartile a
        failure rate has to be to be considered an outlier.
    min_failures: The minimum number of failures a bot must have in order to be
        reported. This helps avoid cases where healthy bots are erroneously
        reported due to getting a small number of random failures in a small
        number of total tasks.

  Returns:
    A BadMachineList containing any bad machines found.
  """
  if mixin_stats.total_tasks <= 0:
    raise ValueError('mixin_stats needs to contain data')
  if iqr_multiplier <= 0:
    raise ValueError('iqr_multiplier must be positive')

  bad_machines = BadMachineList()
  failure_rates = mixin_stats.GetOverallFailureRates()
  if len(failure_rates) <= 4:
    logging.info(
        'Quartiles require at least 5 samples to be meaningful. Mixin %s only '
        'provided %d samples.', mixin_name, len(failure_rates))
    return bad_machines

  quartiles = statistics.quantiles(failure_rates, n=4, method='inclusive')
  iqr = quartiles[2] - quartiles[0]

  if iqr == 0:
    logging.info(
        'Mixin %s resulted in an IQR of 0, which is not useful for detecting '
        'outliers.', mixin_name)
    return bad_machines

  upper_bound = quartiles[2] + iqr_multiplier * iqr

  for bot_id, bot_stats in mixin_stats.IterBots():
    bot_failure_rate = bot_stats.overall_failure_rate
    if bot_failure_rate <= upper_bound:
      continue
    if bot_stats.failed_tasks < min_failures:
      logging.debug(
          'Bot %s skipped in DetectViaInterquartileRange due to only having %d '
          'failed tasks', bot_id, bot_stats.failed_tasks)
      continue
    reason = (f'Failure rate of {bot_failure_rate} is above the IQR-based '
              f'upper bound of {upper_bound}.')
    bad_machines.AddBadMachine(bot_id, reason)

  return bad_machines


@functools.lru_cache(maxsize=None)
def _ChanceOfNOrMoreIndependentEvents(event_probability: decimal.Decimal,
                                      total_events: int, n: int) -> float:
  """Calculates the probability of getting |n| or more cases of independent
  events.

  Args:
    event_probability: The probability of the event happening.
    total_events: The number of times we check whether the event happened or
        not.
    n: The minimum number of times the event happened.
  """
  cumulative_probability = decimal.Decimal(0)
  for current_n in range(n, total_events + 1):
    cumulative_probability += _ChanceOfExactlyNIndependentEvents(
        event_probability, total_events, current_n)
  return float(cumulative_probability)


@functools.lru_cache(maxsize=None)
def _ChanceOfExactlyNIndependentEvents(event_probability: decimal.Decimal,
                                       total_events: int,
                                       n: int) -> decimal.Decimal:
  """Calculates the probability of getting exactly |n| cases of independent
  events.

  Args:
    event_probability: The probability of the event happening.
    total_events: The number of times we check whether the event happened or
        not.
    n: The number of times the event happened.

  Returns:
    A decimal.Decimal object containing the chance that the event happened
    exactly |n| times.
  """
  # We use decimal.Decimal instead of float since math.comb() can produce
  # numbers that are too large to store in a float.
  combinations = decimal.Decimal(math.comb(total_events, n))
  chance_of_one_permutation = (event_probability**n *
                               (1 - event_probability)**(total_events - n))
  return combinations * chance_of_one_permutation
