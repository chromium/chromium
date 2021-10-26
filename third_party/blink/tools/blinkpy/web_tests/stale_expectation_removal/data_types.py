# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom data types for the web test stale expectation remover."""

import fnmatch

from unexpected_passes_common import data_types

VIRTUAL_PREFIX = 'virtual/'
# According to //third_party/blink/web_tests/SlowTests, a test is considered
# slow if it is slower than ~30% of the default timeout. The default timeouts
# are 6s for release, 12s for release with DCHECKs enabled, and 30s for debug.
RELEASE_THRESHOLD = 6 * 0.3
RELEASE_DCHECK_THRESHOLD = 12 * 0.3
DEBUG_THRESHOLD = 30 * 0.3


class WebTestExpectation(data_types.BaseExpectation):
    """Web test-specific container for a test expectation.

    Identical to the base implementation except it can properly handle the case
    of virtual tests falling back to non-virtual expectations.
    """

    def _CompareWildcard(self, result_test_name):
        success = super(WebTestExpectation,
                        self)._CompareWildcard(result_test_name)
        if not success and result_test_name.startswith(VIRTUAL_PREFIX):
            result_test_name = _StripOffVirtualPrefix(result_test_name)
            success = fnmatch.fnmatch(result_test_name, self.test)
        return success

    def _CompareNonWildcard(self, result_test_name):
        success = super(WebTestExpectation,
                        self)._CompareNonWildcard(result_test_name)
        if not success and result_test_name.startswith(VIRTUAL_PREFIX):
            result_test_name = _StripOffVirtualPrefix(result_test_name)
            success = result_test_name == self.test
        return success


class WebTestResult(data_types.BaseResult):
    """Web test-specific container for a test result.

    Identical to the base implementation except it can store duration to
    determine if the test run was considered slow.
    """
    def __init__(self, *args, **kwargs):
        super(WebTestResult, self).__init__(*args, **kwargs)
        self._duration = 0
        self.is_slow_result = False

    def SetDuration(self, duration):
        self._duration = float(duration)
        if 'debug' in self.tags:
            threshold = DEBUG_THRESHOLD
        else:
            # TODO(crbug.com/1222827): Dynamically determine which builders are
            # DCHECK builders by using Buildbucket to determine which builders
            # actually compile things (i.e. find all parent builders) and get
            # their GN args from //tools/mb/mb_config_expectations/
            threshold = RELEASE_THRESHOLD
        self.is_slow_result = (self._duration > threshold)


class WebTestBuildStats(data_types.BaseBuildStats):
    """Web-test specific container for keeping track of a builder's stats.

    Identical to the base implementation except it can store slow builds.
    """
    def __init__(self):
        super(WebTestBuildStats, self).__init__()
        self.slow_builds = 0

    @property
    def always_slow(self):
        return self.slow_builds == self.total_builds

    @property
    def never_slow(self):
        return self.slow_builds == 0

    def AddSlowBuild(self, build_id):
        # Don't increment total builds since the corresponding build should
        # already be added as a passed/failed build.
        self.slow_builds += 1
        build_link = data_types.BuildLinkFromBuildId(build_id)
        self.failure_links = frozenset([build_link]) | self.failure_links

    def GetStatsAsString(self):
        s = super(WebTestBuildStats, self).GetStatsAsString()
        s += ' (%d/%d slow)' % (self.slow_builds, self.total_builds)
        return s

    def NeverNeededExpectation(self, expectation):
        # If this is solely a slow expectation, ignore pass/fail and only
        # consider whether the test was slow or not.
        if set(['Slow']) == expectation.expected_results:
            return self.never_slow
        rv = super(WebTestBuildStats, self).NeverNeededExpectation(expectation)
        if 'Slow' in expectation.expected_results:
            rv = rv and self.never_slow
        return rv

    def AlwaysNeededExpectation(self, expectation):
        # If this is solely a slow expectation, ignore pass/fail and only
        # consider whether the test was slow or not.
        if set(['Slow']) == expectation.expected_results:
            return self.always_slow
        rv = super(WebTestBuildStats,
                   self).AlwaysNeededExpectation(expectation)
        if 'Slow' in expectation.expected_results:
            rv = rv or self.always_slow
        return rv

    def __eq__(self, other):
        return (super(WebTestBuildStats, self).__eq__(other)
                and self.slow_builds == other.slow_builds)


class WebTestTestExpectationMap(data_types.BaseTestExpectationMap):
    """Web-test specific typed map for string types -> ExpectationBuilderMap.

    Identical to the base implementation except it correctly handles adding
    slow results.
    """
    def _AddSingleResult(self, result, stats):
        super(WebTestTestExpectationMap, self)._AddSingleResult(result, stats)
        if result.is_slow_result:
            stats.AddSlowBuild(result.build_id)


def _StripOffVirtualPrefix(test_name):
    # Strip off the leading `virtual/virtual_identifier/`.
    return test_name.split('/', 2)[-1]
