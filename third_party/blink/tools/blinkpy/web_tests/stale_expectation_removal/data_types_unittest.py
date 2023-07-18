#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for the web test stale expectation remover data types."""

import datetime
from typing import Dict
import unittest

from blinkpy.web_tests.stale_expectation_removal import data_types
from unexpected_passes_common import data_types as common_data_types

FULL_PASS = common_data_types.FULL_PASS
NEVER_PASS = common_data_types.NEVER_PASS
PARTIAL_PASS = common_data_types.PARTIAL_PASS


class WebTestExpectationUnittest(unittest.TestCase):
    def testCompareWildcard(self) -> None:
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

    def testCompareNonWildcard(self) -> None:
        """Tests that non-wildcard comparisons work as expected."""
        e = data_types.WebTestExpectation('test', ['tag1'], 'Failure')
        self.assertTrue(e._CompareNonWildcard('test'))
        self.assertTrue(e._CompareNonWildcard('virtual/some-identifier/test'))
        self.assertFalse(e._CompareNonWildcard('tes'))
        self.assertFalse(
            e._CompareNonWildcard('/virtual/some-identifier/test'))
        self.assertFalse(e._CompareNonWildcard('virtual/some/malformed/test'))

    def testProcessTagsForFileUse(self) -> None:
        """Tests that tags are properly capitalized for use in files."""
        e = data_types.WebTestExpectation('test', ['tag1'], 'Failure')
        self.assertEqual(e.AsExpectationFileString(),
                         '[ Tag1 ] test [ Failure ]')


class WebTestResultUnittest(unittest.TestCase):
    def testSetDurationNotSlow(self) -> None:
        """Tests that setting a duration for a non-slow result works."""
        result = data_types.WebTestResult('foo', ['debug'], 'Pass', 'step',
                                          'build_id')
        # The cutoff should be 30% of the timeout.
        result.SetDuration(datetime.timedelta(seconds=30),
                           datetime.timedelta(seconds=100))
        self.assertFalse(result.is_slow_result)

    def testSetDurationSlow(self) -> None:
        """Tests that setting a duration for a slow result works."""
        result = data_types.WebTestResult('foo', ['debug'], 'Pass', 'step',
                                          'build_id')
        # The cutoff should be 30% of the timeout.
        result.SetDuration(datetime.timedelta(seconds=30.01),
                           datetime.timedelta(seconds=100))
        self.assertTrue(result.is_slow_result)


