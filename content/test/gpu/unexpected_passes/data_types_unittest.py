# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mock

from unexpected_passes import data_types

GENERIC_EXPECTATION = data_types.Expectation('test', ['tag1', 'tag2'], ['Pass'])
GENERIC_RESULT = data_types.Result('test', ['tag1', 'tag2'], 'Pass',
                                   'pixel_tests', 'build_id')


class ExpectationUnittest(unittest.TestCase):
  def testEquality(self):
    e = GENERIC_EXPECTATION
    other = data_types.Expectation('test', ['tag1', 'tag2'], 'Pass')
    self.assertEqual(e, other)
    other = data_types.Expectation('test2', ['tag1', 'tag2'], 'Pass')
    self.assertNotEqual(e, other)
    other = data_types.Expectation('test', ['tag1'], 'Pass')
    self.assertNotEqual(e, other)
    other = data_types.Expectation('test', ['tag1', 'tag2'], 'Failure')
    self.assertNotEqual(e, other)
    other = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                              'build_id')
    self.assertNotEqual(e, other)

  def testHashability(self):
    e = GENERIC_EXPECTATION
    _ = {e}

  def testAppliesToResultNonResult(self):
    e = GENERIC_EXPECTATION
    with self.assertRaises(AssertionError):
      e.AppliesToResult(e)

  def testAppliesToResultApplies(self):
    r = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                          'build_id')
    # Exact name match, exact tag match.
    e = GENERIC_EXPECTATION
    self.assertTrue(e.AppliesToResult(r))
    # Glob name match, exact tag match.
    e = data_types.Expectation('te*', ['tag1', 'tag2'], 'Pass')
    self.assertTrue(e.AppliesToResult(r))
    # Exact name match, tag subset match.
    e = data_types.Expectation('test', ['tag1'], 'Pass')
    self.assertTrue(e.AppliesToResult(r))
    # Expected result subset match.
    r = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                          'build_id')
    e = GENERIC_EXPECTATION
    self.assertTrue(e.AppliesToResult(r))
    e = data_types.Expectation('test', ['tag1', 'tag2'], ['RetryOnFailure'])
    self.assertTrue(e.AppliesToResult(r))

  def testAppliesToResultDoesNotApply(self):
    r = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                          'build_id')
    # Exact name mismatch.
    e = data_types.Expectation('te', ['tag1', 'tag2'], 'Pass')
    self.assertFalse(e.AppliesToResult(r))
    # Glob name mismatch.
    e = data_types.Expectation('ta*', ['tag1', 'tag2'], 'Pass')
    self.assertFalse(e.AppliesToResult(r))
    # Tags subset mismatch.
    e = data_types.Expectation('test', ['tag3'], 'Pass')
    self.assertFalse(e.AppliesToResult(r))


class ResultUnittest(unittest.TestCase):
  def testWildcardsDisallowed(self):
    with self.assertRaises(AssertionError):
      data_types.Result('*', ['tag1'], 'Pass', 'pixel_tests', 'build_id')

  def testEquality(self):
    r = GENERIC_RESULT
    other = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                              'build_id')
    self.assertEqual(r, other)
    other = data_types.Result('test2', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                              'build_id')
    self.assertNotEqual(r, other)
    other = data_types.Result('test', ['tag1'], 'Pass', 'pixel_tests',
                              'build_id')
    self.assertNotEqual(r, other)
    other = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                              'build_id')
    self.assertNotEqual(r, other)
    other = data_types.Result('test', ['tag1', 'tag2'], 'Failure',
                              'pixel_tests', 'build_id')
    self.assertNotEqual(r, other)
    other = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'webgl_tests',
                              'build_id')
    self.assertNotEqual(r, other)
    other = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                              'other_build_id')
    self.assertNotEqual(r, other)
    other = data_types.Expectation('test', ['tag1', 'tag2'], 'Pass')
    self.assertNotEqual(r, other)

  def testHashability(self):
    r = GENERIC_RESULT
    _ = {r}


class BuildStatsUnittest(unittest.TestCase):
  def CreateGenericBuildStats(self):
    stats = data_types.BuildStats()
    stats.AddPassedBuild()
    stats.AddFailedBuild('')
    return stats

  def testEquality(self):
    s = self.CreateGenericBuildStats()
    other = self.CreateGenericBuildStats()
    self.assertEqual(s, other)
    other.passed_builds = 0
    self.assertNotEqual(s, other)
    other = self.CreateGenericBuildStats()
    other.total_builds = 0
    self.assertNotEqual(s, other)
    other = self.CreateGenericBuildStats()
    other.failure_links = frozenset()
    self.assertNotEqual(s, other)

  def testAddFailedBuild(self):
    s = data_types.BuildStats()
    s.AddFailedBuild('build_id')
    self.assertEqual(s.total_builds, 1)
    self.assertEqual(s.failed_builds, 1)
    self.assertEqual(s.failure_links,
                     frozenset(['http://ci.chromium.org/b/build_id']))


if __name__ == '__main__':
  unittest.main(verbosity=2)
