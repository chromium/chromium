# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Custom data types for the web test stale expectation remover."""

import datetime
import fnmatch
from typing import Any, Dict, List, Union

from unexpected_passes_common import data_types

VIRTUAL_PREFIX = 'virtual/'

SANITIZER_BUILDERS = {
    'chromium/ci:WebKit Linux ASAN',
    'chromium/ci:WebKit Linux MSAN',
}


class WebTestExpectation(data_types.BaseExpectation):
    """Web test-specific container for a test expectation.

    Identical to the base implementation except it can properly handle the case
    of virtual tests falling back to non-virtual expectations.
    """

    def _CompareWildcard(self, result_test_name: str) -> bool:
        success = super(WebTestExpectation,
                        self)._CompareWildcard(result_test_name)
        if not success and result_test_name.startswith(VIRTUAL_PREFIX):
            result_test_name = _StripOffVirtualPrefix(result_test_name)
            success = fnmatch.fnmatch(result_test_name, self.test)
        return success

    def _CompareNonWildcard(self, result_test_name: str) -> bool:
        success = super(WebTestExpectation,
                        self)._CompareNonWildcard(result_test_name)
        if not success and result_test_name.startswith(VIRTUAL_PREFIX):
            result_test_name = _StripOffVirtualPrefix(result_test_name)
            success = result_test_name == self.test
        return success

    def _ProcessTagsForFileUse(self) -> List[str]:
        return [t.capitalize() for t in self.tags]


class WebTestResult(data_types.BaseResult):
    """Web test-specific container for a test result.

    Identical to the base implementation except it can store duration to
    determine if the test run was considered slow.
    """
    def __init__(self, *args, **kwargs):
        super(WebTestResult, self).__init__(*args, **kwargs)
        self._duration = datetime.timedelta(0)
        self.is_slow_result = False

    def SetDuration(self, duration: datetime.timedelta,
                    timeout: datetime.timedelta) -> None:
        self._duration = duration
        # According to //third_party/blink/web_tests/SlowTests, as tests is
        # considered slow if it is slower than ~30% of its timeout since test
        # times can vary by up to 3x.
        threshold = 0.3 * timeout
        self.is_slow_result = (self._duration > threshold)


class WebTestBuildStats(data_types.BaseBuildStats):
    """Web-test specific container for keeping track of a builder's stats.

    Identical to the base implementation except it can store slow builds.
    """
    def __init__(self):
        super(WebTestBuildStats, self).__init__()
        self.slow_builds = 0

    @property
    def always_slow(self) -> bool:
        return self.slow_builds == self.total_builds

    @property
    def never_slow(self) -> bool:
        return self.slow_builds == 0

    def AddSlowBuild(self, build_id: str) -> None:
        # Don't increment total builds since the corresponding build should
        # already be added as a passed/failed build. Similarly, we don't take
        # tags as an argument since those will already be passed to
        # AddPassedBuild/AddFailedBuild.
        self.slow_builds += 1
        self.failure_links.add(data_types.BuildLinkFromBuildId(build_id))

    def GetStatsAsString(self) -> str:
        s = super(WebTestBuildStats, self).GetStatsAsString()
        s += ' (%d/%d slow)' % (self.slow_builds, self.total_builds)
        return s

    def NeverNeededExpectation(self,
                               expectation: data_types.Expectation) -> bool:
        # If this is solely a slow expectation, ignore pass/fail and only
        # consider whether the test was slow or not.
        if set(['Slow']) == expectation.expected_results:
            return self.never_slow
        rv = super(WebTestBuildStats, self).NeverNeededExpectation(expectation)
        if 'Slow' in expectation.expected_results:
            rv = rv and self.never_slow
        return rv

    def AlwaysNeededExpectation(self,
                                expectation: data_types.Expectation) -> bool:
        # If this is solely a slow expectation, ignore pass/fail and only
        # consider whether the test was slow or not.
        if set(['Slow']) == expectation.expected_results:
            return self.always_slow
        rv = super(WebTestBuildStats,
                   self).AlwaysNeededExpectation(expectation)
        if 'Slow' in expectation.expected_results:
            rv = rv or self.always_slow
        return rv

    def __eq__(self, other: Any) -> bool:
        return (super(WebTestBuildStats, self).__eq__(other)
                and self.slow_builds == other.slow_builds)


class WebTestTestExpectationMap(data_types.BaseTestExpectationMap):
    """Web-test specific typed map for string types -> ExpectationBuilderMap.

    Identical to the base implementation except it correctly handles adding
    slow results.
    """
    # pytype: disable=signature-mismatch
    # Pytype complains that WebTestResult is not a type of BaseResult, despite
    # WebTestResult being a child. Suspected bug, but pytype team was laid off.
    def _AddSingleResult(self, result: WebTestResult,
                         stats: data_types.BuildStats) -> None:
        # pytype: enable=signature-mismatch
        super(WebTestTestExpectationMap, self)._AddSingleResult(result, stats)
        if result.is_slow_result:
            stats.AddSlowBuild(result.build_id)

    def _ShouldTreatSemiStaleAsActive(
            self, pass_map: Dict[int, data_types.BuilderStepMap]) -> bool:
        # The ASAN/MSAN builders pass in a runtime flag that causes the test
        # runner to only fail if a crash or a timeout occurs, i.e. a regular
        # failure is treated as a pass. As a result, the vast majority of
        # semi-stale expectations without this workaround have a test failing
        # everywhere except the ASAN/MSAN builders. This causes a lot of clutter
        # and makes it very tedious to audit the semi-stale expectations. So,
        # if we have a semi-stale expectation and the only fully passing
        # builders are the ASAN/MSAN ones, treat it as an active expectation
        # instead. For reference, this behavior is caused by the _run_crash_test
        # call here
        # https://source.chromium.org/chromium/chromium/src/+/6bae9bfe93a299104790b2a35cb031fda36d2980:third_party/blink/tools/blinkpy/web_tests/controllers/single_test_runner.py;l=113
        only_passed_on_sanitizers = (set(pass_map[data_types.FULL_PASS].keys())
                                     <= SANITIZER_BUILDERS)
        ran_elsewhere = bool(pass_map[data_types.NEVER_PASS]
                             or pass_map[data_types.PARTIAL_PASS])
        return only_passed_on_sanitizers and ran_elsewhere


def _StripOffVirtualPrefix(test_name: str) -> str:
    # Strip off the leading `virtual/virtual_identifier/`.
    return test_name.split('/', 2)[-1]
