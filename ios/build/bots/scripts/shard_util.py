# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os
from typing import List, Optional, Tuple

import test_runner_errors

LOGGER = logging.getLogger(__name__)

# Caps chunks at 12 methods so slow 19-63 method EG2 classes split across shards
# while keeping ~73% of classes compact. +forceRestartAndWipe already relaunches
# the app per method, so splitting adds only per-class setup/teardown overhead.
DEFAULT_MAX_TESTS_PER_CHUNK = 12

# Floor on the dynamic per-shard chunk cap so small suites avoid splitting
# small classes unnecessarily.
DEFAULT_MIN_TESTS_PER_CHUNK = 4


class ShardingError(test_runner_errors.Error):
  """Error related with sharding logic."""

  pass


class ExcessShardsError(ShardingError):
  """The test module has more shards than schedulable test units."""

  def __init__(self, num_shards, num_test_units):
    super(ExcessShardsError, self).__init__(
      'The test module is misconfigured to have more shards than '
      'schedulable test units.'
      f' Shards: {num_shards} Test Units: {num_test_units}'
    )


class _Shard:
  """Accumulates test identifiers assigned to a single shard."""

  def __init__(self, index: int):
    self.index: int = index
    self.identifiers: List[str] = []
    self.num_classes: int = 0
    self.size: int = 0


def gtest_shard_index():
  """Returns shard index in environment, or 0 if not in sharding environment."""
  return int(os.getenv('GTEST_SHARD_INDEX', 0))


def gtest_total_shards():
  """Returns total shard count in environment, or 1 if not in environment."""
  return int(os.getenv('GTEST_TOTAL_SHARDS', 1))


