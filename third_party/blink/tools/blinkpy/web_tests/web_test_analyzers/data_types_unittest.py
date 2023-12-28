# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import typing
import unittest

from blinkpy.web_tests.web_test_analyzers import data_types


class FuzzyDiffAnalyzerDataTypesUnittest(unittest.TestCase):
    def testTypTagsTupleEnforced(self) -> None:
        """Tests that typ tags must be in a tuple."""
        fake_typ_tuple = typing.cast(tuple, ['win', 'x86'])
        with self.assertRaises(AssertionError):
            _ = data_types.Result('test', fake_typ_tuple, (1, 10), 'build_id')

    def testImageDiffTupleEnforced(self) -> None:
        """Tests that image diff tag must be in a tuple."""
        fake_image_diff_tuple = typing.cast(tuple, [1, 10])
        with self.assertRaises(AssertionError):
            _ = data_types.Result('test', ('win', 'x86'),
                                  fake_image_diff_tuple, 'build_id')

    def testImageDiffLengthEnforced(self) -> None:
        """Tests that image diff tag must be 2 length."""
        with self.assertRaises(AssertionError):
            _ = data_types.Result('test', ('win', 'x86'), (1, 2, 3),
                                  'build_id')

    def testWildcardsDisallowed(self) -> None:
        with self.assertRaises(AssertionError):
            _ = data_types.Result('t*', ('win', 'x86'), (1, 10), 'id')

    def testHashability(self) -> None:
        """Tests that Result objects are hashable."""
        r = data_types.Result('test_1', ('win', 'x86'), (1, 10), 'id')
        test_set = set([r])
        test_set.add(r)
        self.assertEqual(1, len(test_set))

        r = data_types.Result('test_2', ('win', 'x86'), (2, 30), 'id')
        test_set.add(r)
        self.assertEqual(2, len(test_set))

    def testEquality(self) -> None:
        """Tests that equality is properly calculated."""
        r = data_types.Result('test_1', ('win', 'x86'), (1, 10), 'id')
        other = data_types.Result('test_1', ('win', 'x86'), (1, 10), 'id')
        self.assertEqual(r, other)

        other = data_types.Result('test_2', ('win', 'x86'), (1, 10), 'id')
        self.assertNotEqual(r, other)

        other = data_types.Result('test_1', ('win', 'arm64'), (1, 10), 'id')
        self.assertNotEqual(r, other)

        other = data_types.Result('test_1', ('win', 'x86'), (2, 11), 'id')
        self.assertNotEqual(r, other)

        other = data_types.Result('test_1', ('win', 'x86'), (1, 10), 'id_2')
        self.assertNotEqual(r, other)

        other = None
        self.assertNotEqual(r, other)