class WebTestBuildStatsUnittest(unittest.TestCase):
    def CreateGenericBuildStats(self) -> data_types.WebTestBuildStats:
        stats = data_types.WebTestBuildStats()
        stats.AddPassedBuild(frozenset())
        stats.AddFailedBuild('build_id', frozenset())
        return stats

    def testEquality(self) -> None:
        s = self.CreateGenericBuildStats()
        other = self.CreateGenericBuildStats()
        self.assertEqual(s, other)
        # Ensure the base class' equality comparison is preserved.
        other.passed_builds = 0
        self.assertNotEqual(s, other)
        # Test slow build equality.
        other = self.CreateGenericBuildStats()
        s.AddSlowBuild('slow_id')
        self.assertNotEqual(s, other)
        other.AddSlowBuild('slow_id')
        self.assertEqual(s, other)
        other = self.CreateGenericBuildStats()
        other.AddSlowBuild('other_slow_id')
        self.assertNotEqual(s, other)

    def testProperties(self) -> None:
        s = data_types.WebTestBuildStats()
        s.AddPassedBuild(frozenset())
        self.assertTrue(s.never_slow)
        self.assertFalse(s.always_slow)
        s.AddSlowBuild('slow_id')
        self.assertFalse(s.never_slow)
        self.assertTrue(s.always_slow)

    def testSlowBuildsAddedToFailureLinks(self) -> None:
        s = self.CreateGenericBuildStats()
        self.assertEqual(s.failure_links,
                         set(['http://ci.chromium.org/b/build_id']))

    def testGetStatsAsString(self) -> None:
        s = self.CreateGenericBuildStats()
        s.AddSlowBuild('slow_id')
        expected_str = '(1/2 passed) (1/2 slow)'
        self.assertEqual(s.GetStatsAsString(), expected_str)

    def testNeverNeededExpectationSlowExpectation(self) -> None:
        """Tests that special logic is used for Slow-only expectations."""
        expectation = data_types.WebTestExpectation('foo', ['debug'], 'Slow')
        stats = data_types.WebTestBuildStats()
        # The fact that this failed should be ignored.
        stats.AddFailedBuild('build_id', frozenset())
        self.assertTrue(stats.NeverNeededExpectation(expectation))
        stats.AddSlowBuild('build_id')
        self.assertFalse(stats.NeverNeededExpectation(expectation))

    def testNeverNeededExpectationMixedSlowExpectation(self) -> None:
        """Tests that special logic is used for mixed Slow expectations."""
        expectation = data_types.WebTestExpectation('foo', ['debug'],
                                                    ['Slow', 'Failure'])
        stats = data_types.WebTestBuildStats()
        # This should only return true if there are no slow builds AND there
        # are no failed builds.
        stats.AddPassedBuild(frozenset())
        # Passed build, not slow.
        self.assertTrue(stats.NeverNeededExpectation(expectation))
        stats.AddSlowBuild('build_id')
        # Passed build, slow.
        self.assertFalse(stats.NeverNeededExpectation(expectation))
        stats = data_types.WebTestBuildStats()
        stats.AddFailedBuild('build_id', frozenset())
        # Failed build, not slow.
        self.assertFalse(stats.NeverNeededExpectation(expectation))
        stats.AddSlowBuild('build_id')
        # Failed build, slow.
        self.assertFalse(stats.NeverNeededExpectation(expectation))

    def testNeverNeededExpectationNoSlowExpectation(self) -> None:
        """Tests that no special logic is used for non-Slow expectations."""
        expectation = data_types.WebTestExpectation('foo', ['debug'],
                                                    'Failure')
        stats = data_types.WebTestBuildStats()
        stats.AddPassedBuild(frozenset())
        self.assertTrue(stats.NeverNeededExpectation(expectation))
        # Slowness should not be considered in this case.
        stats.AddSlowBuild('build_id')
        self.assertTrue(stats.NeverNeededExpectation(expectation))
        stats.AddFailedBuild('build_id', frozenset())
        self.assertFalse(stats.NeverNeededExpectation(expectation))

    def testAlwaysNeededExpectationSlowExpectation(self) -> None:
        """Tests that special logic is used for Slow-only expectations."""
        expectation = data_types.WebTestExpectation('foo', ['debug'], 'Slow')
        stats = data_types.WebTestBuildStats()
        # The fact that this failed should be ignored.
        stats.AddFailedBuild('build_id', frozenset())
        self.assertFalse(stats.AlwaysNeededExpectation(expectation))
        stats.AddSlowBuild('build_id')
        self.assertTrue(stats.AlwaysNeededExpectation(expectation))

    def testAlwaysNeededExpectationMixedSlowExpectations(self) -> None:
        """Tests that special logic is used for mixed Slow expectations."""
        expectation = data_types.WebTestExpectation('foo', ['debug'],
                                                    ['Slow', 'Failure'])
        stats = data_types.WebTestBuildStats()
        # This should return true if either all builds failed OR all builds were
        # slow.
        stats.AddPassedBuild(frozenset())
        # Passed build, not slow.
        self.assertFalse(stats.AlwaysNeededExpectation(expectation))
        stats.AddSlowBuild('build_id')
        # Passed build, slow.
        self.assertTrue(stats.AlwaysNeededExpectation(expectation))
        stats = data_types.WebTestBuildStats()
        stats.AddFailedBuild('build_id', frozenset())
        # Failed build, not slow.
        self.assertTrue(stats.AlwaysNeededExpectation(expectation))
        stats.AddSlowBuild('build_id')
        # Failed build, slow.
        self.assertTrue(stats.AlwaysNeededExpectation(expectation))

    def testAlwaysNeededExpectationNoSlowExpectation(self) -> None:
        """Tests that no special logic is used for non-Slow expectations."""
        expectation = data_types.WebTestExpectation('foo', ['debug'],
                                                    'Failure')
        stats = data_types.WebTestBuildStats()
        stats.AddFailedBuild('build_id', frozenset())
        self.assertTrue(stats.AlwaysNeededExpectation(expectation))
        stats.AddPassedBuild(frozenset())
        self.assertFalse(stats.AlwaysNeededExpectation(expectation))
        # Slowness should not be considered in this case even if all builds are
        # slow.
        stats.AddSlowBuild('build_id')
        stats.AddSlowBuild('build_id2')
        self.assertFalse(stats.AlwaysNeededExpectation(expectation))


