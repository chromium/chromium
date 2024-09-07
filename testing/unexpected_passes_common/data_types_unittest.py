#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import typing
from typing import Dict, List
import unittest
from unittest import mock

from unexpected_passes_common import constants
from unexpected_passes_common import data_types
from unexpected_passes_common import unittest_utils as uu

# Protected access is allowed for unittests.
# pylint: disable=protected-access

GENERIC_EXPECTATION = data_types.Expectation('test', ['tag1', 'tag2'], ['Pass'])
GENERIC_RESULT = data_types.Result('test', ['tag1', 'tag2'], 'Pass',
                                   'pixel_tests', 'build_id')


class CustomImplementationUnittest(unittest.TestCase):
  def testCustomExpectation(self) -> None:
    class CustomExpectation(data_types.BaseExpectation):
      pass

    data_types.SetExpectationImplementation(CustomExpectation)
    expectation = data_types.Expectation('test', ['tag1', 'tag2'], 'Pass')
    self.assertIsInstance(expectation, CustomExpectation)

  def testCustomResult(self) -> None:
    class CustomResult(data_types.BaseResult):
      pass

    data_types.SetResultImplementation(CustomResult)
    result = data_types.Result('test', ['tag1', 'tag2'], 'Pass', 'pixel_tests',
                               'build_id')
    self.assertIsInstance(result, CustomResult)

  def testCustomBuildStats(self) -> None:
    class CustomBuildStats(data_types.BaseBuildStats):
      pass

    data_types.SetBuildStatsImplementation(CustomBuildStats)
    build_stats = data_types.BuildStats()
    self.assertIsInstance(build_stats, CustomBuildStats)

  def testCustomTestExpectationMap(self) -> None:
    class CustomTestExpectationMap(data_types.BaseTestExpectationMap):
      pass

    data_types.SetTestExpectationMapImplementation(CustomTestExpectationMap)
    expectation_map = data_types.TestExpectationMap()
    self.assertIsInstance(expectation_map, CustomTestExpectationMap)


