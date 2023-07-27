# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import enum
import logging
from typing import Optional, Tuple

import gold_inexact_matching.base_parameter_optimizer as base_optimizer
from gold_inexact_matching import common_typing as ct
from gold_inexact_matching import parameter_set


class _UnlockedParameter(enum.Enum):
  MAX_DIFF = 1
  DELTA_THRESHOLD = 2
  EDGE_THRESHOLD = 3


class BinarySearchParameterOptimizer(base_optimizer.BaseParameterOptimizer):
  """A ParameterOptimizer for use with a single changing parameter.

  The ideal optimizer if only one parameter needs to be varied, e.g. finding
  the best Sobel edge threshold to use when not using any additional fuzzy
  diffing.
  """

  def __init__(self, args: ct.ParsedCmdArgs, test_name: str):
    self._unlocked_parameter: Optional[_UnlockedParameter] = None
    super().__init__(args, test_name)

  def _VerifyArgs(self) -> None:
    super()._VerifyArgs()

    max_diff_locked = self._args.max_max_diff == self._args.min_max_diff
    delta_threshold_locked = (
        self._args.max_delta_threshold == self._args.min_delta_threshold)
    edge_threshold_locked = (
        self._args.max_edge_threshold == self._args.min_delta_threshold)

    if not ((max_diff_locked ^ delta_threshold_locked) or
            (delta_threshold_locked ^ edge_threshold_locked)):
      raise RuntimeError(
          'Binary search optimization requires all but one parameter to be '
          'locked (min == max).')

    if not max_diff_locked:
      self._unlocked_parameter = _UnlockedParameter.MAX_DIFF
    elif not delta_threshold_locked:
      self._unlocked_parameter = _UnlockedParameter.DELTA_THRESHOLD
    else:
      self._unlocked_parameter = _UnlockedParameter.EDGE_THRESHOLD

  def _RunOptimizationImpl(self) -> None:
    known_good, known_bad = self._GetStartingValues()
    while (abs(known_good - known_bad)) > 1:
      midpoint = (known_good + known_bad) // 2
      parameters = self._CreateParameterSet(midpoint)
      success, _, _ = self._RunComparisonForParameters(parameters)
      if success:
        logging.info('Found good parameters %s', parameters)
        known_good = midpoint
      else:
        logging.info('Found bad parameters %s', parameters)
        known_bad = midpoint
    print('Found optimal parameters: %s' % parameters)

  def _GetStartingValues(self) -> Tuple[int, int]:
    """Gets the initial good/bad values for the binary search.

    Returns:
      A tuple (known_good, assumed_bad). |known_good| is a value that is known
      to make the comparison succeed. |assumed_bad| is a value that is expected
      to make the comparison fail, although it has not necessarily been tested
      yet.
    """
    if self._unlocked_parameter == _UnlockedParameter.MAX_DIFF:
      return self._args.max_max_diff, self._args.min_max_diff
    if self._unlocked_parameter == _UnlockedParameter.DELTA_THRESHOLD:
      return self._args.max_delta_threshold, self._args.min_delta_threshold
    return self._args.min_edge_threshold, self._args.max_edge_threshold

  def _CreateParameterSet(self, value: int) -> parameter_set.ParameterSet:
    """Creates a parameter_set.ParameterSet to test.

    Args:
      value: The value to set the variable parameter to.

    Returns:
      A parameter_set.ParameterSet with the variable parameter set to |value|
      and the other parameters set to their fixed values.
    """
    if self._unlocked_parameter == _UnlockedParameter.MAX_DIFF:
      return parameter_set.ParameterSet(value, self._args.min_delta_threshold,
                                        self._args.min_edge_threshold)
    if self._unlocked_parameter == _UnlockedParameter.DELTA_THRESHOLD:
      return parameter_set.ParameterSet(self._args.min_max_diff, value,
                                        self._args.min_edge_threshold)
    return parameter_set.ParameterSet(self._args.min_max_diff,
                                      self._args.min_delta_threshold, value)
