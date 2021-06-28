#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys
import unittest

if sys.version_info[0] == 2:
  import mock
else:
  import unittest.mock as mock

from unexpected_passes_common import data_types

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
    other = data_types.Expectation('test', ['tag1', 'tag2'], 'Pass', 'bug')
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


class MapTypeUnittest(unittest.TestCase):
  def testMapConstructor(self):
    """Tests that constructors enforce type."""
    # We only use one map type since they all share the same implementation for
    # this logic.
    with self.assertRaises(AssertionError):
      data_types.StepBuildStatsMap({1: 2})
    m = data_types.StepBuildStatsMap({'step': data_types.BuildStats()})
    self.assertEqual(m, {'step': data_types.BuildStats()})

  def testMapUpdate(self):
    """Tests that update() enforces type."""
    # We only use one map type since they all share the same implementation for
    # this logic.
    m = data_types.StepBuildStatsMap({'step': data_types.BuildStats()})
    with self.assertRaises(AssertionError):
      m.update({1: 2})
    with self.assertRaises(AssertionError):
      m.update(step2=1)
    m.update(step=data_types.BuildStats())
    self.assertEqual(m, {'step': data_types.BuildStats()})

  def testMapSetdefault(self):
    """Tests that setdefault() enforces type."""
    # We only use one map type since they all share the same implementation for
    # this logic.
    m = data_types.StepBuildStatsMap()
    with self.assertRaises(AssertionError):
      m.setdefault(1, data_types.BuildStats())
    with self.assertRaises(AssertionError):
      m.setdefault('1', 2)
    m.setdefault('1', data_types.BuildStats())
    self.assertEqual(m, {'1': data_types.BuildStats()})

  def _StringToMapHelper(self, map_type, value_type):
    """Helper function for testing string type -> map type enforcement."""
    m = map_type()
    with self.assertRaises(AssertionError):
      m[1] = value_type()
    with self.assertRaises(AssertionError):
      m['1'] = 2
    m['1'] = value_type()
    self.assertEqual(m, {'1': value_type()})
    m[u'2'] = value_type()
    self.assertEqual(m, {'1': value_type(), u'2': value_type()})

  def testStepBuildStatsMap(self):
    """Tests StepBuildStats' type enforcement."""
    self._StringToMapHelper(data_types.StepBuildStatsMap, data_types.BuildStats)

  def testBuilderStepMap(self):
    """Tests BuilderStepMap's type enforcement."""
    self._StringToMapHelper(data_types.BuilderStepMap,
                            data_types.StepBuildStatsMap)

  def testExpectationBuilderMap(self):
    """Tests ExpectationBuilderMap's type enforcement."""
    m = data_types.ExpectationBuilderMap()
    e = data_types.Expectation('test', ['tag'], 'Failure')
    with self.assertRaises(AssertionError):
      m[1] = data_types.BuilderStepMap()
    with self.assertRaises(AssertionError):
      m[e] = 2
    m[e] = data_types.BuilderStepMap()
    self.assertEqual(m, {e: data_types.BuilderStepMap()})

  def testTestExpectationMap(self):
    """Tests TestExpectationMap's type enforcement."""
    self._StringToMapHelper(data_types.TestExpectationMap,
                            data_types.ExpectationBuilderMap)

  def _GetSampleBuildStats(self):
    build_stats = []
    for i in range(8):
      bs = data_types.BuildStats()
      for _ in range(i):
        bs.AddPassedBuild()
      build_stats.append(bs)
    return build_stats

  def _GetSampleTestExpectationMap(self):
    build_stats = self._GetSampleBuildStats()
    return data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['tag'], ['Failure']):
            data_types.BuilderStepMap({
                'builder1':
                data_types.StepBuildStatsMap({
                    'step1': build_stats[0],
                    'step2': build_stats[1],
                }),
                'builder2':
                data_types.StepBuildStatsMap({
                    'step3': build_stats[2],
                    'step4': build_stats[3],
                }),
            }),
            data_types.Expectation('foo', ['tag2'], ['Failure']):
            data_types.BuilderStepMap({
                'builder3':
                data_types.StepBuildStatsMap({
                    'step5': build_stats[4],
                    'step6': build_stats[5],
                }),
                'builder4':
                data_types.StepBuildStatsMap({
                    'step7': build_stats[6],
                    'step8': build_stats[7],
                }),
            }),
        }),
    })

  def testIterBuilderStepMaps(self):
    """Tests that iterating to BuilderStepMap works as expected."""
    test_expectation_map = self._GetSampleTestExpectationMap()
    expected_values = []
    for test_name, expectation_map in test_expectation_map.items():
      for expectation, builder_map in expectation_map.items():
        expected_values.append((test_name, expectation, builder_map))
    returned_values = []
    for (test_name, expectation,
         builder_map) in test_expectation_map.IterBuilderStepMaps():
      returned_values.append((test_name, expectation, builder_map))
    self.assertEqual(len(returned_values), len(expected_values))
    for rv in returned_values:
      self.assertIn(rv, expected_values)
      self.assertIsInstance(rv[-1], data_types.BuilderStepMap)

  def testIterToNoSuchValue(self):
    """Tests that iterating to a type that has no data works as expected."""
    test_expectation_map = data_types.TestExpectationMap()
    # This should neither break nor return any data.
    for _, __, ___ in test_expectation_map.IterBuilderStepMaps():
      self.fail()

  def testIterToNoSuchType(self):
    """Tests that an error is raised if no such type is found when iterating."""
    test_expectation_map = self._GetSampleBuildStats()
    with self.assertRaises(AttributeError):
      test_expectation_map.IterToValueType(int)


if __name__ == '__main__':
  unittest.main(verbosity=2)
