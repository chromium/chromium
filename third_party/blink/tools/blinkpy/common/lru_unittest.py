# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.lru import LRUMapping


class LRUMappingTest(unittest.TestCase):

    def test_reorder_get(self):
        mapping = LRUMapping(2)
        mapping[1] = 1
        mapping[2] = 2
        self.assertEqual(list(mapping), [2, 1])
        self.assertEqual(mapping[1], 1)
        mapping[3] = 3
        self.assertEqual(list(mapping), [3, 1])

    def test_reorder_set(self):
        mapping = LRUMapping(2)
        mapping[1] = 1
        mapping[2] = 2
        self.assertEqual(list(mapping), [2, 1])
        mapping[1] = 1
        mapping[3] = 3
        self.assertEqual(list(mapping), [3, 1])

    def test_evict(self):
        mapping = LRUMapping(2)
        for i in range(4):
            mapping[i] = i
        self.assertEqual(list(mapping), [3, 2])

    def test_update_capacity(self):
        mapping = LRUMapping(1)
        with self.assertRaises(ValueError):
            mapping.capacity = -1
        mapping.capacity = 2
        mapping[1] = 1
        mapping[2] = 2
        self.assertEqual(list(mapping), [2, 1])
