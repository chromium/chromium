#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import typing
import unittest

from flake_suppressor_common import data_types


class ExpectationUnittest(unittest.TestCase):
  def testAppliesToResultNonResult(self) -> None:
    """Tests that AppliesToResult properly fails when given a non-Result."""
    e = data_types.Expectation('test', ['win', 'nvidia'], ['Failure'])
    fake_result = typing.cast(data_types.Result, None)
    with self.assertRaises(AssertionError):
      e.AppliesToResult(fake_result)

  def testAppliesToResultApplies(self) -> None:
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

  def testAppliesToResultDoesNotApply(self) -> None:
    """Tests that AppliesToResult properly returns False on expected Results."""
    # Name mismatch
    e = data_types.Expectation('test', ['win', 'nvidia'], ['Failure'])
    r = data_types.Result('suite', 'notatest', ('win', 'nvidia'), 'id')
    self.assertFalse(e.AppliesToResult(r))
    # Tag superset
    r = data_types.Result('suite', 'test', tuple(['win']), 'id')
    self.assertFalse(e.AppliesToResult(r))


class ResultUnittest(unittest.TestCase):
  def testTupleEnforced(self) -> None:
    """Tests that tags must be in a tuple."""
    fake_tuple = typing.cast(tuple, ['win', 'nvidia'])
    with self.assertRaises(AssertionError):
      _ = data_types.Result('suite', 'test', fake_tuple, 'id')

  def testWildcardsDisallowed(self) -> None:
    with self.assertRaises(AssertionError):
      _ = data_types.Result('suite', 't*', ('win', 'nvidia'), 'id')

  def testHashability(self) -> None:  # pylint: disable=no-self-use
    """Tests that Result objects are hashable."""
    r = data_types.Result('suite', 'test', ('win', 'nvidia'), 'id')
    _ = set([r])

  def testEquality(self) -> None:
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