def _CreateEmptyPassMap() -> Dict[int, common_data_types.BuilderStepMap]:
    return {
        FULL_PASS: common_data_types.BuilderStepMap(),
        NEVER_PASS: common_data_types.BuilderStepMap(),
        PARTIAL_PASS: common_data_types.BuilderStepMap(),
    }


class WebTestTestExpectationMapUnittest(unittest.TestCase):

    def testAddSingleResult(self) -> None:
        expectation_map = data_types.WebTestTestExpectationMap()
        result = data_types.WebTestResult('foo', ['debug'], 'Pass', 'step',
                                          'build_id')
        # Test adding a non-slow result.
        result.SetDuration(datetime.timedelta(seconds=1),
                           datetime.timedelta(seconds=10))
        stats = data_types.WebTestBuildStats()
        expectation_map._AddSingleResult(result, stats)
        expected_stats = data_types.WebTestBuildStats()
        expected_stats.AddPassedBuild(frozenset(['debug']))
        self.assertEqual(stats, expected_stats)

        # Test adding a slow result.
        result.SetDuration(datetime.timedelta(seconds=1),
                           datetime.timedelta(seconds=2))
        stats = data_types.WebTestBuildStats()
        expectation_map._AddSingleResult(result, stats)
        expected_stats = data_types.WebTestBuildStats()
        expected_stats.AddPassedBuild(frozenset(['debug']))
        expected_stats.AddSlowBuild('build_id')
        self.assertEqual(stats, expected_stats)

    def testShouldTreatSemiStaleAsActiveOnlySanitizersPass(self) -> None:
        """Tests behavior when only sanitizer bots fully pass."""
        expectation_map = data_types.WebTestTestExpectationMap()

        pass_map = _CreateEmptyPassMap()
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux ASAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux MSAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[NEVER_PASS]['Some Bot'] = (
            common_data_types.StepBuildStatsMap())
        self.assertTrue(
            expectation_map._ShouldTreatSemiStaleAsActive(pass_map))

        pass_map = _CreateEmptyPassMap()
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux ASAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux MSAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[PARTIAL_PASS]['Some Bot'] = (
            common_data_types.StepBuildStatsMap())
        self.assertTrue(
            expectation_map._ShouldTreatSemiStaleAsActive(pass_map))

    def testShouldTreatSemiStaleAsActiveOnlySanitizersPassNoOthers(self
                                                                   ) -> None:
        """Tests behavior when sanitizers fully pass without other results."""
        expectation_map = data_types.WebTestTestExpectationMap()

        pass_map = _CreateEmptyPassMap()
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux ASAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux MSAN'] = (
            common_data_types.StepBuildStatsMap())
        self.assertFalse(
            expectation_map._ShouldTreatSemiStaleAsActive(pass_map))

    def testShouldTreatSemiStaleAsActiveOnlyOneSanitizerPasses(self) -> None:
        """Tests behavior when one sanitizer passes but not the other."""
        expectation_map = data_types.WebTestTestExpectationMap()

        pass_map = _CreateEmptyPassMap()
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux ASAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[NEVER_PASS]['chromium/ci:WebKit Linux MSAN'] = (
            common_data_types.StepBuildStatsMap())
        self.assertTrue(
            expectation_map._ShouldTreatSemiStaleAsActive(pass_map))

        pass_map = _CreateEmptyPassMap()
        pass_map[NEVER_PASS]['chromium/ci:WebKit Linux ASAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux MSAN'] = (
            common_data_types.StepBuildStatsMap())
        self.assertTrue(
            expectation_map._ShouldTreatSemiStaleAsActive(pass_map))

    def testShouldTreatSemiStaleAsActiveOthersPass(self) -> None:
        """Tests behavior when other bots pass in addition to the sanitizers."""
        expectation_map = data_types.WebTestTestExpectationMap()

        pass_map = _CreateEmptyPassMap()
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux ASAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[FULL_PASS]['chromium/ci:WebKit Linux MSAN'] = (
            common_data_types.StepBuildStatsMap())
        pass_map[FULL_PASS]['Foo Bot'] = common_data_types.StepBuildStatsMap()
        pass_map[NEVER_PASS]['Some Bot'] = (
            common_data_types.StepBuildStatsMap())
        self.assertFalse(
            expectation_map._ShouldTreatSemiStaleAsActive(pass_map))


if __name__ == '__main__':
    unittest.main(verbosity=2)
