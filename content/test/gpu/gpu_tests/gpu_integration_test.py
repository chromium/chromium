# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=too-many-lines

import collections
import fnmatch
import functools
import importlib
import inspect
import json
import logging
import os
import pkgutil
import re
import types
from typing import Any, Dict, Generator, List, Optional, Set, Tuple, Type
import unittest

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from telemetry.internal.browser import browser_options as bo
from telemetry.internal.platform import system_info as si_module
from telemetry.internal.results import artifact_compatibility_wrapper as acw
from telemetry.testing import serially_executed_browser_test_case
from telemetry.util import minidump_utils
from telemetry.util import screenshot
from typ import json_results

import gpu_path_util
import validate_tag_consistency

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import constants
from gpu_tests import gpu_helper
from gpu_tests import overlay_support
from gpu_tests.util import host_information

TEST_WAS_SLOW = 'test_was_slow'

_START_BROWSER_RETRIES = 3
_MAX_TEST_TRIES = 3

ResultType = json_results.ResultType


# Please expand the following lists when we expand to new bot configs.
_SUPPORTED_WIN_VERSIONS = ['win7', 'win10', 'win11']
_SUPPORTED_WIN_GPU_VENDORS = [
    constants.GpuVendor.AMD,
    constants.GpuVendor.INTEL,
    constants.GpuVendor.NVIDIA,
    constants.GpuVendor.QUALCOMM,
]

_ARGS_TO_CONSOLIDATE = frozenset([
    '--enable-features',
    '--disable-features',
    '--enable-dawn-features',
    '--disable-dawn-features',
])

TestTuple = Tuple[str, ct.GeneratedTest]
TestTupleGenerator = Generator[TestTuple, None, None]


# Handled in a function to avoid polluting the module's environment with
# temporary variable names.
def _GenerateSpecificToGenericTagMapping() -> Dict[str, str]:
  specific_to_generic = {}
  for _, tag_set in validate_tag_consistency.TAG_SPECIALIZATIONS.items():
    for general_tag, specific_tags in tag_set.items():
      for tag in specific_tags:
        specific_to_generic[tag] = general_tag
  return specific_to_generic


_specific_to_generic_tags = _GenerateSpecificToGenericTagMapping()


@dataclasses.dataclass
class _BrowserLaunchInfo():
  browser_args: Set[str] = ct.EmptySet()
  profile_dir: Optional[str] = None
  profile_type: Optional[str] = None

  def __eq__(self, other: Any):
    return (isinstance(other, _BrowserLaunchInfo)
            and self.browser_args == other.browser_args
            and self.profile_dir == other.profile_dir
            and self.profile_type == other.profile_type)