class ExpectationUnittest(unittest.TestCase):
  def testEquality(self) -> None:
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

  def testHashability(self) -> None:
    e = GENERIC_EXPECTATION
    _ = {e}

  def testAppliesToResultNonResult(self) -> None:
    e = GENERIC_EXPECTATION
    with self.assertRaises(AssertionError):
      e.AppliesToResult(typing.cast(data_types.Result, e))

  def testAppliesToResultApplies(self) -> None:
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

  def testAppliesToResultDoesNotApply(self) -> None:
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

  def testAppliesToResultResultHasAsterisk(self) -> None:
    r = data_types.Result('foo.html?include=*', ['tag1', 'tag2'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('*', ['tag1', 'tag2'], 'Pass')
    self.assertTrue(e.AppliesToResult(r))
    e = data_types.Expectation('foo.html?include=*', ['tag1', 'tag2'], 'Pass')
    self.assertTrue(e.AppliesToResult(r))
    e = data_types.Expectation('foo.html?include=bar*', ['tag1', 'tag2'],
                               'Pass')
    self.assertFalse(e.AppliesToResult(r))

  def testAsExpectationFileString(self) -> None:
    e = data_types.Expectation('foo/test', ['tag2', 'tag1'], 'Failure')
    self.assertEqual(e.AsExpectationFileString(),
                     '[ tag1 tag2 ] foo/test [ Failure ]')
    e = data_types.Expectation('foo/test', ['tag2', 'tag1'], 'Failure', 'bug')
    self.assertEqual(e.AsExpectationFileString(),
                     'bug [ tag1 tag2 ] foo/test [ Failure ]')
    e = data_types.Expectation('foo/*', ['tag2', 'tag1'], 'Failure', 'bug')
    self.assertEqual(e.AsExpectationFileString(),
                     'bug [ tag1 tag2 ] foo/* [ Failure ]')

  def testWildcard(self) -> None:
    e = data_types.Expectation('foo/test', ['tag1'], 'Failure')
    self.assertFalse(e._IsWildcard())
    e = data_types.Expectation('foo/\\*', ['tag1'], 'Failure')
    self.assertFalse(e._IsWildcard())
    e = data_types.Expectation('foo/*', ['tag1'], 'Failure')
    self.assertTrue(e._IsWildcard())
    e = data_types.Expectation('foo/\\*bar/*', ['tag1'], 'Failure')
    self.assertTrue(e._IsWildcard())


class ResultUnittest(unittest.TestCase):
  def testEquality(self) -> None:
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

  def testHashability(self) -> None:
    r = GENERIC_RESULT
    _ = {r}


class BuildStatsUnittest(unittest.TestCase):
  def CreateGenericBuildStats(self) -> data_types.BuildStats:
    stats = data_types.BuildStats()
    stats.AddPassedBuild(frozenset())
    stats.AddFailedBuild('', frozenset())
    return stats

  def testEquality(self) -> None:
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
    other = self.CreateGenericBuildStats()
    other.tag_sets = {frozenset(['tag'])}
    self.assertNotEqual(s, other)

  def testAddPassedBuild(self) -> None:
    s = data_types.BuildStats()
    s.AddPassedBuild(frozenset(['tag']))
    s.AddPassedBuild(frozenset(['other_tag']))
    self.assertEqual(s.total_builds, 2)
    self.assertEqual(s.failed_builds, 0)
    self.assertEqual(s.failure_links, set())
    self.assertEqual(s.tag_sets, {frozenset(['tag']), frozenset(['other_tag'])})

  def testAddFailedBuild(self) -> None:
    s = data_types.BuildStats()
    s.AddFailedBuild('build_id', frozenset(['tag']))
    s.AddFailedBuild('build_id', frozenset(['other_tag']))
    self.assertEqual(s.total_builds, 2)
    self.assertEqual(s.failed_builds, 2)
    self.assertEqual(s.failure_links, {'http://ci.chromium.org/b/build_id'})
    self.assertEqual(s.tag_sets, {frozenset(['tag']), frozenset(['other_tag'])})

  def testGetStatsAsString(self) -> None:
    s = self.CreateGenericBuildStats()
    expected_str = '(1/2 passed)'
    self.assertEqual(s.GetStatsAsString(), expected_str)


class MapTypeUnittest(unittest.TestCase):
  def testMapConstructor(self) -> None:
    """Tests that constructors enforce type."""
    # We only use one map type since they all share the same implementation for
    # this logic.
    with self.assertRaises(AssertionError):
      data_types.StepBuildStatsMap({1: 2})
    m = data_types.StepBuildStatsMap({'step': data_types.BuildStats()})
    self.assertEqual(m, {'step': data_types.BuildStats()})

  def testMapUpdate(self) -> None:
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

  def testMapSetdefault(self) -> None:
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

  def _StringToMapHelper(self, map_type: type, value_type: type) -> None:
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

  def testStepBuildStatsMap(self) -> None:
    """Tests StepBuildStats' type enforcement."""
    self._StringToMapHelper(data_types.StepBuildStatsMap, data_types.BuildStats)

  def testBuilderStepMap(self) -> None:
    """Tests BuilderStepMap's type enforcement."""
    self._StringToMapHelper(data_types.BuilderStepMap,
                            data_types.StepBuildStatsMap)

  def testExpectationBuilderMap(self) -> None:
    """Tests ExpectationBuilderMap's type enforcement."""
    m = data_types.ExpectationBuilderMap()
    e = data_types.Expectation('test', ['tag'], 'Failure')
    with self.assertRaises(AssertionError):
      m[typing.cast(data_types.BaseExpectation,
                    1)] = data_types.BuilderStepMap()
    with self.assertRaises(AssertionError):
      m[e] = typing.cast(data_types.BuilderStepMap, 2)
    m[e] = data_types.BuilderStepMap()
    self.assertEqual(m, {e: data_types.BuilderStepMap()})

  def testTestExpectationMap(self) -> None:
    """Tests TestExpectationMap's type enforcement."""
    self._StringToMapHelper(data_types.TestExpectationMap,
                            data_types.ExpectationBuilderMap)

  def _GetSampleBuildStats(self) -> List[data_types.BuildStats]:
    build_stats = []
    for i in range(8):
      bs = data_types.BuildStats()
      for _ in range(i):
        bs.AddPassedBuild(frozenset())
      build_stats.append(bs)
    return build_stats

  def _GetSampleTestExpectationMap(self) -> data_types.TestExpectationMap:
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

  def testIterBuilderStepMaps(self) -> None:
    """Tests that iterating to BuilderStepMap works as expected."""
    test_expectation_map = self._GetSampleTestExpectationMap()
    expected_values = []
    for expectation_file, expectation_map in test_expectation_map.items():
      for expectation, builder_map in expectation_map.items():
        expected_values.append((expectation_file, expectation, builder_map))
    returned_values = []
    for (expectation_file, expectation,
         builder_map) in test_expectation_map.IterBuilderStepMaps():
      returned_values.append((expectation_file, expectation, builder_map))
    self.assertEqual(len(returned_values), len(expected_values))
    for rv in returned_values:
      self.assertIn(rv, expected_values)
      self.assertIsInstance(rv[-1], data_types.BuilderStepMap)

  def testIterToNoSuchValue(self) -> None:
    """Tests that iterating to a type that has no data works as expected."""
    test_expectation_map = data_types.TestExpectationMap()
    # This should neither break nor return any data.
    for _, __, ___ in test_expectation_map.IterBuilderStepMaps():
      self.fail()

  # TODO(bsheedy): Test is temporarily disabled because no AttributeError is
  # raised when an object of the correct type is used for test_expectation_map.
  #
  # def testIterToNoSuchType(self):
  #   """Tests that an error is raised if no such type is found when
  #   iterating."""
  #   test_expectation_map = self._GetSampleTestExpectationMap()
  #   with self.assertRaises(AttributeError):
  #     test_expectation_map.IterToValueType(int)


class TypedMapMergeUnittest(unittest.TestCase):
  def testEmptyBaseMap(self) -> None:
    """Tests that a merge with an empty base map copies the merge map."""
    base_map = data_types.TestExpectationMap()
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    original_merge_map = copy.deepcopy(merge_map)
    base_map.Merge(merge_map)
    self.assertEqual(base_map, merge_map)
    self.assertEqual(merge_map, original_merge_map)

  def testEmptyMergeMap(self) -> None:
    """Tests that a merge with an empty merge map is a no-op."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_map = data_types.TestExpectationMap()
    original_base_map = copy.deepcopy(base_map)
    base_map.Merge(merge_map)
    self.assertEqual(base_map, original_base_map)
    self.assertEqual(merge_map, {})

  def testMissingKeys(self) -> None:
    """Tests that missing keys are properly copied to the base map."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step2': data_types.BuildStats(),
                }),
                'builder2':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
            data_types.Expectation('foo', ['mac'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                })
            })
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    expected_base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                    'step2': data_types.BuildStats(),
                },
                'builder2': {
                    'step': data_types.BuildStats(),
                },
            },
            data_types.Expectation('foo', ['mac'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                }
            }
        },
        'bar': {
            data_types.Expectation('bar', ['win'], 'Failure'): {
                'builder': {
                    'step': data_types.BuildStats(),
                },
            },
        },
    }
    base_map.Merge(merge_map)
    self.assertEqual(base_map, expected_base_map)

  def testMergeBuildStats(self) -> None:
    """Tests that BuildStats for the same step are merged properly."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_stats = data_types.BuildStats()
    merge_stats.AddFailedBuild('1', frozenset())
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': merge_stats,
                }),
            }),
        }),
    })
    expected_stats = data_types.BuildStats()
    expected_stats.AddFailedBuild('1', frozenset())
    expected_base_map = {
        'foo': {
            data_types.Expectation('foo', ['win'], 'Failure'): {
                'builder': {
                    'step': expected_stats,
                },
            },
        },
    }
    base_map.Merge(merge_map)
    self.assertEqual(base_map, expected_base_map)

  def testInvalidMerge(self) -> None:
    """Tests that updating a BuildStats instance twice is an error."""
    base_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': data_types.BuildStats(),
                }),
            }),
        }),
    })
    merge_stats = data_types.BuildStats()
    merge_stats.AddFailedBuild('1', frozenset())
    merge_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], 'Failure'):
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'step': merge_stats,
                }),
            }),
        }),
    })
    original_base_map = copy.deepcopy(base_map)
    base_map.Merge(merge_map, original_base_map)
    with self.assertRaises(AssertionError):
      base_map.Merge(merge_map, original_base_map)


class TestExpectationMapAddResultListUnittest(unittest.TestCase):
  def GetGenericRetryExpectation(self) -> data_types.Expectation:
    return data_types.Expectation('foo/test', ['win10'], 'RetryOnFailure')

  def GetGenericFailureExpectation(self) -> data_types.Expectation:
    return data_types.Expectation('foo/test', ['win10'], 'Failure')

  def GetEmptyMapForGenericRetryExpectation(self
                                            ) -> data_types.TestExpectationMap:
    foo_expectation = self.GetGenericRetryExpectation()
    return data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            foo_expectation:
            data_types.BuilderStepMap(),
        }),
    })

  def GetEmptyMapForGenericFailureExpectation(
      self) -> data_types.TestExpectationMap:
    foo_expectation = self.GetGenericFailureExpectation()
    return data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            foo_expectation:
            data_types.BuilderStepMap(),
        }),
    })

  def GetPassedMapForExpectation(self, expectation: data_types.Expectation
                                 ) -> data_types.TestExpectationMap:
    stats = data_types.BuildStats()
    stats.AddPassedBuild(expectation.tags)
    return self.GetMapForExpectationAndStats(expectation, stats)

  def GetFailedMapForExpectation(self, expectation: data_types.Expectation
                                 ) -> data_types.TestExpectationMap:
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id', expectation.tags)
    return self.GetMapForExpectationAndStats(expectation, stats)

  def GetMapForExpectationAndStats(self, expectation: data_types.Expectation,
                                   stats: data_types.BuildStats
                                   ) -> data_types.TestExpectationMap:
    return data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            expectation:
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'pixel_tests': stats,
                }),
            }),
        }),
    })

  def testRetryOnlyPassMatching(self) -> None:
    """Tests when the only tests are retry expectations that pass and match."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = expectation_map.AddResultList('builder', [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testRetryOnlyFailMatching(self) -> None:
    """Tests when the only tests are retry expectations that fail and match."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                   'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = expectation_map.AddResultList('builder', [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetFailedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testRetryFailThenPassMatching(self) -> None:
    """Tests when there are pass and fail results for retry expectations."""
    foo_fail_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                        'pixel_tests', 'build_id')
    foo_pass_result = data_types.Result('foo/test', ['win10'], 'Pass',
                                        'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericRetryExpectation()
    unmatched_results = expectation_map.AddResultList(
        'builder', [foo_fail_result, foo_pass_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetFailedMapForExpectation(
        self.GetGenericRetryExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testFailurePassMatching(self) -> None:
    """Tests when there are pass results for failure expectations."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = expectation_map.AddResultList('builder', [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testFailureFailureMatching(self) -> None:
    """Tests when there are failure results for failure expectations."""
    foo_result = data_types.Result('foo/test', ['win10'], 'Failure',
                                   'pixel_tests', 'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = expectation_map.AddResultList('builder', [foo_result])
    self.assertEqual(unmatched_results, [])

    expected_expectation_map = self.GetFailedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)

  def testMismatches(self) -> None:
    """Tests that unmatched results get returned."""
    foo_match_result = data_types.Result('foo/test', ['win10'], 'Pass',
                                         'pixel_tests', 'build_id')
    foo_mismatch_result = data_types.Result('foo/not_a_test', ['win10'],
                                            'Failure', 'pixel_tests',
                                            'build_id')
    bar_result = data_types.Result('bar/test', ['win10'], 'Pass', 'pixel_tests',
                                   'build_id')
    expectation_map = self.GetEmptyMapForGenericFailureExpectation()
    unmatched_results = expectation_map.AddResultList(
        'builder', [foo_match_result, foo_mismatch_result, bar_result])
    self.assertEqual(len(set(unmatched_results)), 2)
    self.assertEqual(set(unmatched_results),
                     set([foo_mismatch_result, bar_result]))

    expected_expectation_map = self.GetPassedMapForExpectation(
        self.GetGenericFailureExpectation())
    self.assertEqual(expectation_map, expected_expectation_map)


