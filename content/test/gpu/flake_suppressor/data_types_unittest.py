#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from flake_suppressor import data_types


class ExpectationUnittest(unittest.TestCase):
  def testAppliesToResultNonResult(self):
    """Tests that AppliesToResult properly fails when given a non-Result."""
    e = data_types.Expectation('test', ['win', 'nvidia'], ['Failure'])
    with self.assertRaises(AssertionError):
      e.AppliesToResult(None)

  def testAppliesToResultApplies(self):
    """Tests that AppliesToResult properly returns True on expected Results."""
    # Exact match.
    e = data_types.Expectation('test', ['win', 'nvidia'], ['Failure'])
    r = data_types.Result('suite', 'test', ('win', 'nvidia'), 'id')
    self.assertTrue(e.AppliesToResult(r))
    # Tag subset
    r = data_types.Result('suite', 'test', ('win', 'nvidia', 'release'), 'id')
    self.assertTrue(e.AppliesToResult(r))
    # Glob match
    e = data_types.Expectation('t*', ['win', 'nvidia'], ['Failure'])
    self.assertTrue(e.AppliesToResult(r))

  def testAppliesToResultDoesNotApply(self):
    """Tests that AppliesToResult properly returns False on expected Results."""
    # Name mismatch
    e = data_types.Expectation('test', ['win', 'nvidia'], ['Failure'])
    r = data_types.Result('suite', 'notatest', ('win', 'nvidia'), 'id')
    self.assertFalse(e.AppliesToResult(r))
    # Tag superset
    r = data_types.Result('suite', 'test', tuple(['win']), 'id')
    self.assertFalse(e.AppliesToResult(r))


class ResultUnittest(unittest.TestCase):
  def testTupleEnforced(self):
    """Tests that tags must be in a tuple."""
    with self.assertRaises(AssertionError):
      _ = data_types.Result('suite', 'test', ['win', 'nvidia'], 'id')

  def testWildcardsDisallowed(self):
    with self.assertRaises(AssertionError):
      _ = data_types.Result('suite', 't*', ('win', 'nvidia'), 'id')

  def testHashability(self):  # pylint: disable=no-self-use
    """Tests that Result objects are hashable."""
    r = data_types.Result('suite', 'test', ('win', 'nvidia'), 'id')
    _ = set([r])

  def testEquality(self):
    """Tests that equality is properly calculated."""
    r = data_types.Result('suite', 'test', ('win', 'nvidia'), 'id')
    other = data_types.Result('suite', 'test', ('win', 'nvidia'), 'id')
    self.assertEqual(r, other)

    other = data_types.Result('notsuite', 'test', ('win', 'nvidia'), 'id')
    self.assertNotEqual(r, other)

    other = data_types.Result('suite', 'nottest', ('win', 'nvidia'), 'id')
    self.assertNotEqual(r, other)

    other = data_types.Result('suite', 'test', tuple(['win']), 'id')
    self.assertNotEqual(r, other)

    other = data_types.Result('suite', 'test', ('win', 'nvidia'), 'notid')
    self.assertNotEqual(r, other)

    other = None
    self.assertNotEqual(r, other)


if __name__ == '__main__':
  unittest.main(verbosity=2)
