#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os
import random
import unittest
from unittest import mock

import shard_util


def _make_suite(class_counts):
  """Builds a synthetic `(test_class, test_method)` list from class counts."""
  return [
    (test_class, f'test{i:03d}')
    for test_class, count in class_counts.items()
    for i in range(count)
  ]


def _expand_shard(all_tests, shard_entries):
  """Expands shard identifiers to `'Class/method'` like `EgtestsApp`."""
  included = set(shard_entries)
  seen = set()
  expanded = []
  for test_class, test_method in all_tests:
    test_name = f'{test_class}/{test_method}'
    if test_name in seen:
      continue
    if test_name in included or test_class in included:
      seen.add(test_name)
      expanded.append(test_name)
  return expanded


def _classes_in_shard(shard_entries):
  """Returns the set of distinct test class names referenced in a shard."""
  return {entry.split('/', 1)[0] for entry in shard_entries}


class TestShardUtil(unittest.TestCase):
  """Test cases for shard_util.py"""

  def _assert_valid_partition(self, all_tests, sublists):
    """Verifies exact partition and single-chunk-per-class shard invariants."""
    self.assertGreaterEqual(len(sublists), 1)
    expected_all = {f'{cls}/{method}' for cls, method in set(all_tests)}
    methods_by_class = collections.defaultdict(list)
    for cls, method in sorted(set(all_tests)):
      methods_by_class[cls].append(method)

    union = set()
    compact_classes = set()
    split_classes = set()

    for shard_entries in sublists:
      self.assertGreater(len(shard_entries), 0)
      self.assertEqual(len(shard_entries), len(set(shard_entries)))

      # Track compact vs split classes and ensure any split class in a shard
      # appears as a single contiguous slice (never two chunks on one shard).
      per_shard_methods = collections.defaultdict(list)
      for entry in shard_entries:
        if '/' in entry:
          cls, method = entry.split('/', 1)
          split_classes.add(cls)
          per_shard_methods[cls].append(method)
        else:
          compact_classes.add(entry)
          self.assertNotIn(entry, per_shard_methods)

      for cls, methods in per_shard_methods.items():
        all_sorted_methods = methods_by_class[cls]
        indices = [all_sorted_methods.index(m) for m in methods]
        self.assertEqual(indices, sorted(indices))
        self.assertEqual(
          indices,
          list(range(indices[0], indices[0] + len(indices))),
          f'{cls} has non-contiguous methods on a single shard: {methods}',
        )

      expanded = _expand_shard(all_tests, shard_entries)
      self.assertGreater(len(expanded), 0)
      expanded_set = set(expanded)
      self.assertEqual(len(expanded), len(expanded_set))
      self.assertTrue(
        union.isdisjoint(expanded_set),
        f'Duplicate test across shards: {union & expanded_set}',
      )
      union.update(expanded_set)

    self.assertTrue(
      compact_classes.isdisjoint(split_classes),
      f'Class emitted both compact and split: '
      f'{compact_classes & split_classes}',
    )
    self.assertEqual(union, expected_all)

  def test_exact_partition_invariant(self):
    """Tests exact partition and tight balance across shard counts."""
    class_counts = {
      'Big1': 63,
      'Big2': 47,
      'Big3': 32,
      'Med1': 23,
      'Med2': 15,
      'Border12': 12,
    }
    for i in range(20):
      class_counts[f'Small{i:02d}'] = (i % 11) + 1
    suite = _make_suite(class_counts)
    total_methods = len(suite)

    for total_shards in (2, 3, 5, 10, 20):
      sublists = shard_util.balance_eg_tests_into_sublists(suite, total_shards)
      self.assertEqual(len(sublists), total_shards)
      self._assert_valid_partition(suite, sublists)
      shard_sizes = [len(_expand_shard(suite, s)) for s in sublists]
      effective_cap = min(
        shard_util.DEFAULT_MAX_TESTS_PER_CHUNK,
        max(
          shard_util.DEFAULT_MIN_TESTS_PER_CHUNK,
          total_methods // (2 * total_shards),
        ),
      )
      self.assertLessEqual(max(shard_sizes) - min(shard_sizes), effective_cap)

  def test_order_independence_and_deduplication(self):
    """Tests identical output across random permutations and duplicates."""
    suite = _make_suite(
      {
        'ZetaBig': 30,
        'AlphaBig': 25,
        'MidClass': 13,
        'SmallB': 5,
        'SmallA': 5,
        'Tiny': 2,
      }
    )
    baseline = shard_util.balance_eg_tests_into_sublists(suite, 4)
    self._assert_valid_partition(suite, baseline)

    for seed in range(10):
      shuffled = list(suite)
      random.Random(seed).shuffle(shuffled)
      self.assertEqual(
        shard_util.balance_eg_tests_into_sublists(shuffled, 4), baseline
      )

    with_duplicates = list(suite) + suite[:15] + [suite[0], suite[-1]]
    self.assertEqual(
      shard_util.balance_eg_tests_into_sublists(with_duplicates, 4), baseline
    )

    # Deduplication happens before computing `total_methods`, so repeated
    # entries do not inflate the dynamic chunk cap and suppress splitting.
    dedup_base = _make_suite({'A': 6, 'B': 5, 'C': 5})
    dedup_expected = shard_util.balance_eg_tests_into_sublists(dedup_base, 2)
    self._assert_valid_partition(dedup_base, dedup_expected)
    dedup_heavy = dedup_base + [('A', 'test000')] * 20
    self.assertEqual(
      shard_util.balance_eg_tests_into_sublists(dedup_heavy, 2), dedup_expected
    )
    self.assertTrue(
      all('/' in entry for shard in dedup_expected for entry in shard)
    )

  def test_small_classes_compact_large_classes_split_contiguous(self):
    """Tests compact small classes and contiguous divmod chunking."""
    class_counts = {
      'Small4': 4,
      'Border12': 12,
      'Split13': 13,
      'Split25': 25,
    }
    # Add filler so total_methods // (2 * 4) >= 12 -> effective_max_chunk == 12.
    for i in range(6):
      class_counts[f'Filler{i}'] = 10
    suite = _make_suite(class_counts)

    sublists = shard_util.balance_eg_tests_into_sublists(suite, 4)
    self._assert_valid_partition(suite, sublists)

    all_entries = [entry for shard in sublists for entry in shard]
    self.assertIn('Small4', all_entries)
    self.assertIn('Border12', all_entries)
    self.assertFalse(any(e.startswith('Small4/') for e in all_entries))
    self.assertFalse(any(e.startswith('Border12/') for e in all_entries))

    split13_chunks = sorted(
      [
        c
        for c in (
          [e for e in shard if e.startswith('Split13/')] for shard in sublists
        )
        if c
      ],
      key=lambda c: c[0],
    )
    self.assertEqual([len(c) for c in split13_chunks], [7, 6])
    self.assertEqual(
      split13_chunks[0], [f'Split13/test{i:03d}' for i in range(7)]
    )
    self.assertEqual(
      split13_chunks[1], [f'Split13/test{i:03d}' for i in range(7, 13)]
    )

    split25_chunks = sorted(
      [
        c
        for c in (
          [e for e in shard if e.startswith('Split25/')] for shard in sublists
        )
        if c
      ],
      key=lambda c: c[0],
    )
    self.assertEqual([len(c) for c in split25_chunks], [9, 8, 8])
    self.assertEqual(
      split25_chunks[0], [f'Split25/test{i:03d}' for i in range(9)]
    )
    self.assertEqual(
      split25_chunks[1], [f'Split25/test{i:03d}' for i in range(9, 17)]
    )
    self.assertEqual(
      split25_chunks[2], [f'Split25/test{i:03d}' for i in range(17, 25)]
    )

  def test_split_class_chunks_on_distinct_shards(self):
    """Tests that chunks of every split class land on distinct shards."""
    suite = _make_suite(
      {
        'Huge63': 63,
        'Large47': 47,
        'Large32': 32,
        'Med23': 23,
        'SmallA': 6,
        'SmallB': 5,
      }
    )
    expected_chunks = {'Huge63': 6, 'Large47': 4, 'Large32': 3, 'Med23': 2}

    sublists = shard_util.balance_eg_tests_into_sublists(suite, 6)
    self._assert_valid_partition(suite, sublists)

    for cls, num_chunks in expected_chunks.items():
      shards_with_cls = [
        idx
        for idx, shard in enumerate(sublists)
        if any(e.startswith(f'{cls}/') for e in shard)
      ]
      self.assertEqual(len(shards_with_cls), num_chunks)

  def test_at_least_two_distinct_classes_per_shard(self):
    """Tests shards get >= 2 distinct classes for clone-2 utilization."""
    # TabGridTestCase-like bottleneck plus smaller classes.
    bottleneck_counts = {'TabGridTestCase': 63}
    for i in range(20):
      bottleneck_counts[f'SmallClass{i:02d}'] = 4
    bottleneck_suite = _make_suite(bottleneck_counts)
    sublists_a = shard_util.balance_eg_tests_into_sublists(bottleneck_suite, 4)
    self._assert_valid_partition(bottleneck_suite, sublists_a)
    for shard in sublists_a:
      self.assertGreaterEqual(len(_classes_in_shard(shard)), 2)

    # Equal-size tiebreak prefers fewer distinct classes (giving shard 0 a
    # second class rather than stacking onto shard 1).
    tie_suite = _make_suite({'A': 4, 'B': 2, 'C': 2, 'D': 1})
    sublists_tie = shard_util.balance_eg_tests_into_sublists(tie_suite, 2)
    self._assert_valid_partition(tie_suite, sublists_tie)
    self.assertEqual(sublists_tie, [['A', 'D'], ['B', 'C']])

  def test_mixed_remainder_and_saturated_spread_bounded(self):
    """Tests mixed-remainder and saturated suites keep spread <= chunk cap."""
    # Mixed-remainder 5-shard case where splitting chunks across global size
    # bands hits spread 22; placing all k chunks per class keeps it <= 12.
    mixed_suite = _make_suite({'A': 56, 'B': 47, 'C': 35, 'D': 46})
    mixed_sublists = shard_util.balance_eg_tests_into_sublists(mixed_suite, 5)
    self._assert_valid_partition(mixed_suite, mixed_sublists)
    mixed_sizes = [len(_expand_shard(mixed_suite, s)) for s in mixed_sublists]
    self.assertLessEqual(max(mixed_sizes) - min(mixed_sizes), 12)
    for shard in mixed_sublists:
      self.assertGreaterEqual(len(_classes_in_shard(shard)), 2)

    # Saturated uneven classes spanning all shards balance evenly.
    saturated_suite = _make_suite({'C0': 31, 'C1': 31, 'C2': 31})
    saturated_sublists = shard_util.balance_eg_tests_into_sublists(
      saturated_suite, 3, max_tests_per_chunk=10
    )
    self._assert_valid_partition(saturated_suite, saturated_sublists)
    self.assertEqual(
      [len(_expand_shard(saturated_suite, s)) for s in saturated_sublists],
      [31, 31, 31],
    )

  def test_dynamic_cap_and_min_chunk_clamping(self):
    """Tests dynamic cap, floor clamping, and explicit max/min overrides."""
    # 200 methods on 20 shards -> dynamic cap = 200 // 40 = 5.
    counts_a = {'Split10': 10, 'Split6': 6, 'Fill4': 4}
    for i in range(36):
      counts_a[f'C{i:02d}'] = 5
    suite_a = _make_suite(counts_a)
    sublists_a = shard_util.balance_eg_tests_into_sublists(suite_a, 20)
    self._assert_valid_partition(suite_a, sublists_a)
    all_a = [e for s in sublists_a for e in s]
    self.assertIn('C00', all_a)
    self.assertNotIn('Split6', all_a)
    split6_sizes = sorted(
      sum(1 for e in s if e.startswith('Split6/'))
      for s in sublists_a
      if any(e.startswith('Split6/') for e in s)
    )
    self.assertEqual(split6_sizes, [3, 3])

    # 30 methods on 10 shards -> 30 // 20 = 1, floored to min=4.
    counts_b = {'Compact4': 4}
    for i in range(13):
      counts_b[f'D{i:02d}'] = 2
    suite_b = _make_suite(counts_b)
    sublists_b = shard_util.balance_eg_tests_into_sublists(suite_b, 10)
    self._assert_valid_partition(suite_b, sublists_b)
    self.assertIn('Compact4', [e for s in sublists_b for e in s])

    # min_tests_per_chunk <= 0 clamped to 1 (no ZeroDivisionError).
    suite_c = _make_suite({'TwoMethod': 2, 'OneMethod': 1})
    sublists_c = shard_util.balance_eg_tests_into_sublists(
      suite_c, 3, min_tests_per_chunk=0
    )
    self._assert_valid_partition(suite_c, sublists_c)
    split_two = [
      [e for e in s if e.startswith('TwoMethod/')] for s in sublists_c
    ]
    self.assertEqual(sorted(len(c) for c in split_two if c), [1, 1])

    # Explicit max_tests_per_chunk < default min_tests_per_chunk caps at max.
    suite_d = _make_suite({'ThreeMethod': 3, 'TwoMethod': 2})
    sublists_d = shard_util.balance_eg_tests_into_sublists(
      suite_d, 2, max_tests_per_chunk=2
    )
    self._assert_valid_partition(suite_d, sublists_d)
    split_three = [
      [e for e in s if e.startswith('ThreeMethod/')] for s in sublists_d
    ]
    self.assertEqual(sorted(len(c) for c in split_three if c), [1, 2])

  def test_num_chunks_capped_at_total_shards(self):
    """Tests desired chunk count is capped at total_shards."""
    suite = _make_suite({'Huge': 60})
    sublists = shard_util.balance_eg_tests_into_sublists(
      suite, 3, max_tests_per_chunk=10
    )
    self._assert_valid_partition(suite, sublists)
    self.assertEqual([len(s) for s in sublists], [20, 20, 20])

  def test_single_shard_and_disabled_splitting(self):
    """Tests total_shards=1 and max_tests_per_chunk None/<=0 keep compact."""
    suite = _make_suite({'Big': 50, 'Med': 10, 'Small': 10})
    single = shard_util.balance_eg_tests_into_sublists(suite, 1)
    self.assertEqual(single, [['Big', 'Med', 'Small']])
    self._assert_valid_partition(suite, single)

    for max_chunk in (None, 0, -4):
      sublists = shard_util.balance_eg_tests_into_sublists(
        suite, 2, max_tests_per_chunk=max_chunk
      )
      self._assert_valid_partition(suite, sublists)
      self.assertFalse(any('/' in e for s in sublists for e in s))
      self.assertEqual(sublists, [['Big'], ['Med', 'Small']])

  def test_excess_shards_and_invalid_bounds(self):
    """Tests ExcessShardsError and invalid total_shards bounds."""
    with self.assertRaises(shard_util.ShardingError) as ctx:
      shard_util.balance_eg_tests_into_sublists(_make_suite({'A': 5}), 0)
    self.assertNotIsInstance(ctx.exception, shard_util.ExcessShardsError)

    small_suite = _make_suite({'ClassA': 2, 'ClassB': 2})
    with self.assertRaises(shard_util.ExcessShardsError) as ctx:
      shard_util.balance_eg_tests_into_sublists(small_suite, 5)
    self.assertIn('Shards: 5 Test Units: 2', str(ctx.exception))

    with self.assertRaises(shard_util.ExcessShardsError) as ctx:
      shard_util.balance_eg_tests_into_sublists([], 1)
    self.assertIn('Shards: 1 Test Units: 0', str(ctx.exception))

    # Single 20-method class: valid on 5 shards (4 methods/chunk), excess on 6.
    single_class = _make_suite({'Solo20': 20})
    sublists_5 = shard_util.balance_eg_tests_into_sublists(single_class, 5)
    self._assert_valid_partition(single_class, sublists_5)
    self.assertEqual([len(s) for s in sublists_5], [4, 4, 4, 4, 4])

    with self.assertRaises(shard_util.ExcessShardsError) as ctx:
      shard_util.balance_eg_tests_into_sublists(single_class, 6)
    self.assertIn('Shards: 6 Test Units: 5', str(ctx.exception))

  def test_objc_inheritance_parent_and_child_independent(self):
    """Tests inherited methods partition independently per emitted class."""
    shared_methods = [f'test{chr(ord("A") + i)}' for i in range(20)]
    suite = (
      [('ParentTestCase', m) for m in shared_methods]
      + [('ChildTestCase', m) for m in shared_methods]
      + [('ChildTestCase', 'testChildOnly')]
      + _make_suite({'SmallA': 3, 'SmallB': 3})
    )

    sublists = shard_util.balance_eg_tests_into_sublists(suite, 4)
    self._assert_valid_partition(suite, sublists)

    all_entries = [entry for shard in sublists for entry in shard]
    parent_entries = [e for e in all_entries if e.startswith('ParentTestCase/')]
    child_entries = [e for e in all_entries if e.startswith('ChildTestCase/')]
    self.assertEqual(len(parent_entries), 20)
    self.assertEqual(len(child_entries), 21)
    self.assertNotIn('ParentTestCase', all_entries)
    self.assertNotIn('ChildTestCase', all_entries)

  def test_shard_eg_test_cases_env_integration_and_bounds(self):
    """Tests shard_eg_test_cases env var integration and bounds validation."""
    suite = _make_suite(
      {'BigA': 25, 'BigB': 18, 'SmallA': 4, 'SmallB': 3, 'SmallC': 2}
    )
    expected = shard_util.balance_eg_tests_into_sublists(suite, 4)
    collected = []
    for i in range(4):
      with mock.patch.dict(
        os.environ, {'GTEST_SHARD_INDEX': str(i), 'GTEST_TOTAL_SHARDS': '4'}
      ):
        shard_entries = shard_util.shard_eg_test_cases(suite)
      self.assertEqual(shard_entries, expected[i])
      collected.append(shard_entries)
    self._assert_valid_partition(suite, collected)

    # Default env (GTEST_* unset) returns single-shard compact list.
    env_without_gtest = {
      k: v
      for k, v in os.environ.items()
      if k not in ('GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS')
    }
    with mock.patch.dict(os.environ, env_without_gtest, clear=True):
      single_shard = shard_util.shard_eg_test_cases(suite)
    self.assertEqual(
      single_shard, ['BigA', 'BigB', 'SmallA', 'SmallB', 'SmallC']
    )

    for env_override in (
      {'GTEST_SHARD_INDEX': '-1', 'GTEST_TOTAL_SHARDS': '4'},
      {'GTEST_SHARD_INDEX': '4', 'GTEST_TOTAL_SHARDS': '4'},
      {'GTEST_SHARD_INDEX': '0', 'GTEST_TOTAL_SHARDS': '0'},
    ):
      with mock.patch.dict(os.environ, env_override):
        with self.assertRaises(shard_util.ShardingError) as ctx:
          shard_util.shard_eg_test_cases(suite)
        self.assertNotIsInstance(ctx.exception, shard_util.ExcessShardsError)


if __name__ == '__main__':
  unittest.main()
