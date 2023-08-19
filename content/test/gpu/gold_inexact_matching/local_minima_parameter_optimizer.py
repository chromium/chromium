# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import collections
import itertools
import logging
import sys
from typing import Dict

import gold_inexact_matching.iterative_parameter_optimizer\
    as iterative_optimizer
from gold_inexact_matching import common_typing as ct
from gold_inexact_matching import parameter_set


Sparse2DIntArray = Dict[int, Dict[int, int]]


class LocalMinimaParameterOptimizer(
    iterative_optimizer.IterativeParameterOptimizer):
  """A ParameterOptimizer to find local minima.

  Works on any number of variable parameters and is faster than brute
  forcing, but not guaranteed to find all interesting parameter combinations.
  """
  MIN_EDGE_THRESHOLD_WEIGHT = 0
  MIN_MAX_DIFF_WEIGHT = MIN_DELTA_THRESHOLD_WEIGHT = 0

  def __init__(self, args: ct.ParsedCmdArgs, test_name: str):
    super().__init__(args, test_name)
    # These are (or will be) maps of ints to maps of ints to ints, i.e. a 2D
    # array containing ints, just using maps instead of lists. They hold the
    # most permissive value visited so far that resulted in a comparison failure
    # for a particular parameter given the other two parameters. These are used
    # to prune combinations we don't care about, similar to skipping
    # combinations that produce a higher weight than our smallest.
    # Delta -> Edge -> Max Diff
    self._permissive_max_diff_map: Sparse2DIntArray = {}
    # Max Diff -> Edge -> Delta
    self._permissive_delta_map: Sparse2DIntArray = {}
    # Max Diff -> Delta -> Edge
    self._permissive_edge_map: Sparse2DIntArray = {}

  @classmethod
  def AddArguments(cls, parser: ct.CmdArgParser) -> ct.ArgumentGroupTuple:
    common_group, sobel_group, fuzzy_group = super(
        LocalMinimaParameterOptimizer, cls).AddArguments(parser)

    common_group.add_argument(
        '--use-bfs',
        action='store_true',
        default=False,
        help='Use a breadth-first search instead of a depth-first search. This '
        'will likely be significantly slower, but is more likely to find '
        'multiple local minima with the same weight.')

    sobel_group.add_argument(
        '--edge-threshold-weight',
        default=1,
        type=int,
        help='The weight associated with the edge threshold. Higher values '
        'will penalize a more permissive parameter value more harshly.')

    fuzzy_group.add_argument(
        '--max-diff-weight',
        default=3,
        type=int,
        help='The weight associated with the maximum number of different '
        'pixels. Higher values will penalize a more permissive parameter value '
        'more harshly.')
    fuzzy_group.add_argument(
        '--delta-threshold-weight',
        default=10,
        type=int,
        help='The weight associated with the per-channel delta sum. Higher '
        'values will penalize a more permissive parameter value more harshly.')

    return common_group, sobel_group, fuzzy_group

  def _VerifyArgs(self) -> None:
    super()._VerifyArgs()

    assert self._args.edge_threshold_weight >= self.MIN_EDGE_THRESHOLD_WEIGHT

    assert self._args.max_diff_weight >= self.MIN_MAX_DIFF_WEIGHT
    assert self._args.delta_threshold_weight >= self.MIN_DELTA_THRESHOLD_WEIGHT

  def _RunOptimizationImpl(self) -> None:
    visited_parameters = set()
    to_visit = collections.deque()
    smallest_weight = sys.maxsize
    smallest_parameters = []

    to_visit.append(self._GetMostPermissiveParameters())
    # Do a search, only considering adjacent parameters if:
    # 1. Their weight is less than or equal to the smallest found weight.
    # 2. They haven't been visited already.
    # 3. They are not guaranteed to fail based on previously tested parameters.
    # 4. The current parameters result in a successful comparison.
    while to_visit:
      current_parameters = None
      if self._args.use_bfs:
        current_parameters = to_visit.popleft()
      else:
        current_parameters = to_visit.pop()
      weight = self._GetWeight(current_parameters)
      if weight > smallest_weight:
        continue
      if current_parameters in visited_parameters:
        continue
      if self._ParametersAreGuaranteedToFail(current_parameters):
        visited_parameters.add(current_parameters)
        continue
      visited_parameters.add(current_parameters)
      success, _, _ = self._RunComparisonForParameters(current_parameters)
      if success:
        for adjacent in self._AdjacentParameters(current_parameters):
          to_visit.append(adjacent)
        if smallest_weight == weight:
          logging.info('Found additional smallest parameter %s',
                       current_parameters)
          smallest_parameters.append(current_parameters)
        else:
          logging.info('Found new smallest parameter with weight %d: %s',
                       weight, current_parameters)
          smallest_weight = weight
          smallest_parameters = [current_parameters]
      else:
        self._UpdateMostPermissiveFailedParameters(current_parameters)
    print('Found %d parameter(s) with the smallest weight:' %
          len(smallest_parameters))
    for p in smallest_parameters:
      print(p)

  def _ParametersAreGuaranteedToFail(self,
                                     parameters: parameter_set.ParameterSet
                                     ) -> bool:
    """Checks whether the given ParameterSet is guaranteed to fail.

    A ParameterSet is guaranteed to fail if we have already tried and failed
    with a similar ParameterSet that was more permissive. Specifically, if we
    have tried and failed with a ParameterSet with all but one parameters
    matching, and the non-matching parameter was more permissive than the
    current one.

    Args:
      parameters: The ParameterSet instance to check.

    Returns:
      True if |parameters| is guaranteed to fail based on previously tried
      parameters, otherwise False.
    """
    permissive_max_diff = self._permissive_max_diff_map.get(
        parameters.delta_threshold, {}).get(parameters.edge_threshold, -1)
    if parameters.max_diff < permissive_max_diff:
      return True

    permissive_delta = self._permissive_delta_map.get(
        parameters.max_diff, {}).get(parameters.edge_threshold, -1)
    if parameters.delta_threshold < permissive_delta:
      return True

    permissive_edge = self._permissive_edge_map.get(
        parameters.max_diff, {}).get(parameters.delta_threshold, sys.maxsize)
    if parameters.edge_threshold > permissive_edge:
      return True

    return False

  def _UpdateMostPermissiveFailedParameters(
      self, parameters: parameter_set.ParameterSet) -> None:
    """Updates the array of most permissive failed parameters.

    This is used in conjunction with _ParametersAreGuaranteedToFail to prune
    ParameterSets without having to actually test them. Values are updated if
    |parameters| shares two parameters with a a previously failed ParameterSet,
    but |parameters|' third parameter is more permissive.

    Args:
      parameters: A ParameterSet to pull updated values from.
    """
    permissive_max_diff = self._permissive_max_diff_map.setdefault(
        parameters.delta_threshold, {}).get(parameters.edge_threshold, -1)
    permissive_max_diff = max(permissive_max_diff, parameters.max_diff)
    self._permissive_max_diff_map[parameters.delta_threshold][
        parameters.edge_threshold] = permissive_max_diff

    permissive_delta = self._permissive_delta_map.setdefault(
        parameters.max_diff, {}).get(parameters.edge_threshold, -1)
    permissive_delta = max(permissive_delta, parameters.delta_threshold)
    self._permissive_delta_map[parameters.max_diff][
        parameters.edge_threshold] = permissive_delta

    permissive_edge = self._permissive_edge_map.setdefault(
        parameters.max_diff, {}).get(parameters.delta_threshold, sys.maxsize)
    permissive_edge = min(permissive_edge, parameters.edge_threshold)
    self._permissive_edge_map[parameters.max_diff][
        parameters.delta_threshold] = permissive_edge

  def _AdjacentParameters(self, starting_parameters):
    max_diff = starting_parameters.max_diff
    delta_threshold = starting_parameters.delta_threshold
    edge_threshold = starting_parameters.edge_threshold

    max_diff_step = self._args.max_diff_step
    delta_threshold_step = self._args.delta_threshold_step
    edge_threshold_step = self._args.edge_threshold_step

    max_diffs = [
        max(self._args.min_max_diff, max_diff - max_diff_step), max_diff,
        min(self._args.max_max_diff, max_diff + max_diff_step)
    ]
    delta_thresholds = [
        max(self._args.min_delta_threshold,
            delta_threshold - delta_threshold_step), delta_threshold,
        min(self._args.max_delta_threshold,
            delta_threshold + delta_threshold_step)
    ]
    edge_thresholds = [
        max(self._args.min_edge_threshold,
            edge_threshold - edge_threshold_step), edge_threshold,
        min(self._args.max_edge_threshold, edge_threshold + edge_threshold_step)
    ]
    for combo in itertools.product(max_diffs, delta_thresholds,
                                   edge_thresholds):
      adjacent = parameter_set.ParameterSet(combo[0], combo[1], combo[2])
      if adjacent != starting_parameters:
        yield adjacent

  def _GetWeight(self, parameters: parameter_set.ParameterSet) -> int:
    return (parameters.max_diff * self._args.max_diff_weight +
            parameters.delta_threshold * self._args.delta_threshold_weight +
            (self.MAX_EDGE_THRESHOLD - parameters.edge_threshold) *
            self._args.edge_threshold_weight)
