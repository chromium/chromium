# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import gold_inexact_matching.base_parameter_optimizer as base_optimizer
from gold_inexact_matching import common_typing as ct


# This is an abstract class itself, so it's fine that it doesn't implement
# all of BaseParameterOptimizer's abstract methods.
# pylint: disable=abstract-method
class IterativeParameterOptimizer(base_optimizer.BaseParameterOptimizer):
  """Abstract ParameterOptimizer class for running an iterative algorithm."""

  MIN_EDGE_THRESHOLD_STEP = 0
  MAX_EDGE_THRESHOLD_STEP = (
      base_optimizer.BaseParameterOptimizer.MAX_EDGE_THRESHOLD)
  MIN_MAX_DIFF_STEP = MIN_DELTA_THRESHOLD_STEP = 0
  MAX_DELTA_THRESHOLD_STEP = (
      base_optimizer.BaseParameterOptimizer.MAX_DELTA_THRESHOLD)

  @classmethod
  def AddArguments(cls, parser: ct.CmdArgParser) -> ct.ArgumentGroupTuple:
    common_group, sobel_group, fuzzy_group = super(IterativeParameterOptimizer,
                                                   cls).AddArguments(parser)

    sobel_group.add_argument(
        '--edge-threshold-step',
        default=10,
        type=int,
        help='The amount to change the Sobel edge threshold on each iteration.')

    fuzzy_group.add_argument(
        '--max-diff-step',
        default=10,
        type=int,
        help='The amount to change the fuzzy diff maximum number of different '
        'pixels on each iteration.')
    fuzzy_group.add_argument(
        '--delta-threshold-step',
        default=5,
        type=int,
        help='The amount to change the fuzzy diff per-channel delta sum '
        'threshold on each iteration.')

    return common_group, sobel_group, fuzzy_group

  def _VerifyArgs(self) -> None:
    super()._VerifyArgs()

    assert self._args.edge_threshold_step >= self.MIN_EDGE_THRESHOLD_STEP
    assert self._args.edge_threshold_step <= self.MAX_EDGE_THRESHOLD_STEP

    assert self._args.max_diff_step >= self.MIN_MAX_DIFF_STEP
    assert self._args.delta_threshold_step >= self.MIN_DELTA_THRESHOLD_STEP
#pylint: enable=abstract-method
