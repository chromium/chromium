#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the web test stale expectation remover data types."""

import unittest

from blinkpy.web_tests.stale_expectation_removal import data_types


class WebTestExpectationUnittest(unittest.TestCase):
    def testCompareWildcard(self):
        """Tests that wildcard comparisons work as expected."""
        e = data_types.WebTestExpectation('test*', ['tag1'], 'Failure')
        self.assertTrue(e._CompareWildcard('testing123'))
        self.assertTrue(
            e._CompareWildcard('virtual/some-identifier/testing123'))
        self.assertTrue(e._CompareWildcard('test'))
        self.assertTrue(e._CompareWildcard('virtual/some-identifier/test'))
        self.assertFalse(e._CompareWildcard('tes'))
        self.assertFalse(e._CompareWildcard('/virtual/some-identifier/test'))
        self.assertFalse(e._CompareWildcard('virtual/some/malformed/test'))

    def testCompareNonWildcard(self):
        """Tests that non-wildcard comparisons work as expected."""
        e = data_types.WebTestExpectation('test', ['tag1'], 'Failure')
        self.assertTrue(e._CompareNonWildcard('test'))
        self.assertTrue(e._CompareNonWildcard('virtual/some-identifier/test'))
        self.assertFalse(e._CompareNonWildcard('tes'))
        self.assertFalse(
            e._CompareNonWildcard('/virtual/some-identifier/test'))
        self.assertFalse(e._CompareNonWildcard('virtual/some/malformed/test'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
