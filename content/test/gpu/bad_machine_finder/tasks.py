# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Datatypes for locally storing Swarming task data."""

import collections
import functools
from typing import Generator, List, Tuple


class BotStats:
  """Stores the task stats for a single bot for a particular mixin."""

  def __init__(self):
    self._frozen = False
    self._total_tasks = 0
    self._failed_tasks = 0
    self._per_suite_total_tasks = collections.defaultdict(int)
    self._per_suite_failed_tasks = collections.defaultdict(int)

  def Freeze(self) -> None:
    if self._frozen:
      return
    self._frozen = True

  # Accessors

  @property
  def total_tasks(self) -> int:
    assert self._frozen
    return self._total_tasks

  @property
  def failed_tasks(self) -> int:
    assert self._frozen
    return self._failed_tasks

  @functools.cached_property
  def overall_failure_rate(self) -> float:
    assert self._frozen
    return float(self._failed_tasks) / self._total_tasks

  def GetTotalTasksForSuite(self, test_suite: str) -> int:
    assert self._frozen
    return self._per_suite_total_tasks[test_suite]

  def GetFailedTasksForSuite(self, test_suite: str) -> int:
    assert self._frozen
    return self._per_suite_failed_tasks[test_suite]

  # Mutators

  def AddStatsForSuite(self, test_suite: str, total_tasks: int,
                       failed_tasks: int) -> None:
    assert not self._frozen
    if total_tasks <= 0:
      raise ValueError('total_tasks must be positive')
    if failed_tasks < 0:
      raise ValueError('failed_tasks must be non-negative')
    if failed_tasks > total_tasks:
      raise ValueError('total_tasks must be >= failed_tasks')
    if test_suite in self._per_suite_total_tasks:
      raise ValueError(
          f'Stats for test suite {test_suite} were already provided - queries '
          f'should only return one row for each mixin/bot/test_suite '
          f'combination')
    self._total_tasks += total_tasks
    self._failed_tasks += failed_tasks
    self._per_suite_total_tasks[test_suite] = total_tasks
    self._per_suite_failed_tasks[test_suite] = failed_tasks


class MixinStats:
  """Stores the task stats for a single mixin."""

  def __init__(self):
    self._frozen = False
    self._total_tasks = 0
    self._failed_tasks = 0
    self._bots = collections.defaultdict(BotStats)

  def Freeze(self) -> None:
    if self._frozen:
      return
    self._frozen = True
    for bot in self._bots.values():
      bot.Freeze()

  # Accessors

  @property
  def total_tasks(self):
    assert self._frozen
    return self._total_tasks

  @property
  def failed_tasks(self):
    assert self._frozen
    return self._failed_tasks

  def IterBots(self) -> Generator[Tuple[str, 'BotStats'], None, None]:
    assert self._frozen
    for bot_id, stats in self._bots.items():
      yield bot_id, stats

  @functools.lru_cache(maxsize=None)
  def GetOverallFailureRates(self) -> List[float]:
    assert self._frozen
    failure_rates = []
    for _, stats in self._bots.items():
      failure_rates.append(stats.overall_failure_rate)
    return failure_rates

  # Mutators

  def AddStatsForBotAndSuite(self, bot_id: str, test_suite: str,
                             total_tasks: int, failed_tasks: int) -> None:
    assert not self._frozen
    if total_tasks <= 0:
      raise ValueError('total_tasks must be positive')
    if failed_tasks < 0:
      raise ValueError('failed_tasks must be non-negative')
    self._total_tasks += total_tasks
    self._failed_tasks += failed_tasks
    self._bots[bot_id].AddStatsForSuite(test_suite, total_tasks, failed_tasks)
