# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import sys

from telemetry.testing import serially_executed_browser_test_case
from telemetry.util import screenshot
from typ import json_results

from gpu_tests import common_browser_args as cba
from gpu_tests import gpu_helper

_START_BROWSER_RETRIES = 3

ResultType = json_results.ResultType

# Please expand the following lists when we expand to new bot configs.
_SUPPORTED_WIN_VERSIONS = ['win7', 'win10']
_SUPPORTED_WIN_VERSIONS_WITH_DIRECT_COMPOSITION = ['win10']
_SUPPORTED_WIN_GPU_VENDORS = [0x8086, 0x10de, 0x1002]
_SUPPORTED_WIN_INTEL_GPUS = [0x5912, 0x3e92]
_SUPPORTED_WIN_INTEL_GPUS_WITH_YUY2_OVERLAYS = [0x5912, 0x3e92]
_SUPPORTED_WIN_INTEL_GPUS_WITH_NV12_OVERLAYS = [0x5912, 0x3e92]


class GpuIntegrationTest(
    serially_executed_browser_test_case.SeriallyExecutedBrowserTestCase):

  _cached_expectations = None
  _also_run_disabled_tests = False
  _disable_log_uploads = False

  # Several of the tests in this directory need to be able to relaunch
  # the browser on demand with a new set of command line arguments
  # than were originally specified. To enable this, the necessary
  # static state is hoisted here.

  # We store a deep copy of the original browser finder options in
  # order to be able to restart the browser multiple times, with a
  # different set of command line arguments each time.
  _original_finder_options = None

  # We keep track of the set of command line arguments used to launch
  # the browser most recently in order to figure out whether we need
  # to relaunch it, if a new pixel test requires a different set of
  # arguments.
  _last_launched_browser_args = set()

  @classmethod
  def SetUpProcess(cls):
    super(GpuIntegrationTest, cls).SetUpProcess()
    cls._original_finder_options = cls._finder_options.Copy()

  @classmethod
  def AddCommandlineArgs(cls, parser):
    """Adds command line arguments understood by the test harness.

    Subclasses overriding this method must invoke the superclass's
    version!"""
    parser.add_option(
        '--disable-log-uploads',
        dest='disable_log_uploads',
        action='store_true',
        default=False,
        help='Disables uploads of logs to cloud storage')

  @classmethod
  def GenerateBrowserArgs(cls, additional_args):
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
    ]
    return default_args + additional_args

  @classmethod
  def CustomizeBrowserArgs(cls, additional_args=None):
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
  def _GenerateAndSanitizeBrowserArgs(cls, additional_args=None):
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
      if os_name == 'android' or os_name == 'chromeos':
        browser_args.remove(cba.DISABLE_GPU)
    return browser_args

  @classmethod
  def _SetBrowserArgsForNextStartup(cls, browser_args):
    """Sets the browser arguments to use for the next browser startup.

    Args:
      browser_args: A list of strings containing the browser arguments to use
          for the next browser startup.
    """
    cls._finder_options = cls.GetOriginalFinderOptions().Copy()
    browser_options = cls._finder_options.browser_options

    # If requested, disable uploading of failure logs to cloud storage.
    if cls._disable_log_uploads:
      browser_options.logs_cloud_bucket = None

    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    cls._last_launched_browser_args = set(browser_args)
    cls.SetBrowserOptions(cls._finder_options)

  @classmethod
  def RestartBrowserIfNecessaryWithArgs(cls,
                                        additional_args=None,
                                        force_restart=False):
    """Restarts the browser if it is determined to be necessary.

    A restart is necessary if restarting would cause the browser to run with
    different arguments or if it is explicitly forced.

    Args:
      additional_args: A list of strings containing any additional, non-default
          args to use for the next browser startup. See the child class'
          GenerateBrowserArgs implementation for default arguments.
      force_restart: True to force the browser to restart even if restarting
          the browser would not change any browser arguments.
    """
    new_browser_args = cls._GenerateAndSanitizeBrowserArgs(additional_args)
    if force_restart or set(
        new_browser_args) != cls._last_launched_browser_args:
      logging.info('Restarting browser with arguments: ' +
                   str(new_browser_args))
      cls.StopBrowser()
      cls._SetBrowserArgsForNextStartup(new_browser_args)
      cls.StartBrowser()

  @classmethod
  def RestartBrowserWithArgs(cls, additional_args=None):
    cls.RestartBrowserIfNecessaryWithArgs(additional_args, force_restart=True)

  # The following is the rest of the framework for the GPU integration tests.

  @classmethod
  def GenerateTestCases__RunGpuTest(cls, options):
    cls._disable_log_uploads = options.disable_log_uploads
    for test_name, url, args in cls.GenerateGpuTests(options):
      yield test_name, (url, test_name, args)

  @classmethod
  def StartBrowser(cls):
    # We still need to retry the browser's launch even though
    # desktop_browser_finder does so too, because it wasn't possible
    # to push the fetch of the first tab into the lower retry loop
    # without breaking Telemetry's unit tests, and that hook is used
    # to implement the gpu_integration_test_unittests.
    for x in range(1, _START_BROWSER_RETRIES + 1):  # Index from 1 instead of 0.
      try:
        super(GpuIntegrationTest, cls).StartBrowser()
        cls.tab = cls.browser.tabs[0]
        return
      except Exception:  # pylint: disable=broad-except
        logging.exception('Browser start failed (attempt %d of %d). Backtrace:',
                          x, _START_BROWSER_RETRIES)
        # If we are on the last try and there is an exception take a screenshot
        # to try and capture more about the browser failure and raise
        if x == _START_BROWSER_RETRIES:
          url = screenshot.TryCaptureScreenShotAndUploadToCloudStorage(
              cls.platform)
          if url is not None:
            logging.info("GpuIntegrationTest screenshot of browser failure " +
                         "located at " + url)
          else:
            logging.warning("GpuIntegrationTest unable to take screenshot.")
        # Stop the browser to make sure it's in an
        # acceptable state to try restarting it.
        if cls.browser:
          cls.StopBrowser()
    # Re-raise the last exception thrown. Only happens if all the retries
    # fail.
    raise

  @classmethod
  def _RestartBrowser(cls, reason):
    logging.warning('Restarting browser due to ' + reason)
    # The Browser may be None at this point if all attempts to start it failed.
    # This can occur if there is a consistent startup crash. For example caused
    # by a bad combination of command-line arguments. So reset to the original
    # options in attempt to successfully launch a browser.
    if cls.browser is None:
      cls.SetBrowserOptions(cls.GetOriginalFinderOptions())
      cls.StartBrowser()
    else:
      cls.StopBrowser()
      cls.SetBrowserOptions(cls._finder_options)
      cls.StartBrowser()

  def _RunGpuTest(self, url, test_name, *args):
    expected_results, should_retry_on_failure = (
        self.GetExpectationsForTest()[:2])
    try:
      # TODO(nednguyen): For some reason the arguments are getting wrapped
      # in another tuple sometimes (like in the WebGL extension tests).
      # Perhaps only if multiple arguments are yielded in the test
      # generator?
      if len(args) == 1 and isinstance(args[0], tuple):
        args = args[0]
      expected_crashes = self.GetExpectedCrashes(args)
      os_name = self.browser.platform.GetOSName()
      # The GPU tests don't function correctly if the screen is not on, so
      # ensure that this is the case.
      if os_name == 'android':
        self.browser.platform.android_action_runner.TurnScreenOn()
      self.RunActualGpuTest(url, *args)
    except Exception:
      if ResultType.Failure in expected_results or should_retry_on_failure:
        # We don't check the return value here since we'll be raising the
        # caught exception already.
        self._ClearExpectedCrashes(expected_crashes)
        if should_retry_on_failure:
          logging.exception('Exception while running flaky test %s', test_name)
          # For robustness, shut down the browser and restart it
          # between flaky test failures, to make sure any state
          # doesn't propagate to the next iteration.
          self._RestartBrowser('flaky test failure')
        else:
          logging.exception('Expected exception while running %s', test_name)
          # Even though this is a known failure, the browser might still
          # be in a bad state; for example, certain kinds of timeouts
          # will affect the next test. Restart the browser to prevent
          # these kinds of failures propagating to the next test.
          self._RestartBrowser('expected test failure')
      else:
        logging.exception('Unexpected exception while running %s', test_name)
        # Symbolize any crash dump (like from the GPU process) that
        # might have happened but wasn't detected above. Note we don't
        # do this for either 'fail' or 'flaky' expectations because
        # there are still quite a few flaky failures in the WebGL test
        # expectations, and since minidump symbolization is slow
        # (upwards of one minute on a fast laptop), symbolizing all the
        # stacks could slow down the tests' running time unacceptably.
        # We also don't do this if the browser failed to startup.
        if self.browser is not None:
          self.browser.CollectDebugData(logging.ERROR)
        # This failure might have been caused by a browser or renderer
        # crash, so restart the browser to make sure any state doesn't
        # propagate to the next test iteration.
        self._RestartBrowser('unexpected test failure')
      raise
    else:
      # We always want to clear any expected crashes, but we don't bother
      # failing the test if it's expected to fail.
      actual_and_expected_crashes_match = self._ClearExpectedCrashes(
          expected_crashes)
      if ResultType.Failure in expected_results:
        logging.warning('%s was expected to fail, but passed.\n', test_name)
      else:
        if not actual_and_expected_crashes_match:
          raise RuntimeError('Actual and expected crashes did not match')

  @staticmethod
  def _IsIntel(vendor_id):
    return vendor_id == 0x8086

  def _IsIntelGPUActive(self):
    gpu = self.browser.GetSystemInfo().gpu
    # The implementation of GetSystemInfo guarantees that the first entry in the
    # GPU devices list is the active GPU.
    return self._IsIntel(gpu.devices[0].vendor_id)

  def _IsDualGPUMacLaptop(self):
    if sys.platform != 'darwin':
      return False
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")
    gpu = system_info.gpu
    if not gpu:
      self.fail('Target machine must have a GPU')
    if len(gpu.devices) != 2:
      return False
    if (self._IsIntel(gpu.devices[0].vendor_id)
        and not self._IsIntel(gpu.devices[1].vendor_id)):
      return True
    if (not self._IsIntel(gpu.devices[0].vendor_id)
        and self._IsIntel(gpu.devices[1].vendor_id)):
      return True
    return False

  def _ClearExpectedCrashes(self, expected_crashes):
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
    # TODO(crbug.com/1006331): Properly match type once we have a way of
    # checking the crashed process type without symbolizing the minidump.
    total_expected_crashes = sum(expected_crashes.values())
    # The Telemetry-wide cleanup will handle any remaining minidumps, so early
    # return here since we don't expect any, which saves us a bit of work.
    if total_expected_crashes == 0:
      return True
    unsymbolized_minidumps = self.browser.GetAllUnsymbolizedMinidumpPaths()
    total_unsymbolized_minidumps = len(unsymbolized_minidumps)

    if total_expected_crashes == total_unsymbolized_minidumps:
      for path in unsymbolized_minidumps:
        self.browser.IgnoreMinidump(path)
      return True

    logging.error(
        'Found %d unsymbolized minidumps when we expected %d. Expected '
        'crash breakdown: %s', total_unsymbolized_minidumps,
        total_expected_crashes, expected_crashes)
    return False

  def GetExpectedCrashes(self, args):  # pylint: disable=no-self-use
    """Returns which crashes, per process type, to expect for the current test.

    Should be overridden by child classes to actually return valid data if
    available.

    Args:
      args: The list passed to _RunGpuTest()

    Returns:
      A dictionary mapping crash types as strings to the number of expected
      crashes of that type. Examples include 'gpu' for the GPU process,
      'renderer' for the renderer process, and 'browser' for the browser
      process.
    """
    del args
    return {}

  @classmethod
  def GenerateGpuTests(cls, options):
    """Subclasses must implement this to yield (test_name, url, args)
    tuples of tests to run."""
    raise NotImplementedError

  def RunActualGpuTest(self, file_path, *args):
    """Subclasses must override this to run the actual test at the given
    URL. file_path is a path on the local file system that may need to
    be resolved via UrlOfStaticFilePath.
    """
    raise NotImplementedError

  def GetOverlayBotConfig(self):
    """Returns expected bot config for DirectComposition and overlay support.

    This is only meaningful on Windows platform.

    The rules to determine bot config are:
      1) Only win10 or newer supports DirectComposition
      2) Only Intel supports hardware overlays with DirectComposition
      3) Currently the Win/Intel GPU bot supports YUY2 and NV12 overlays
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
    gpu_device_id = gpu.device_id
    os_version = self.browser.platform.GetOSVersionName()
    if os_version is None:
      raise Exception("browser.platform.GetOSVersionName() returns None")
    os_version = os_version.lower()

    config = {
        'direct_composition': False,
        'supports_overlays': False,
        'yuy2_overlay_support': 'NONE',
        'nv12_overlay_support': 'NONE',
    }
    assert os_version in _SUPPORTED_WIN_VERSIONS
    assert gpu_vendor_id in _SUPPORTED_WIN_GPU_VENDORS
    if os_version in _SUPPORTED_WIN_VERSIONS_WITH_DIRECT_COMPOSITION:
      config['direct_composition'] = True
      config['supports_overlays'] = True
      config['yuy2_overlay_support'] = 'SOFTWARE'
      config['nv12_overlay_support'] = 'SOFTWARE'
      if gpu_vendor_id == 0x8086:
        assert gpu_device_id in _SUPPORTED_WIN_INTEL_GPUS
        if gpu_device_id in _SUPPORTED_WIN_INTEL_GPUS_WITH_YUY2_OVERLAYS:
          config['yuy2_overlay_support'] = 'SCALING'
        if gpu_device_id in _SUPPORTED_WIN_INTEL_GPUS_WITH_NV12_OVERLAYS:
          config['nv12_overlay_support'] = 'SCALING'
    return config

  def GetDx12VulkanBotConfig(self):
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
      raise Exception("browser.platform.GetOSVersionName() returns None")
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
  def GetPlatformTags(cls, browser):
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
      # On the dual-GPU MacBook Pros, surface the tags of the secondary GPU if
      # it's the discrete GPU, so that test expectations can be written that
      # target the discrete GPU.
      gpu_tags.append(gpu_helper.GetANGLERenderer(gpu_info))
      gpu_tags.append(gpu_helper.GetSwiftShaderGLRenderer(gpu_info))
      gpu_tags.append(gpu_helper.GetCommandDecoder(gpu_info))
      if gpu_info and gpu_info.devices:
        for ii in xrange(0, len(gpu_info.devices)):
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
      # all spaces and underscores in the tag will be replaced by dashes
      tags.extend([re.sub('[ _]', '-', tag) for tag in gpu_tags])

      # Add tags based on GPU feature status.
      skia_renderer = gpu_helper.GetSkiaRenderer(gpu_info.feature_status)
      tags.append(skia_renderer)
      use_vulkan = gpu_helper.GetVulkan(gpu_info.feature_status)
      tags.append(use_vulkan)

    # If additional options have been set via '--extra-browser-args' check for
    # those which map to expectation tags. The '_browser_backend' attribute may
    # not exist in unit tests.
    if hasattr(browser, 'startup_args'):
      use_gl = gpu_helper.GetGL(browser.startup_args)
      tags.append(use_gl)
      use_skia_dawn = gpu_helper.GetSkiaDawn(browser.startup_args)
      tags.append(use_skia_dawn)
    return tags

  @classmethod
  def _EnsureTabIsAvailable(cls):
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
      logging.exception("Failure during browser startup")
      cls._RestartBrowser('failure in setup')
      raise

  # @property doesn't work on class methods without messing with __metaclass__,
  # so just use an explicit getter for simplicity.
  @classmethod
  def GetOriginalFinderOptions(cls):
    return cls._original_finder_options

  def setUp(self):
    self._EnsureTabIsAvailable()

  @staticmethod
  def GetJSONResultsDelimiter():
    return '/'


def LoadAllTestsInModule(module):
  # Just delegates to serially_executed_browser_test_case to reduce the
  # number of imports in other files.
  return serially_executed_browser_test_case.LoadAllTestsInModule(module)
