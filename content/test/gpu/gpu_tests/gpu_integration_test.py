# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import sys

from telemetry.core import android_platform
from telemetry.testing import serially_executed_browser_test_case
from telemetry.util import screenshot
from typ import json_results

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
      action='store_true', default=False,
      help='Disables uploads of logs to cloud storage')

  @classmethod
  def CustomizeBrowserArgs(cls, browser_args):
    """Customizes the browser's command line arguments.

    NOTE that redefining this method in subclasses will NOT do what
    you expect! Do not attempt to redefine this method!
    """
    if not browser_args:
      browser_args = []
    cls._finder_options = cls._original_finder_options.Copy()
    browser_options = cls._finder_options.browser_options

    # If requested, disable uploading of failure logs to cloud storage.
    if cls._disable_log_uploads:
      browser_options.logs_cloud_bucket = None

    # Append the new arguments.
    browser_options.AppendExtraBrowserArgs(browser_args)
    cls._last_launched_browser_args = set(browser_args)
    cls.SetBrowserOptions(cls._finder_options)

  @classmethod
  def RestartBrowserIfNecessaryWithArgs(cls, browser_args, force_restart=False):
    if not browser_args:
      browser_args = []
    elif '--disable-gpu' in browser_args:
      # Some platforms require GPU process, so browser fails to launch with
      # --disable-gpu mode, therefore, even test expectations fail to evaluate.
      browser_args = list(browser_args)
      os_name = cls.browser.platform.GetOSName()
      if os_name == 'android' or os_name == 'chromeos':
        browser_args.remove('--disable-gpu')
    if force_restart or set(browser_args) != cls._last_launched_browser_args:
      logging.info('Restarting browser with arguments: ' + str(browser_args))
      cls.StopBrowser()
      cls.CustomizeBrowserArgs(browser_args)
      cls.StartBrowser()

  @classmethod
  def RestartBrowserWithArgs(cls, browser_args):
    cls.RestartBrowserIfNecessaryWithArgs(browser_args, force_restart=True)

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
    for x in range(1, _START_BROWSER_RETRIES+1):  # Index from 1 instead of 0.
      try:
        super(GpuIntegrationTest, cls).StartBrowser()
        cls.tab = cls.browser.tabs[0]
        return
      except Exception:
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
    logging.warning('Restarting browser due to '+ reason)
    # The Browser may be None at this point if all attempts to start it failed.
    # This can occur if there is a consistent startup crash. For example caused
    # by a bad combination of command-line arguments. So reset to the original
    # options in attempt to successfully launch a browser.
    if cls.browser is None:
      cls.SetBrowserOptions(cls._original_finder_options)
      cls.StartBrowser()
    else:
      cls.StopBrowser()
      cls.SetBrowserOptions(cls._finder_options)
      cls.StartBrowser()

  def _RunGpuTest(self, url, test_name, *args):
    expected_results, should_retry_on_failure = self.GetExpectationsForTest()[:2]
    try:
      # TODO(nednguyen): For some reason the arguments are getting wrapped
      # in another tuple sometimes (like in the WebGL extension tests).
      # Perhaps only if multiple arguments are yielded in the test
      # generator?
      if len(args) == 1 and isinstance(args[0], tuple):
        args = args[0]
      self.RunActualGpuTest(url, *args)
    except Exception:
      if ResultType.Failure in expected_results or should_retry_on_failure:
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
          # TODO(https://crbug.com/1008075): Remove this split once stack
          # symbolization is standardized across all platforms.
          if isinstance(self.browser.platform,
              android_platform.AndroidPlatform):
            _, output = self.browser.GetStackTrace()
            logging.error(output)
          else:
            self.browser.LogSymbolizedUnsymbolizedMinidumps(logging.ERROR)
        # This failure might have been caused by a browser or renderer
        # crash, so restart the browser to make sure any state doesn't
        # propagate to the next test iteration.
        self._RestartBrowser('unexpected test failure')
      self.fail()
    else:
      if ResultType.Failure in expected_results:
        logging.warning(
          '%s was expected to fail, but passed.\n', test_name)

  def _IsIntel(self, vendor_id):
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
    if (self._IsIntel(gpu.devices[0].vendor_id) and not
        self._IsIntel(gpu.devices[1].vendor_id)):
      return True
    if (not self._IsIntel(gpu.devices[0].vendor_id) and
        self._IsIntel(gpu.devices[1].vendor_id)):
      return True
    return False

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
      if gpu_vendor_id == 0x8086:
        config['supports_overlays'] = True
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
      2) Vulkan: All bots support Vulkan except for Win FYI AMD bots
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

    # "Win7 FYI Release (AMD)" and "Win7 FYI Debug (AMD)" bots
    if (os_version == 'win7' and gpu_vendor_id == 0x1002
        and gpu_device_id == 0x6613):
      config['supports_vulkan'] = False

    return config

  @classmethod
  def GenerateTags(cls, finder_options, possible_browser):
    # If no expectations file paths are returned from cls.ExpectationsFiles()
    # then an empty list will be returned from this function. If tags are
    # returned and there are no expectations files, then Typ will raise
    # an exception.
    if not cls.ExpectationsFiles():
      return []
    with possible_browser.BrowserSession(
        finder_options.browser_options) as browser:
      return cls.GetPlatformTags(browser)

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
      gpu_info = system_info.gpu
      gpu_vendor = gpu_helper.GetGpuVendorString(gpu_info)
      gpu_device_id = gpu_helper.GetGpuDeviceId(gpu_info)
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
      angle_renderer = gpu_helper.GetANGLERenderer(gpu_info)
      cmd_decoder = gpu_helper.GetCommandDecoder(gpu_info)
      # all spaces and underscores in the tag will be replaced by dashes
      tags.extend([re.sub('[ _]', '-', tag) for tag in [
          gpu_vendor, gpu_device_tag, angle_renderer, cmd_decoder]])
    # If additional options have been set via '--extra-browser-args' check for
    # those which map to expectation tags. The '_browser_backend' attribute may
    # not exist in unit tests.
    if (hasattr(browser, '_browser_backend') and
        browser._browser_backend.browser_options.extra_browser_args):
      skia_renderer = gpu_helper.GetSkiaRenderer(\
          browser._browser_backend.browser_options.extra_browser_args)
      tags.extend([skia_renderer])
      use_gl = gpu_helper.GetGL(\
          browser._browser_backend.browser_options.extra_browser_args)
      tags.extend([use_gl])
      use_vulkan = gpu_helper.GetVulkan(\
          browser._browser_backend.browser_options.extra_browser_args)
      tags.extend([use_vulkan])
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

  def setUp(self):
    self._EnsureTabIsAvailable()

  @staticmethod
  def GetJSONResultsDelimiter():
    return '/'


def LoadAllTestsInModule(module):
  # Just delegates to serially_executed_browser_test_case to reduce the
  # number of imports in other files.
  return serially_executed_browser_test_case.LoadAllTestsInModule(module)