class TestExpectationMapAddGroupedResultsUnittest(unittest.TestCase):
  def testResultMatchPassingNew(self) -> None:
    """Test adding a passing result when no results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
        }),
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set([r]))
    stats = data_types.BuildStats()
    stats.AddPassedBuild(frozenset(['win', 'win10']))
    expected_expectation_map = {
        'expectation_file': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchFailingNew(self) -> None:
    """Test adding a failing result when no results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Failure',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
        }),
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set([r]))
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id', frozenset(['win', 'win10']))
    expected_expectation_map = {
        'expectation_file': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            }
        }
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchPassingExisting(self) -> None:
    """Test adding a passing result when results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id', frozenset(['win', 'win10']))
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e:
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'pixel_tests': stats,
                }),
            }),
        }),
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set([r]))
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id', frozenset(['win', 'win10']))
    stats.AddPassedBuild(frozenset(['win', 'win10']))
    expected_expectation_map = {
        'expectation_file': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchFailingExisting(self) -> None:
    """Test adding a failing result when results for a builder exist."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Failure',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    stats = data_types.BuildStats()
    stats.AddPassedBuild(frozenset(['win', 'win10']))
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e:
            data_types.BuilderStepMap({
                'builder':
                data_types.StepBuildStatsMap({
                    'pixel_tests': stats,
                }),
            }),
        }),
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set([r]))
    stats = data_types.BuildStats()
    stats.AddFailedBuild('build_id', frozenset(['win', 'win10']))
    stats.AddPassedBuild(frozenset(['win', 'win10']))
    expected_expectation_map = {
        'expectation_file': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
        },
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchMultiMatch(self) -> None:
    """Test adding a passing result when multiple expectations match."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Pass',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10'], 'Failure')
    e2 = data_types.Expectation('some/test/case', ['win10'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
            e2: data_types.BuilderStepMap(),
        }),
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set([r]))
    stats = data_types.BuildStats()
    stats.AddPassedBuild(frozenset(['win', 'win10']))
    expected_expectation_map = {
        'expectation_file': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                },
            },
            e2: {
                'builder': {
                    'pixel_tests': stats,
                },
            }
        }
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultNoMatch(self) -> None:
    """Tests that a result is not added if no match is found."""
    r = data_types.Result('some/test/case', ['win', 'win10'], 'Failure',
                          'pixel_tests', 'build_id')
    e = data_types.Expectation('some/test/*', ['win10', 'foo'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e: data_types.BuilderStepMap(),
        })
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set())
    expected_expectation_map = {'expectation_file': {e: {}}}
    self.assertEqual(expectation_map, expected_expectation_map)

  def testResultMatchSpecificExpectationFiles(self) -> None:
    """Tests that a match can be found when specifying expectation files."""
    r = data_types.Result('some/test/case', ['win'], 'Pass', 'pixel_tests',
                          'build_id')
    e = data_types.Expectation('some/test/case', ['win'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'foo_expectations':
        data_types.ExpectationBuilderMap({e: data_types.BuilderStepMap()}),
        'bar_expectations':
        data_types.ExpectationBuilderMap({e: data_types.BuilderStepMap()}),
    })
    grouped_results = {
        'some/test/case': [r],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', ['bar_expectations'])
    self.assertEqual(matched_results, set([r]))
    stats = data_types.BuildStats()
    stats.AddPassedBuild(frozenset(['win']))
    expected_expectation_map = {
        'foo_expectations': {
            e: {},
        },
        'bar_expectations': {
            e: {
                'builder': {
                    'pixel_tests': stats,
                }
            }
        }
    }
    self.assertEqual(expectation_map, expected_expectation_map)

  def testMultipleResults(self) -> None:
    """Tests that behavior is as expected when multiple results are given."""
    r1 = data_types.Result('some/test/case', ['win'], 'Pass', 'pixel_tests',
                           'build_id')
    r2 = data_types.Result('some/test/case', ['linux'], 'Pass', 'pixel_tests',
                           'build_id')
    r3 = data_types.Result('some/other/test', ['win'], 'Pass', 'pixel_tests',
                           'build_id')
    r4 = data_types.Result('some/other/test', ['linux'], 'Pass', 'pixel_tests',
                           'build_id')
    r5 = data_types.Result('some/other/other/test', ['win'], 'Pass',
                           'pixel_tests', 'build_id')
    r6 = data_types.Result('some/other/other/test', ['linux'], 'Pass',
                           'pixel_tests', 'build_id')
    e1 = data_types.Expectation('some/test/case', [], 'Failure')
    e2 = data_types.Expectation('some/other/test', ['win'], 'Failure')
    e3 = data_types.Expectation('some/other/other/test', ['mac'], 'Failure')
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            e1: data_types.BuilderStepMap(),
            e2: data_types.BuilderStepMap(),
            e3: data_types.BuilderStepMap(),
        })
    })
    grouped_results = {
        'some/test/case': [r1, r2],
        'some/other/test': [r3, r4],
        'some/other/other/test': [r5, r6],
    }
    matched_results = expectation_map._AddGroupedResults(
        grouped_results, 'builder', None)
    self.assertEqual(matched_results, set([r1, r2, r3]))
    stats1 = data_types.BuildStats()
    stats1.AddPassedBuild(frozenset(['win']))
    stats1.AddPassedBuild(frozenset(['linux']))
    stats2 = data_types.BuildStats()
    stats2.AddPassedBuild(frozenset(['win']))
    expected_expectation_map = {
        'expectation_file': {
            e1: {
                'builder': {
                    'pixel_tests': stats1,
                },
            },
            e2: {
                'builder': {
                    'pixel_tests': stats2,
                },
            },
            e3: {},
        }
    }
    self.assertEqual(expectation_map, expected_expectation_map)


class TestExpectationMapSplitByStalenessUnittest(unittest.TestCase):
  def testEmptyInput(self) -> None:
    """Tests that nothing blows up with empty input."""
    stale_dict, semi_stale_dict, active_dict =\
        data_types.TestExpectationMap().SplitByStaleness()
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, {})
    self.assertIsInstance(stale_dict, data_types.TestExpectationMap)
    self.assertIsInstance(semi_stale_dict, data_types.TestExpectationMap)
    self.assertIsInstance(active_dict, data_types.TestExpectationMap)

  def testStaleExpectations(self) -> None:
    """Tests output when only stale expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(4, 0)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(5, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(6, 0),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(7, 0),
                }),
            }),
        }),
    })
    expected_stale_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectation_map.SplitByStaleness()
    self.assertEqual(stale_dict, expected_stale_dict)
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, {})

  def testActiveExpectations(self) -> None:
    """Tests output when only active expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 1),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 3),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 5),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 6),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 7),
                }),
            }),
        }),
    })
    expected_active_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectation_map.SplitByStaleness()
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, {})
    self.assertEqual(active_dict, expected_active_dict)

  def testSemiStaleExpectations(self) -> None:
    """Tests output when only semi-stale expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(5, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(6, 6),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(7, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 8),
                }),
            }),
        }),
    })
    expected_semi_stale_dict = copy.deepcopy(expectation_map)
    stale_dict, semi_stale_dict, active_dict =\
        expectation_map.SplitByStaleness()
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, expected_semi_stale_dict)
    self.assertEqual(active_dict, {})

  def testSemiStaleTreatedAsActive(self) -> None:
    """Tests output when semi-stale expectations are considered active."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(5, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(6, 6),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(7, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 8),
                }),
            }),
        }),
    })

    expected_semi_stale_dict = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['linux'], ['RetryOnFailure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(5, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(6, 6),
                }),
            }),
        }),
        'bar':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('bar', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(7, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(0, 8),
                }),
            }),
        }),
    })

    expected_active_dict = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
        }),
    })

    def SideEffect(pass_map: Dict[int, data_types.BuilderStepMap]) -> bool:
      return pass_map[data_types.FULL_PASS]['foo_builder'][
          'step1'] == uu.CreateStatsWithPassFails(1, 0)

    with mock.patch.object(expectation_map,
                           '_ShouldTreatSemiStaleAsActive',
                           side_effect=SideEffect):
      stale_dict, semi_stale_dict, active_dict =\
          expectation_map.SplitByStaleness()
    self.assertEqual(stale_dict, {})
    self.assertEqual(semi_stale_dict, expected_semi_stale_dict)
    self.assertEqual(active_dict, expected_active_dict)

  def testAllExpectations(self) -> None:
    """Tests output when all three types of expectations are provided."""
    expectation_map = data_types.TestExpectationMap({
        'foo':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo', ['stale'], 'Failure'):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 0),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(4, 0)
                }),
            }),
            data_types.Expectation('foo', ['semistale'], 'Failure'):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 0),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
            data_types.Expectation('foo', ['active'], 'Failure'):
            data_types.BuilderStepMap({
                'foo_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(1, 1),
                    'step2':
                    uu.CreateStatsWithPassFails(2, 2),
                }),
                'bar_builder':
                data_types.StepBuildStatsMap({
                    'step1':
                    uu.CreateStatsWithPassFails(3, 3),
                    'step2':
                    uu.CreateStatsWithPassFails(0, 4)
                }),
            }),
        }),
    })
    expected_stale = {
        'foo': {
            data_types.Expectation('foo', ['stale'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 0),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(4, 0)
                },
            },
        },
    }
    expected_semi_stale = {
        'foo': {
            data_types.Expectation('foo', ['semistale'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 0),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 0),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
        },
    }
    expected_active = {
        'foo': {
            data_types.Expectation('foo', ['active'], 'Failure'): {
                'foo_builder': {
                    'step1': uu.CreateStatsWithPassFails(1, 1),
                    'step2': uu.CreateStatsWithPassFails(2, 2),
                },
                'bar_builder': {
                    'step1': uu.CreateStatsWithPassFails(3, 3),
                    'step2': uu.CreateStatsWithPassFails(0, 4)
                },
            },
        },
    }

    stale_dict, semi_stale_dict, active_dict =\
        expectation_map.SplitByStaleness()
    self.assertEqual(stale_dict, expected_stale)
    self.assertEqual(semi_stale_dict, expected_semi_stale)
    self.assertEqual(active_dict, expected_active)


class TestExpectationMapFilterOutUnusedExpectationsUnittest(unittest.TestCase):
  def testNoUnused(self) -> None:
    """Tests that filtering is a no-op if there are no unused expectations."""
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'SomeBuilder':
                data_types.StepBuildStatsMap(),
            }),
        })
    })
    expected_expectation_map = copy.deepcopy(expectation_map)
    unused_expectations = expectation_map.FilterOutUnusedExpectations()
    self.assertEqual(len(unused_expectations), 0)
    self.assertEqual(expectation_map, expected_expectation_map)

  def testUnusedButNotEmpty(self) -> None:
    """Tests filtering if there is an unused expectation but no empty tests."""
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'SomeBuilder':
                data_types.StepBuildStatsMap(),
            }),
            data_types.Expectation('foo/test', ['linux'], ['Failure']):
            data_types.BuilderStepMap(),
        })
    })
    expected_expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap({
                'SomeBuilder':
                data_types.StepBuildStatsMap(),
            }),
        }),
    })
    expected_unused = {
        'expectation_file':
        [data_types.Expectation('foo/test', ['linux'], ['Failure'])]
    }
    unused_expectations = expectation_map.FilterOutUnusedExpectations()
    self.assertEqual(unused_expectations, expected_unused)
    self.assertEqual(expectation_map, expected_expectation_map)

  def testUnusedAndEmpty(self) -> None:
    """Tests filtering if there is an expectation that causes an empty test."""
    expectation_map = data_types.TestExpectationMap({
        'expectation_file':
        data_types.ExpectationBuilderMap({
            data_types.Expectation('foo/test', ['win'], ['Failure']):
            data_types.BuilderStepMap(),
        }),
    })
    expected_unused = {
        'expectation_file':
        [data_types.Expectation('foo/test', ['win'], ['Failure'])]
    }
    unused_expectations = expectation_map.FilterOutUnusedExpectations()
    self.assertEqual(unused_expectations, expected_unused)
    self.assertEqual(expectation_map, {})


class BuilderEntryUnittest(unittest.TestCase):
  def testProject(self) -> None:
    """Tests that the project property functions as expected."""
    be = data_types.BuilderEntry('', constants.BuilderTypes.CI, False)
    self.assertEqual(be.project, 'chromium')
    be = data_types.BuilderEntry('', constants.BuilderTypes.CI, True)
    self.assertEqual(be.project, 'chrome')

  def testEquality(self) -> None:
    """Tests equality between two BuilderEntry instances."""
    be = data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False)
    other = data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False)
    self.assertEqual(be, other)
    other = data_types.BuilderEntry('builder', constants.BuilderTypes.TRY,
                                    False)
    self.assertNotEqual(be, other)
    other = data_types.BuilderEntry('builder', constants.BuilderTypes.CI, True)
    self.assertNotEqual(be, other)
    other = data_types.BuilderEntry('not_builder', constants.BuilderTypes.CI,
                                    False)
    self.assertNotEqual(be, other)
    self.assertNotEqual(be, 'builder')

  def testHashability(self) -> None:
    """Tests the hashability of the BuilderEntry class."""
    be = data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False)
    _ = {be}
    other = data_types.BuilderEntry('builder', constants.BuilderTypes.CI, False)
    self.assertEqual(be.__hash__(), other.__hash__())
    other = data_types.BuilderEntry('builder', constants.BuilderTypes.TRY,
                                    False)
    self.assertNotEqual(be.__hash__(), other.__hash__())
    other = data_types.BuilderEntry('builder', constants.BuilderTypes.CI, True)
    self.assertNotEqual(be.__hash__(), other.__hash__())
    other = data_types.BuilderEntry('not_builder', constants.BuilderTypes.CI,
                                    False)
    self.assertNotEqual(be.__hash__(), other.__hash__())


if __name__ == '__main__':
  unittest.main(verbosity=2)
