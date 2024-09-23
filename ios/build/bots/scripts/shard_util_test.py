#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from mock import patch
import os
import unittest

import shard_util

# example output of
# xcodebuild_runner.SimulatorParallelTestRunner.fetch_test_names
_EXAMPLE_FETCH_TEST_NAMES_RESPONSE = [
    ('CacheTestCase', 'testA'),
    ('CacheTestCase', 'testB'),
    ('CacheTestCase', 'testC'),
    ('TabUITestCase', 'testD'),
    ('TabUITestCase', 'testE'),
    ('KeyboardTestCase', 'testF'),
    ('PasswordManagerTestCase', 'testG'),
    ('ToolBarTestCase', 'testH'),
    # in this scenario PasswordManagerPasswordCheckupDisabledTestCase inherits
    # from PasswordManagerTestCase so it will run testG
    ('PasswordManagerPasswordCheckupDisabledTestCase', 'testI'),
    ('PasswordManagerPasswordCheckupDisabledTestCase', 'testG'),
]


class TestShardUtil(unittest.TestCase):
  """Test cases for shard_util.py"""

  def test_balance_into_sublists_debug(self):
    """Ensure the balancing algorithm works"""
    test_cases = [
        test_case for (test_case, _) in _EXAMPLE_FETCH_TEST_NAMES_RESPONSE
    ]
    test_counts = collections.Counter(test_cases)

    sublists_1 = shard_util.balance_into_sublists(test_counts, 1)
    self.assertEqual(len(sublists_1), 1)
    self.assertEqual(len(sublists_1[0]), 6)

    sublists_3 = shard_util.balance_into_sublists(test_counts, 3)
    self.assertEqual(len(sublists_3), 3)
    # CacheTestCase has 3,
    # TabUITestCase has 2, ToolBarTestCase has 1
    # PasswordManagerTestCase has 2, KeyboardTestCase has 1
    # PasswordManagerPasswordCheckupDisabledTestCase has 2 (due to inheritance)
    # They will be balanced into:
    # [CacheTestCase, ToolBarTestCase],
    # [[TabUITestCase, KeyboardTestCase],
    # [PasswordManagerPasswordCheckupDisabledTestCase, PasswordManagerTestCase]]
    self.assertEqual(
        sorted([len(sublists_3[0]),
                len(sublists_3[1]),
                len(sublists_3[2])]), [2, 2, 2])


if __name__ == '__main__':
  unittest.main()