def balance_eg_tests_into_sublists(
  all_eg_test_names: List[Tuple[str, str]],
  total_shards: int,
  max_tests_per_chunk: Optional[int] = DEFAULT_MAX_TESTS_PER_CHUNK,
  min_tests_per_chunk: int = DEFAULT_MIN_TESTS_PER_CHUNK,
) -> List[List[str]]:
  """Balances enumerated EG test methods into deterministic per-shard lists.

  Small classes remain compact `'TestClass'` entries, while classes larger than
  the effective chunk cap split into balanced contiguous `'TestClass/method'`
  chunks placed across distinct shards.

  Args:
    all_eg_test_names: List of `(test_class, test_method)` tuples enumerated
      from the EG test runner bundle.
    total_shards: Total number of shards (must be >= 1).
    max_tests_per_chunk: Upper bound on methods per chunk. Pass `None` or `<= 0`
      to disable method-level splitting.
    min_tests_per_chunk: Floor on the dynamic chunk cap (clamped to `>= 1`).

  Returns:
    A list of `total_shards` non-empty lists of `OnlyTestIdentifiers` strings
    (`'TestClass'` or `'TestClass/testMethod'`).

  Raises:
    ShardingError: If `total_shards < 1`.
    ExcessShardsError: If `total_shards` exceeds the number of schedulable
      units produced after chunking.
  """
  if total_shards < 1:
    raise ShardingError(f'total_shards must be >= 1, got {total_shards}')

  # Deduplicate and canonicalize so sharding is independent of enumeration order
  # and keeps inherited methods scoped strictly to their emitted class name.
  methods_by_class = collections.defaultdict(set)
  for test_class, test_method in all_eg_test_names:
    methods_by_class[test_class].add(test_method)

  sorted_methods_by_class = [
    (test_class, sorted(methods))
    for test_class, methods in sorted(methods_by_class.items())
  ]
  total_methods = sum(len(methods) for _, methods in sorted_methods_by_class)

  if (
    total_shards == 1 or max_tests_per_chunk is None or max_tests_per_chunk <= 0
  ):
    effective_max_chunk = None
  else:
    effective_min_chunk = max(1, min_tests_per_chunk)
    effective_max_chunk = min(
      max_tests_per_chunk,
      max(effective_min_chunk, total_methods // (2 * total_shards)),
    )

  # Split each oversized class into k <= total_shards balanced contiguous
  # chunks ordered largest-first (`base_size + 1` before `base_size`).
  class_chunks: List[Tuple[str, int, List[Tuple[int, Tuple[str, ...]]]]] = []
  total_units = 0
  for test_class, methods in sorted_methods_by_class:
    num_methods = len(methods)
    if effective_max_chunk is None or num_methods <= effective_max_chunk:
      chunks = [(num_methods, (test_class,))]
    else:
      desired_chunks = (
        num_methods + effective_max_chunk - 1
      ) // effective_max_chunk
      num_chunks = min(desired_chunks, total_shards)
      base_size, remainder = divmod(num_methods, num_chunks)
      chunks = []
      offset = 0
      for chunk_index in range(num_chunks):
        chunk_size = base_size + (1 if chunk_index < remainder else 0)
        chunk_methods = methods[offset : offset + chunk_size]
        offset += chunk_size
        chunks.append(
          (
            chunk_size,
            tuple(f'{test_class}/{method}' for method in chunk_methods),
          )
        )
    total_units += len(chunks)
    class_chunks.append((test_class, num_methods, chunks))

  if total_shards > total_units:
    raise ExcessShardsError(total_shards, total_units)

  LOGGER.debug(
    'Balancing %d methods across %d classes (%d units, '
    'effective_max_chunk=%s) into %d shards.',
    total_methods,
    len(sorted_methods_by_class),
    total_units,
    effective_max_chunk,
    total_shards,
  )

  # Order classes by largest chunk first, then total methods, then name.
  # Placing all k chunks of a class together onto the k lightest shards
  # guarantees distinct shards per class and pairs larger chunks with lighter
  # shards. Breaking ties by `num_classes` spreads distinct classes across
  # shards so parallel simulator clones stay busy.
  class_chunks.sort(key=lambda item: (-item[2][0][0], -item[1], item[0]))

  shards = [_Shard(index=i) for i in range(total_shards)]
  for test_class, _, chunks in class_chunks:
    target_shards = sorted(
      shards, key=lambda s: (s.size, s.num_classes, s.index)
    )[: len(chunks)]
    for chunk_index, (target, (chunk_size, identifiers)) in enumerate(
      zip(target_shards, chunks)
    ):
      target.identifiers.extend(identifiers)
      target.num_classes += 1
      target.size += chunk_size
      LOGGER.debug(
        '%s (chunk %d/%d, %d methods) allocated to shard %d',
        test_class,
        chunk_index + 1,
        len(chunks),
        chunk_size,
        target.index,
      )

  return [shard.identifiers for shard in shards]


def shard_eg_test_cases(
  all_eg_test_names: List[Tuple[str, str]],
  max_tests_per_chunk: Optional[int] = DEFAULT_MAX_TESTS_PER_CHUNK,
  min_tests_per_chunk: int = DEFAULT_MIN_TESTS_PER_CHUNK,
) -> List[str]:
  """Shards test cases into total_shards and returns entries for this shard.

  Args:
    all_eg_test_names: A list of all EG test methods present in the
      `-Runner.app` binary. Each element is a `(test_case, test_method)` tuple.
    max_tests_per_chunk: Upper bound on methods per chunk (`None` or `<= 0`
      disables method-level splitting).
    min_tests_per_chunk: Floor on the dynamic chunk cap.

  Returns:
    A list of `OnlyTestIdentifiers` entries to execute on this shard.

  Raises:
    ShardingError: If shard environment variables or parameters are invalid.
    ExcessShardsError: If `total_shards` exceeds the number of schedulable
      units.
  """
  shard_index = int(gtest_shard_index())
  total_shards = int(gtest_total_shards())
  if total_shards < 1:
    raise ShardingError(f'total_shards must be >= 1, got {total_shards}')
  if shard_index < 0 or shard_index >= total_shards:
    raise ShardingError(
      f'shard_index {shard_index} out of range for total_shards={total_shards}'
    )

  sublists = balance_eg_tests_into_sublists(
    all_eg_test_names,
    total_shards,
    max_tests_per_chunk=max_tests_per_chunk,
    min_tests_per_chunk=min_tests_per_chunk,
  )
  tests = sublists[shard_index]

  LOGGER.info(f'Tests to be executed this round: {tests}')
  return tests