# pylint: disable=too-many-public-methods
class GpuIntegrationTest(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):

  _disable_log_uploads = False
  _skip_post_test_cleanup_and_debug_info = False
  _skip_post_failure_browser_restart = False
  _enforce_browser_version = False

  # Several of the tests in this directory need to be able to relaunch
  # the browser on demand with a new set of command line arguments
  # than were originally specified. To enable this, the necessary
  # static state is hoisted here.

  # We store a deep copy of the original browser finder options in
  # order to be able to restart the browser multiple times, with a
  # different set of command line arguments each time.
  _original_finder_options: Optional[bo.BrowserFinderOptions] = None

  # We keep track of the set of command line arguments used to launch
  # the browser most recently in order to figure out whether we need
  # to relaunch it if the current test requires different ones.
  _last_launched_browser_info = _BrowserLaunchInfo()

  # Keeps track of flaky tests that we're retrying.
  # TODO(crbug.com/40197330): Remove this in favor of a method that doesn't rely
  # on assumptions about retries, etc. if possible.
  _flaky_test_tries = collections.Counter()

  # Keeps track of the first test that is run on a shard for a flakiness
  # workaround. See crbug.com/1079244.
  _first_run_test: Optional[str] = None

  # Keeps track of whether this is the first browser start on a shard for a
  # flakiness workaround. See crbug.com/323927831.
  _is_first_browser_start = True

  _is_asan = False

  tab: Optional[ct.Tab] = None

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    if self.artifacts is None:
      self.set_artifacts(None)

  def set_artifacts(self,
                    artifacts: Optional[Type[acw.ArtifactCompatibilityWrapper]]
                    ) -> None:
    # Instead of using the default logging artifact implementation, use the
    # full logging one. This ensures we get debugging information if something
    # goes wrong before typ can set the actual artifact implementation, such
    # as during initial browser startup.
    if artifacts is None:
      artifacts = acw.FullLoggingArtifactImpl()
    super().set_artifacts(artifacts)

  def ShouldPerformMinidumpCleanupOnSetUp(self) -> bool:
    return not self._skip_post_test_cleanup_and_debug_info

  def ShouldPerformMinidumpCleanupOnTearDown(self) -> bool:
    return not self._skip_post_test_cleanup_and_debug_info

  def CanRunInParallel(self) -> bool:
    """Returns whether a particular test instance can be run in parallel."""
    if not self._SuiteSupportsParallelTests():
      return False
    name = self.shortName()
    for glob in self._GetSerialGlobs():
      if fnmatch.fnmatch(name, glob):
        return False
    return name not in self._GetSerialTests()

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    """Returns whether the suite in general supports parallel tests."""
    return False

  def _GetSerialGlobs(self) -> Set[str]:  # pylint: disable=no-self-use
    """Returns a set of test name globs that should be run serially."""
    return set()

  def _GetSerialTests(self) -> Set[str]:  # pylint: disable=no-self-use
    """Returns a set of test names that should be run serially."""
    return set()

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    """Sets class member variables from parsed command line options.

    This was historically done once in GenerateGpuTests since it was one of the
    earliest called class methods, but that relied on the process always being
    the same, which is not the case if running tests in parallel. Thus, the same
    logic should be run on process setup to ensure that parallel and serial
    execution works the same.

    This should be called once in SetUpProcess and once in GenerateGpuTests.
    """
    cls._original_finder_options = options.Copy()
    cls._skip_post_test_cleanup_and_debug_info =\
        options.skip_post_test_cleanup_and_debug_info
    cls._skip_post_failure_browser_restart =\
        options.no_browser_restart_on_failure
    cls._disable_log_uploads = options.disable_log_uploads
    cls._enforce_browser_version = options.enforce_browser_version

  @classmethod
  def SetUpProcess(cls) -> None:
    super(GpuIntegrationTest, cls).SetUpProcess()
    cls._SetClassVariablesFromOptions(cls._finder_options)
    # Handled here instead of in _SetClassVariablesFromOptions since we only
    # ever want to do this once per process.
    if cls._finder_options.extra_overlay_config_json:
      overlay_support.ParseOverlayJsonFile(
          cls._finder_options.extra_overlay_config_json)

  @classmethod
  def AddCommandlineArgs(cls, parser: ct.CmdArgParser) -> None:
    """Adds command line arguments understood by the test harness.

    Subclasses overriding this method must invoke the superclass's
    version!"""
    parser.add_argument('--disable-log-uploads',
                        dest='disable_log_uploads',
                        action='store_true',
                        default=False,
                        help='Disables uploads of logs to cloud storage')
    parser.add_argument('--extra-overlay-config-json',
                        help=('A path to a JSON file containing additional '
                              'overlay configs to use. See '
                              'overlay_support.ParseOverlayJsonFile() for more '
                              'information on expected format.'))
    parser.add_argument(
        '--skip-post-test-cleanup-and-debug-info',
        action='store_true',
        help=('Disables the automatic cleanup of minidumps after '
              'each test and prevents collection of debug '
              'information such as screenshots when a test '
              'fails. This can can speed up local testing at the '
              'cost of providing less actionable data when a '
              'test does fail.'))
    parser.add_argument(
        '--no-browser-restart-on-failure',
        action='store_true',
        help=('Disables the automatic browser restarts after '
              'failing tests. This can speed up local testing at '
              'the cost of potentially leaving bad state around '
              'after a test fails.'))
    parser.add_argument('--enforce-browser-version',
                        default=False,
                        action='store_true',
                        help=('Enforces that the started browser version is '
                              'the same as what the current Chromium revision '
                              'would build, i.e. that the browser being used '
                              'is one that was built at the current Chromium '
                              'revision.'))

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Generates the browser args to use for the next browser startup.

    Child classes are expected to override this and add any additional default
    arguments that make sense for that particular class in addition to
    the args returned by the parent's implementation.

    Args:
      additional_args: A list of strings containing any additional, non-default
          args to use for the next browser startup.

    Returns:
      A list of strings containing all the browser arguments to use for the next
      browser startup.
    """
    default_args = [
        '--disable-metal-test-shaders',
        # TODO(crbug.com/339479329): Remove this once we either determine that
        # RenderDocument is not the culprit or it is and the root cause of
        # flakiness is fixed.
        '--disable-features=RenderDocument',
    ]
    if cls._SuiteSupportsParallelTests():
      # When running tests in parallel, windows can be treated as occluded if a
      # newly opened window fully covers a previous one, which can cause issues
      # in a few tests. This is practically only an issue on Windows since
      # Linux/Mac stagger new windows, but pass in on all platforms since it
      # could technically be hit on any platform.
      default_args.append('--disable-backgrounding-occluded-windows')

    return default_args + additional_args

  @classmethod
  def CustomizeBrowserArgs(cls,
                           additional_args: Optional[List[str]] = None) -> None:
    """Customizes the browser's command line arguments for the next startup.

    NOTE that redefining this method in subclasses will NOT do what
    you expect! Do not attempt to redefine this method!

    Args:
      additional_args: A list of strings containing any additional, non-default
          args to use for the next browser startup. See the child class'
          GenerateBrowserArgs implementation for default arguments.
    """
    cls._SetBrowserArgsForNextStartup(
        cls._GenerateAndSanitizeBrowserArgs(additional_args))

  @classmethod
  def _GenerateAndSanitizeBrowserArgs(
      cls, additional_args: Optional[List[str]] = None) -> List[str]:
    """Generates browser arguments and sanitizes invalid arguments.

    Args:
      additional_args: A list of strings containing any additional, non-default
          args to use for the next browser startup. See the child class'
          GenerateBrowserArgs implementation for default arguments.

    Returns:
      A list of strings containing all the browser arguments to use for the
      next browser startup with invalid arguments removed.
    """
    additional_args = additional_args or []
    browser_args = cls.GenerateBrowserArgs(additional_args)
    if cba.DISABLE_GPU in browser_args:
      # Some platforms require GPU process, so browser fails to launch with
      # --disable-gpu mode, therefore, even test expectations fail to evaluate.
      os_name = cls.browser.platform.GetOSName()
      if os_name in ('android', 'chromeos'):
        browser_args.remove(cba.DISABLE_GPU)

    if cls._finder_options.browser_type in [
        'web-engine-shell', 'cast-streaming-shell'
    ]:
      # Reduce number of video buffers when running tests on Fuchsia to
      # workaround crbug.com/1203580
      # TODO(crbug.com/40763608): Remove this once the bug is resolved.
      browser_args.append('--double-buffer-compositing')

      # Increase GPU watchdog timeout to 60 seconds to avoid flake when
      # running in emulator on bots.
      browser_args.append('--gpu-watchdog-timeout-seconds=60')

      # Force device scale factor to avoid dependency on
      browser_args.append('--force-device-scale-factor=1.71875')

    return browser_args

  @classmethod
  def _SetBrowserArgsForNextStartup(cls,
                                    browser_args: List[str],
                                    profile_dir: Optional[str] = None,
                                    profile_type: Optional[str] = None) -> None:
    """Sets the browser arguments to use for the next browser startup.

    Args:
      browser_args: A list of strings containing the browser arguments to use
          for the next browser startup.
      profile_dir: A string representing the profile directory to use. In
          general this should be a temporary directory that is cleaned up at
          some point.
      profile_type: A string representing how the profile directory should be
          used. Valid examples are 'clean' which means the profile_dir will be
          used to seed a new temporary directory which is used, or 'exact' which
          means the exact specified directory will be used instead.
    """
    cls._finder_options = cls.GetOriginalFinderOptions().Copy()
    browser_options = cls._finder_options.browser_options

    # If requested, disable uploading of failure logs to cloud storage.
    if cls._disable_log_uploads:
      browser_options.logs_cloud_bucket = None

    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    # Consolidate the args that need to be passed in once with comma-separated
    # values as opposed to being passed in multiple times.
    for arg in _ARGS_TO_CONSOLIDATE:
      browser_options.ConsolidateValuesForArg(arg)

    # Override profile directory behavior if specified.
    if profile_dir:
      browser_options.profile_dir = profile_dir
    if profile_type:
      browser_options.profile_type = profile_type

    # Save the last set of options for comparison.
    cls._last_launched_browser_info = _BrowserLaunchInfo(
        set(browser_args), profile_dir, profile_type)
    cls.SetBrowserOptions(cls._finder_options)

  def RestartBrowserIfNecessaryWithArgs(
      self,
      additional_args: Optional[List[str]] = None,
      force_restart: bool = False,
      profile_dir: Optional[str] = None,
      profile_type: Optional[str] = None) -> None:
    """Restarts the browser if it is determined to be necessary.

    A restart is necessary if restarting would cause the browser to run with
    different arguments or if it is explicitly forced.

    Args:
      additional_args: A list of strings containing any additional, non-default
          args to use for the next browser startup. See the child class'
          GenerateBrowserArgs implementation for default arguments.
      force_restart: True to force the browser to restart even if restarting
          the browser would not change any browser arguments.
      profile_dir: A string representing the profile directory to use. In
          general this should be a temporary directory that is cleaned up at
          some point.
      profile_type: A string representing how the profile directory should be
          used. Valid examples are 'clean' which means the profile_dir will be
          used to seed a new temporary directory which is used, or 'exact' which
          means the exact specified directory will be used instead.
    """
    # cls is largely used here since this used to be a class method and we want
    # to maintain the previous behavior with regards to storing browser launch
    # information between tests. As such, we also disable protected access
    # checks since those would be allowed if this were actually a class method.
    # pylint: disable=protected-access
    cls = self.__class__
    new_browser_args = cls._GenerateAndSanitizeBrowserArgs(additional_args)

    new_browser_info = _BrowserLaunchInfo(set(new_browser_args), profile_dir,
                                          profile_type)
    args_differ = (new_browser_info.browser_args !=
                   cls._last_launched_browser_info.browser_args)
    if force_restart or new_browser_info != cls._last_launched_browser_info:
      logging.info(
          'Restarting browser with arguments: %s, profile type %s, and profile '
          'directory %s', new_browser_args, profile_type, profile_dir)
      cls.StopBrowser()
      cls._SetBrowserArgsForNextStartup(new_browser_args, profile_dir,
                                        profile_type)
      cls.StartBrowser()

    # If we restarted due to a change in browser args, it's possible that a
    # Skip expectation now applies to the test, so check for that.
    if args_differ:
      expected_results, _ = self.GetExpectationsForTest()
      if ResultType.Skip in expected_results:
        message = (
            'Determined that Skip expectation applies after browser restart')
        logging.warning(message)
        self.skipTest(message)
    # pylint: enable=protected-access

  def RestartBrowserWithArgs(self,
                             additional_args: Optional[List[str]] = None,
                             profile_dir: Optional[str] = None,
                             profile_type: str = 'clean') -> None:
    self.RestartBrowserIfNecessaryWithArgs(additional_args,
                                           force_restart=True,
                                           profile_dir=profile_dir,
                                           profile_type=profile_type)

  # The following is the rest of the framework for the GPU integration tests.

  @classmethod
  def GenerateTestCases__RunGpuTest(cls, options: ct.ParsedCmdArgs
                                    ) -> TestTupleGenerator:
    cls._SetClassVariablesFromOptions(options)
    for test_name, url, args in cls.GenerateGpuTests(options):
      yield test_name, (url, test_name, args)

  @classmethod
  def StartBrowser(cls) -> None:
    cls._ModifyBrowserEnvironment()
    # We still need to retry the browser's launch even though
    # desktop_browser_finder does so too, because it wasn't possible
    # to push the fetch of the first tab into the lower retry loop
    # without breaking Telemetry's unit tests, and that hook is used
    # to implement the gpu_integration_test_unittests.
    last_exception = Exception()
    for x in range(1, _START_BROWSER_RETRIES + 1):  # Index from 1 instead of 0.
      try:
        super(GpuIntegrationTest, cls).StartBrowser()
        cls.tab = cls.browser.tabs[0]
        # The GPU tests don't function correctly if the screen is not on, so
        # ensure that this is the case. We do this on browser start instead of
        # before every test since the overhead can be non-trivial, particularly
        # when running many small tests like for WebGPU.
        cls._EnsureScreenOn()
        cls._CheckBrowserVersion()
        return
      except Exception as e:  # pylint: disable=broad-except
        last_exception = e
        logging.exception('Browser start failed (attempt %d of %d). Backtrace:',
                          x, _START_BROWSER_RETRIES)
        # If we are on the last try and there is an exception take a screenshot
        # to try and capture more about the browser failure and raise
        if x == _START_BROWSER_RETRIES:
          url = screenshot.TryCaptureScreenShotAndUploadToCloudStorage(
              cls.platform)
          if url is not None:
            logging.info(
                'GpuIntegrationTest screenshot of browser failure '
                'located at %s', url)
          else:
            logging.warning('GpuIntegrationTest unable to take screenshot.')
        # Stop the browser to make sure it's in an
        # acceptable state to try restarting it.
        if cls.browser:
          cls.StopBrowser()
    # Re-raise the last exception thrown. Only happens if all the retries
    # fail.
    raise last_exception

  @classmethod
  def StopBrowser(cls):
    super(GpuIntegrationTest, cls).StopBrowser()
    cls._RestoreBrowserEnvironment()

  @classmethod
  def _CheckBrowserVersion(cls) -> None:
    if not cls._enforce_browser_version:
      return
    version_info = cls.browser.GetVersionInfo()
    actual_version = version_info['Browser']
    expected_version = _GetExpectedBrowserVersion()
    if expected_version not in actual_version:
      raise RuntimeError(f'Expected browser version {expected_version} not in '
                         f'actual browser version {actual_version}')

  @classmethod
  def _ModifyBrowserEnvironment(cls):
    """Modify the environment before browser startup, if necessary.

    If overridden by a child class, the parent's implementation should be run
    first.
    """

  @classmethod
  def _RestoreBrowserEnvironment(cls):
    """Restore the environment after browser shutdown, if necessary.

    If overridden by a child class, the parent's implementation should be run
    last.
    """

  @classmethod
  def _RestartBrowser(cls, reason: str) -> None:
    logging.warning('Restarting browser due to %s', reason)
    # The Browser may be None at this point if all attempts to start it failed.
    # This can occur if there is a consistent startup crash. For example caused
    # by a bad combination of command-line arguments. So reset to the original
    # options in attempt to successfully launch a browser.
    if cls.browser is None:
      cls.platform.RestartTsProxyServerOnRemotePlatforms()
      cls.SetBrowserOptions(cls.GetOriginalFinderOptions())
      cls.StartBrowser()
    else:
      cls.StopBrowser()
      cls.platform.RestartTsProxyServerOnRemotePlatforms()
      cls.SetBrowserOptions(cls._finder_options)
      cls.StartBrowser()

  @classmethod
  def _EnsureScreenOn(cls) -> None:
    """Ensures the screen is on for applicable platforms."""
    os_name = cls.browser.platform.GetOSName()
    if os_name == 'android':
      cls.browser.platform.android_action_runner.TurnScreenOn()

  # pylint: disable=no-self-use
  def _ShouldForceRetryOnFailureFirstTest(self) -> bool:
    return False

  # pylint: enable=no-self-use

  def _DetermineFirstTestRetryWorkaround(self, test_name: str) -> bool:
    """Potentially allows retries for the first test run on a shard.

    This is a temporary workaround for flaky GPU process startup in WebGL
    conformance tests in the first test run on a shard. This should not be kept
    long-term. See crbug.com/1079244.

    Args:
      test_name: A string containing the name of the test about to be run.

    Returns:
      A boolean indicating whether a retry on failure should be forced.
    """
    if (GpuIntegrationTest._first_run_test == test_name
        and self._ShouldForceRetryOnFailureFirstTest()):
      logging.warning('Forcing RetryOnFailure in test %s', test_name)
      # Notify typ that it should retry this test if necessary.
      # pylint: disable=attribute-defined-outside-init
      self.retryOnFailure = True
      # pylint: enable=attribute-defined-outside-init
      return True
    return False

  # pylint: disable=no-self-use
  def _DetermineFirstBrowserStartWorkaround(self) -> bool:
    """Potentially allows retries for the first browser start on a shard.

    This is a temporary workaround for crbug.com/323927831 and should be
    removed once the root cause is fixed.
    """
    # The browser is assumed to be dead at this point, so we can't rely on
    # GetPlatformTags() to restrict this to the flaking Mac configs.
    if not GpuIntegrationTest._is_first_browser_start:
      return False
    return host_information.IsMac()

  # pylint: enable=no-self-use

  # pylint: disable=no-self-use
  def _DetermineRetryWorkaround(self, exception: Exception) -> bool:
    """Potentially allows retries depending on the exception type.

    This is a temporary workaround for flaky timeouts in the WebGPU CTS which
    should not be kept long term. See crbug.com/1353938.

    Args:
      exception: The exception the test failed with.

    Returns:
      A boolean indicating whether a retry on failure should be forced.
    """
    del exception
    return False

  # pylint: enable=no-self-use

  def _RunGpuTest(self, url: str, test_name: str, args: ct.TestArgs) -> None:
    def _GetExpectedResultsAndShouldRetry():
      expected_results, should_retry_on_failure = (
          self.GetExpectationsForTest()[:2])
      should_retry_on_failure = (
          should_retry_on_failure
          or self._DetermineFirstTestRetryWorkaround(test_name))
      return expected_results, should_retry_on_failure

    if GpuIntegrationTest._first_run_test is None:
      GpuIntegrationTest._first_run_test = test_name

    expected_crashes = {}
    try:
      expected_crashes = self.GetExpectedCrashes(args)
      self.RunActualGpuTest(url, args)
    except unittest.SkipTest:
      # pylint: disable=attribute-defined-outside-init
      self.programmaticSkipIsExpected = True
      # pylint: enable=attribute-defined-outside-init
      raise
    except Exception as e:
      # We get these values here instead of at the beginning of the function
      # because it's possible that RunActualGpuTest() will restart the browser
      # with new browser args, causing any expectation-related data from before
      # then to become invalid due to different typ tags.
      (expected_results,
       should_retry_on_failure) = _GetExpectedResultsAndShouldRetry()
      if not should_retry_on_failure and self._DetermineRetryWorkaround(e):
        should_retry_on_failure = True
        # Notify typ that it should retry this test.
        # pylint: disable=attribute-defined-outside-init
        self.retryOnFailure = True
        # pylint: enable=attribute-defined-outside-init
      if ResultType.Failure in expected_results or should_retry_on_failure:
        self._HandleExpectedFailureOrFlake(test_name, expected_crashes,
                                           should_retry_on_failure)
      else:
        self._HandleUnexpectedFailure(test_name)
      raise
    else:
      (expected_results,
       should_retry_on_failure) = _GetExpectedResultsAndShouldRetry()
      self._HandlePass(test_name, expected_crashes, expected_results)
    finally:
      self.additionalTags[TEST_WAS_SLOW] = json.dumps(self._TestWasSlow())

  def _HandleExpectedFailureOrFlake(self, test_name: str,
                                    expected_crashes: Dict[str, int],
                                    should_retry_on_failure: bool) -> None:
    """Helper method for handling a failure in an expected flaky/failing test"""
    # We don't check the return value here since we'll be raising the caught
    # exception already.
    self._ClearExpectedCrashes(expected_crashes)
    if should_retry_on_failure:
      logging.exception('Exception while running flaky test %s', test_name)
      # Perform the same data collection as we do for an unexpected failure
      # but only if this was the last try for a flaky test so we don't
      # waste time symbolizing minidumps for expected flaky crashes.
      # TODO(crbug.com/40197330): Replace this with a different method of
      # tracking retries if possible.
      self._flaky_test_tries[test_name] += 1
      if self._flaky_test_tries[test_name] == _MAX_TEST_TRIES:
        if self._ShouldCollectDebugInfo():
          self.browser.CollectDebugData(logging.ERROR)
      # For robustness, shut down the browser and restart it
      # between flaky test failures, to make sure any state
      # doesn't propagate to the next iteration.
      if self._ShouldRestartBrowserAfterFailure():
        self._RestartBrowser('flaky test failure')
    else:
      logging.exception('Expected exception while running %s', test_name)
      # Even though this is a known failure, the browser might still
      # be in a bad state; for example, certain kinds of timeouts
      # will affect the next test. Restart the browser to prevent
      # these kinds of failures propagating to the next test.
      if self._ShouldRestartBrowserAfterFailure():
        self._RestartBrowser('expected test failure')

  def _HandleUnexpectedFailure(self, test_name: str) -> None:
    """Helper method for handling an unexpected failure in a test."""
    logging.exception('Unexpected exception while running %s', test_name)
    # Symbolize any crash dump (like from the GPU process) that
    # might have happened but wasn't detected above. Note we don't
    # do this for either 'fail' or 'flaky' expectations because
    # there are still quite a few flaky failures in the WebGL test
    # expectations, and since minidump symbolization is slow
    # (upwards of one minute on a fast laptop), symbolizing all the
    # stacks could slow down the tests' running time unacceptably.
    if self._ShouldCollectDebugInfo():
      self.browser.CollectDebugData(logging.ERROR)
    # This failure might have been caused by a browser or renderer
    # crash, so restart the browser to make sure any state doesn't
    # propagate to the next test iteration.
    if self._ShouldRestartBrowserAfterFailure():
      self._RestartBrowser('unexpected test failure')

  def _TestWasSlow(self) -> bool:  # pylint: disable=no-self-use
    return False

  def _ShouldRestartBrowserAfterFailure(self) -> bool:
    return not self._skip_post_failure_browser_restart

  def _ShouldCollectDebugInfo(self) -> bool:
    # We need a browser in order to collect debug info.
    return (self.browser is not None
            and not self._skip_post_test_cleanup_and_debug_info)

  def _HandlePass(self, test_name: str, expected_crashes: Dict[str, int],
                  expected_results: Set[str]) -> None:
    """Helper function for handling a passing test."""
    # Fuchsia does not have minidump support, use system info to check
    # for crash count.
    if self.browser.platform.GetOSName() == 'fuchsia':
      total_expected_crashes = sum(expected_crashes.values())
      actual_and_expected_crashes_match = self._CheckCrashCountMatch(
          total_expected_crashes)
    else:
      actual_and_expected_crashes_match = self._ClearExpectedCrashes(
          expected_crashes)
    # We always want to clear any expected crashes, but we don't bother
    # failing the test if it's expected to fail.
    if ResultType.Failure in expected_results:
      logging.warning('%s was expected to fail, but passed.\n', test_name)
    else:
      if not actual_and_expected_crashes_match:
        raise RuntimeError('Actual and expected crashes did not match')

  def _CheckCrashCountMatch(self, total_expected_crashes: int) -> bool:
    # We can't get crashes if we don't have a browser.
    if self.browser is None:
      return True

    number_of_crashes = -1
    system_info = self.browser.GetSystemInfo()
    number_of_crashes = \
        system_info.gpu.aux_attributes[u'process_crash_count']

    retval = True
    if number_of_crashes != total_expected_crashes:
      retval = False
      logging.warning('Expected %d gpu process crashes; got: %d',
                      total_expected_crashes, number_of_crashes)
    if number_of_crashes > 0:
      # Restarting is necessary because the crash count includes all
      # crashes since the browser started.
      self._RestartBrowser('Restarting browser to clear process crash count.')
    return retval

  def _IsIntelGPUActive(self) -> bool:
    gpu = self.browser.GetSystemInfo().gpu
    # The implementation of GetSystemInfo guarantees that the first entry in the
    # GPU devices list is the active GPU.
    return gpu_helper.IsIntel(gpu.devices[0].vendor_id)

  def IsDualGPUMacLaptop(self) -> bool:
    if not host_information.IsMac():
      return False
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")
    gpu = system_info.gpu
    if not gpu:
      self.fail('Target machine must have a GPU')
    if len(gpu.devices) != 2:
      return False
    if (gpu_helper.IsIntel(gpu.devices[0].vendor_id)
        and not gpu_helper.IsIntel(gpu.devices[1].vendor_id)):
      return True
    if (not gpu_helper.IsIntel(gpu.devices[0].vendor_id)
        and gpu_helper.IsIntel(gpu.devices[1].vendor_id)):
      return True
    return False

  def AssertLowPowerGPU(self) -> None:
    if self.IsDualGPUMacLaptop():
      if not self._IsIntelGPUActive():
        self.fail("Low power GPU should have been active but wasn't")

  def AssertHighPerformanceGPU(self) -> None:
    if self.IsDualGPUMacLaptop():
      if self._IsIntelGPUActive():
        self.fail("High performance GPU should have been active but wasn't")

  # pylint: disable=too-many-return-statements
  def _ClearExpectedCrashes(self, expected_crashes: Dict[str, int]) -> bool:
    """Clears any expected crash minidumps so they're not caught later.

    Args:
      expected_crashes: A dictionary mapping crash types as strings to the
          number of expected crashes of that type.

    Returns:
      True if the actual number of crashes matched the expected number,
      otherwise False.
    """
    # We can't get crashes if we don't have a browser.
    if self.browser is None:
      return True

    total_expected_crashes = sum(expected_crashes.values())
    # The Telemetry-wide cleanup will handle any remaining minidumps, so early
    # return here since we don't expect any, which saves us a bit of work.
    if total_expected_crashes == 0:
      return True

    unsymbolized_minidumps = self.browser.GetAllUnsymbolizedMinidumpPaths()

    crash_counts = collections.defaultdict(int)
    for path in unsymbolized_minidumps:
      crash_type = minidump_utils.GetProcessTypeFromMinidump(path)
      if not crash_type:
        logging.error(
            'Unable to verify expected crashes due to inability to extract '
            'process type from minidump %s', path)
        return False
      crash_counts[crash_type] += 1

    if crash_counts == expected_crashes:
      for path in unsymbolized_minidumps:
        self.browser.IgnoreMinidump(path)
      return True

    logging.error(
        'Found mismatch between expected and actual crash counts. Expected: '
        '%s, Actual: %s', expected_crashes, crash_counts)
    return False
  # pylint: enable=too-many-return-statements

  # pylint: disable=no-self-use
  def GetExpectedCrashes(self, args: ct.TestArgs) -> Dict[str, int]:
    """Returns which crashes, per process type, to expect for the current test.

    Should be overridden by child classes to actually return valid data if
    available.

    Args:
      args: The tuple passed to _RunGpuTest()

    Returns:
      A dictionary mapping crash types as strings to the number of expected
      crashes of that type. Examples include 'gpu' for the GPU process,
      'renderer' for the renderer process, and 'browser' for the browser
      process.
    """
    del args
    return {}
  # pylint: enable=no-self-use

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    """Subclasses must implement this to yield (test_name, url, args)
    tuples of tests to run."""
    raise NotImplementedError

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    """Subclasses must override this to run the actual test at the given
    URL. test_path is a path on the local file system that may need to
    be resolved via UrlOfStaticFilePath.
    """
    raise NotImplementedError

  def _GetDx12VulkanBotConfig(self) -> Dict[str, bool]:
    """Returns expected bot config for DX12 and Vulkan support.

    This configuration is collected on Windows platform only.
    The rules to determine bot config are:
      1) DX12: Win7 doesn't support DX12. Only Win10 supports DX12
      2) Vulkan: All bots support Vulkan.
    """
    if self.browser is None:
      raise Exception("Browser doesn't exist")
    system_info = self.browser.GetSystemInfo()
    if system_info is None:
      raise Exception("Browser doesn't support GetSystemInfo")
    gpu = system_info.gpu.devices[0]
    if gpu is None:
      raise Exception("System Info doesn't have a gpu")
    gpu_vendor_id = gpu.vendor_id
    assert gpu_vendor_id in _SUPPORTED_WIN_GPU_VENDORS

    os_version = self.browser.platform.GetOSVersionName()
    if os_version is None:
      raise Exception('browser.platform.GetOSVersionName() returns None')
    os_version = os_version.lower()
    assert os_version in _SUPPORTED_WIN_VERSIONS

    config = {
        'supports_dx12': True,
        'supports_vulkan': True,
    }

    if os_version == 'win7':
      config['supports_dx12'] = False

    return config

  @classmethod
  def GetPlatformTags(cls, browser: ct.Browser) -> List[str]:
    """This function will take a Browser instance as an argument.
    It will call the super classes implementation of GetPlatformTags() to get
    a list of tags. Then it will add the gpu vendor, gpu device id,
    angle renderer, and command line decoder tags to that list before
    returning it.
    """
    tags = super(GpuIntegrationTest, cls).GetPlatformTags(browser)
    system_info = browser.GetSystemInfo()
    if system_info:
      gpu_tags = []
      gpu_info = system_info.gpu
      cls._is_asan = gpu_info.aux_attributes.get('is_asan', False)
      # On the dual-GPU MacBook Pros, surface the tags of the secondary GPU if
      # it's the discrete GPU, so that test expectations can be written that
      # target the discrete GPU.
      gpu_tags.append(gpu_helper.GetANGLERenderer(gpu_info))
      gpu_tags.append(gpu_helper.GetCommandDecoder(gpu_info))
      gpu_tags.append(gpu_helper.GetOOPCanvasStatus(gpu_info))
      gpu_tags.append(gpu_helper.GetAsanStatus(gpu_info))
      gpu_tags.append(gpu_helper.GetClangCoverage(gpu_info))
      gpu_tags.append(gpu_helper.GetTargetCpuStatus(gpu_info))
      gpu_tags.append(gpu_helper.GetSkiaGraphiteStatus(gpu_info))
      if gpu_info and gpu_info.devices:
        for ii in range(0, len(gpu_info.devices)):
          gpu_vendor = gpu_helper.GetGpuVendorString(gpu_info, ii)
          gpu_device_id = gpu_helper.GetGpuDeviceId(gpu_info, ii)
          # The gpu device id tag will contain both the vendor and device id
          # separated by a '-'.
          try:
            # If the device id is an integer then it will be added as
            # a hexadecimal to the tag
            gpu_device_tag = '%s-0x%x' % (gpu_vendor, gpu_device_id)
          except TypeError:
            # if the device id is not an integer it will be added as
            # a string to the tag.
            gpu_device_tag = '%s-%s' % (gpu_vendor, gpu_device_id)
          if ii == 0 or gpu_vendor != 'intel':
            gpu_tags.extend([gpu_vendor, gpu_device_tag])
          # This acts as a way to add expectations for Intel GPUs without
          # resorting to the more generic "intel" tag.
          if ii == 0 and gpu_vendor == 'intel':
            if gpu_helper.IsIntelGen9(gpu_device_id):
              gpu_tags.extend(['intel-gen-9'])
            elif gpu_helper.IsIntelGen12(gpu_device_id):
              gpu_tags.extend(['intel-gen-12'])

      # all spaces and underscores in the tag will be replaced by dashes
      tags.extend([re.sub('[ _]', '-', tag) for tag in gpu_tags])

      # Add tags based on GPU feature status.
      skia_renderer = gpu_helper.GetSkiaRenderer(gpu_info)
      tags.append(skia_renderer)
      tags.extend(cls._GetDriverVersionTags(browser, system_info))
    display_server = gpu_helper.GetDisplayServer(browser.browser_type)
    if display_server:
      tags.append(display_server)
    tags = gpu_helper.ReplaceTags(tags)
    return tags

  @classmethod
  def _GetDriverVersionTags(cls, browser: ct.Browser,
                            system_info: si_module.SystemInfo) -> List[str]:
    gpu_info = system_info.gpu
    tags = []
    if gpu_helper.EXPECTATIONS_DRIVER_TAGS and gpu_info:
      driver_vendor = gpu_helper.GetGpuDriverVendor(gpu_info)
      driver_version = gpu_helper.GetGpuDriverVersion(gpu_info)
      if driver_vendor and driver_version:
        driver_vendor = driver_vendor.lower()
        driver_version = driver_version.lower()

        # Extract the string of vendor from 'angle (vendor)'
        matcher = re.compile(r'^angle \(([a-z]+)\)$')
        match = matcher.match(driver_vendor)
        if match:
          driver_vendor = match.group(1)

        # Extract the substring before first space/dash/underscore
        matcher = re.compile(r'^([a-z\d]+)([\s\-_]+[a-z\d]+)+$')
        match = matcher.match(driver_vendor)
        if match:
          driver_vendor = match.group(1)

        for tag in gpu_helper.EXPECTATIONS_DRIVER_TAGS:
          match = gpu_helper.MatchDriverTag(tag)
          assert match
          if (driver_vendor == match.group(1)
              and gpu_helper.EvaluateVersionComparison(
                  driver_version, match.group(2), match.group(3),
                  browser.platform.GetOSName(), driver_vendor)):
            tags.append(tag)
    return tags

  @classmethod
  def GetTagConflictChecker(cls) -> ct.TagConflictChecker:
    return _TagConflictChecker

  @classmethod
  def _EnsureTabIsAvailable(cls) -> None:
    try:
      # If there is no browser, the previous run may have failed an additional
      # time, while trying to recover from an initial failure.
      # ChromeBrowserBackend._GetDevToolsClient can cause this if there is a
      # crash during browser startup. If this has occurred, reset the options,
      # and attempt to bring up a browser for this test. Otherwise failures
      # begin to cascade between tests. https://crbug.com/993379
      if cls.browser is None:
        cls._RestartBrowser('failure in previous shutdown')
      cls.tab = cls.browser.tabs[0]
    except Exception:
      # restart the browser to make sure a failure in a test doesn't
      # propagate to the next test iteration.
      logging.exception('Failure during browser startup')
      cls._RestartBrowser('failure in setup')
      raise

  # @property doesn't work on class methods without messing with __metaclass__,
  # so just use an explicit getter for simplicity.
  @classmethod
  def GetOriginalFinderOptions(cls) -> ct.ParsedCmdArgs:
    return cls._original_finder_options

  def setUp(self) -> None:
    # TODO(crbug.com/323927831): Remove this try/except logic once the root
    # cause of flakes on Macs is resolved.
    try:
      self._EnsureTabIsAvailable()
    except Exception:  # pylint: disable=broad-except
      if self._DetermineFirstBrowserStartWorkaround():
        self._EnsureTabIsAvailable()
      else:
        raise
    finally:
      GpuIntegrationTest._is_first_browser_start = False

  @staticmethod
  def GetJSONResultsDelimiter() -> str:
    return '/'

  @classmethod
  def IgnoredTags(cls) -> List[str]:
    return [
        # We only ever use android-webview-instrumentation if we want to specify
        # that an expectation applies to Webview.
        'android-webview',
        'android-not-webview',
        # These GPUs are analogous to a particular device, and specifying the
        # device name is clearer.
        'arm-mali-g52',  # android-sm-a135m
        'arm-mali-t860',  # chromeos-board-kevin
        # android-moto-g-power-5g---2023
        'imagination-powervr-b-series-bxm-8-256',
        'qualcomm-adreno-(tm)-418',  # android-nexus-5x
        'qualcomm-adreno-(tm)-540',  # android-pixel-2
        'qualcomm-adreno-(tm)-610',  # android-sm-a235m
        'qualcomm-adreno-(tm)-640',  # android-pixel-4
        'qualcomm-adreno-(tm)-740',  # android-sm-s911u1
        'arm-mali-g78',  # android-pixel-6
        'nvidia-nvidia-tegra',  # android-shield-android-tv
        'vmware,',  # VMs
        'vmware,-0x1050',  # ChromeOS VMs
        'mesa/x.org',  # ChromeOS VMs
        'mesa/x.org-0x1050',  # ChromeOS VMs
        'google-vulkan',  # SwiftShader/google-0xc0de
        'chromium-os',  # ChromeOS
        'cros-chrome',  # ChromeOS
        'web-engine-shell',  # Fuchsia
        'cast-streaming-shell',  # Synonymous with cast_streaming suite
        # GPU tests are always run in remote mode on the bots, and it shouldn't
        # make a difference to these tests anyways.
        'chromeos-local',
        'chromeos-remote',
        # "exact" is a valid browser type in Telemetry, but should never be used
        # on the bots.
        'exact',
        # Unknown what exactly causes these to be generated, but they're
        # harmless.
        'win-laptop',
        'unknown-gpu',
        'unknown-gpu-0x8c',
        'unknown-gpu-',
        # Android versions prior to Android 14 use the letter corresponding to
        # the code name, e.g. O for Oreo. 14 and later uses the numerical
        # version. See crbug.com/333795261 for context on why this is
        # necessary.
        'android-8',  # Android O
        'android-9',  # Android P
        'android-10',  # Android Q
        'android-11',  # Android R
        'android-12',  # Android S
        'android-13',  # Android T
        'android-a',  # Android 14+ releases in 2024
    ]

  @classmethod
  def GetExpectationsFilesRepoPath(cls) -> str:
    """Gets the path to the repo that the expectation files live in.

    In most cases, this will be Chromium src/, but it's possible that an
    expectation file lives in a third party repo.
    """
    return gpu_path_util.CHROMIUM_SRC_DIR


def _TagConflictChecker(tag1: str, tag2: str) -> bool:
  # This conflict check takes into account both driver tag matching and
  # cases of tags being subsets of others, e.g. win10 being a subset of win.
  if gpu_helper.MatchDriverTag(tag1):
    return not gpu_helper.IsDriverTagDuplicated(tag1, tag2)
  return (tag1 != tag2 and tag1 != _specific_to_generic_tags.get(tag2, tag2)
          and tag2 != _specific_to_generic_tags.get(tag1, tag1))


def GenerateTestNameMapping() -> Dict[str, Type[GpuIntegrationTest]]:
  """Generates a mapping from suite name to class of all GPU integration tests.

  Returns:
    A dict mapping a suite's human-readable name to the class that implements
    it.
  """
  mapping = {}
  for p in pkgutil.iter_modules(
      [os.path.join(gpu_path_util.GPU_DIR, 'gpu_tests')]):
    if p.ispkg:
      continue
    module_name = 'gpu_tests.' + p.name
    try:
      module = importlib.import_module(module_name)
    except ImportError:
      logging.warning(
          'Unable to import module %s. This is likely due to stale .pyc files '
          'existing on disk.', module_name)
      continue
    for name, obj in inspect.getmembers(module):
      # Look for cases of GpuIntegrationTest that have Name() overridden. The
      # name check filters out base classes.
      if (inspect.isclass(obj) and issubclass(obj, GpuIntegrationTest)
          and obj.Name() != name):
        mapping[obj.Name()] = obj
  return mapping


@functools.lru_cache(maxsize=1)
def _GetExpectedBrowserVersion() -> str:
  version_file = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'chrome',
                              'VERSION')
  with open(version_file, encoding='utf-8') as infile:
    contents = infile.read()
  version_info = {}
  for line in contents.splitlines():
    if not line:
      continue
    k, v = line.split('=')
    version_info[k] = v
  return (f'{version_info["MAJOR"]}.{version_info["MINOR"]}.'
          f'{version_info["BUILD"]}.{version_info["PATCH"]}')


def LoadAllTestsInModule(module: types.ModuleType) -> unittest.TestSuite:
  # Just delegates to serially_executed_browser_test_case to reduce the
  # number of imports in other files.
  return serially_executed_browser_test_case.LoadAllTestsInModule(module)
