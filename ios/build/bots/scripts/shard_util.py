# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os
from typing import Tuple, List

import test_runner_errors

LOGGER = logging.getLogger(__name__)


class ShardingError(test_runner_errors.Error):
  """Error related with sharding logic."""
  pass


class ExcessShardsError(ShardingError):
  """The test module is misconfigured to have more shards than test cases"""

  def __init__(self, num_shards, num_test_cases):
    super(ExcessShardsError, self).__init__(
        f'The test module is misconfigured to have more shards than test cases.'
        f' Shards: {num_shards} Test Cases: {num_test_cases}')


def gtest_shard_index():
  """Returns shard index in environment, or 0 if not in sharding environment."""
  return int(os.getenv('GTEST_SHARD_INDEX', 0))


def gtest_total_shards():
  """Returns total shard count in environment, or 1 if not in environment."""
  return int(os.getenv('GTEST_TOTAL_SHARDS', 1))


def balance_into_sublists(test_counts: collections.Counter,
                          total_shards: int) -> List[List[str]]:
  """Augment the result of otool into balanced sublists

  Args:
    test_counts: (collections.Counter) dict of test_case to test case numbers
    total_shards: (int) total number of shards this was divided into

  Returns:
    list of list of test classes
  """

  class Shard(object):
    """Stores list of test classes and number of all tests"""

    def __init__(self):
      self.test_classes = []
      self.size = 0

  shards = [Shard() for i in range(total_shards)]

  # Balances test classes between shards to have
  # approximately equal number of tests per shard.
  for test_class, number_of_test_methods in test_counts.most_common():
    min_shard = min(shards, key=lambda shard: shard.size)
    min_shard.test_classes.append(test_class)
    min_shard.size += number_of_test_methods
    LOGGER.debug('%s test case is allocated to shard %s with %s test methods' %
                 (test_class, shards.index(min_shard), number_of_test_methods))

  sublists = [shard.test_classes for shard in shards]
  return sublists


def shard_eg_test_cases(all_eg_test_names: List[Tuple[str, str]]) -> List[str]:
  """Shard test cases into total_shards, and determine which test cases to
    run for this shard.

    Raises:
      ExcessShardsError: If there exist more shards than test_cases

    Args:
        all_eg_test_names: A list of all EG test methods present in the
          -Runner.app binary. Each list element is a tuple in the form
          (test_case, test_method)

    Returns: a list of test cases to execute on this shard
    """
  shard_index = gtest_shard_index()
  total_shards = gtest_total_shards()

  test_counts = collections.Counter(
      test_class for test_class, _ in all_eg_test_names)

  # Ensure shard and total shard is int
  shard_index = int(shard_index)
  total_shards = int(total_shards)
  total_test_cases = len(test_counts)

  if total_shards > total_test_cases:
    raise ExcessShardsError(total_shards, total_test_cases)

  sublists = balance_into_sublists(test_counts, total_shards)
  tests = sublists[shard_index]

  LOGGER.info("Tests to be executed this round: {}".format(tests))
  return tests
