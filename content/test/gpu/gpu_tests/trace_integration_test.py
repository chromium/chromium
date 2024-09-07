# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(dawn:549) Move WebGPU caching tests to a separate module to trim file.
# pylint: disable=too-many-lines

from enum import Enum
import logging
import os
import posixpath
import sys
import tempfile
from typing import Any, Generator, Iterator, List, Optional, Set, Tuple
import unittest

import dataclasses  # Built-in, but pylint gives an ordering false positive.

from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import overlay_support
from gpu_tests import trace_test_pages
from gpu_tests.util import host_information

import gpu_path_util

from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config

gpu_data_relative_path = gpu_path_util.GPU_DATA_RELATIVE_PATH

data_paths = [
    gpu_path_util.GPU_DATA_DIR,
    os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'media', 'test', 'data')
]

webgl_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;
  domAutomationController._originalLog = window.console.log;
  domAutomationController._messages = '';

  domAutomationController.log = function(msg) {
    domAutomationController._messages += msg + "\n";
    domAutomationController._originalLog.apply(window.console, [msg]);
  }

  domAutomationController.send = function(msg) {
    // Issue a read pixel to synchronize the gpu process to ensure
    // the asynchronous category enabling is finished.
    var temp_canvas = document.createElement("canvas")
    temp_canvas.width = 1;
    temp_canvas.height = 1;
    var temp_gl = temp_canvas.getContext("experimental-webgl") ||
                  temp_canvas.getContext("webgl");
    if (temp_gl) {
      temp_gl.clear(temp_gl.COLOR_BUFFER_BIT);
      var id = new Uint8Array(4);
      temp_gl.readPixels(0, 0, 1, 1, temp_gl.RGBA, temp_gl.UNSIGNED_BYTE, id);
    } else {
      console.log('Failed to get WebGL context.');
    }

    domAutomationController._finished = true;
  }

  window.domAutomationController = domAutomationController;
"""

basic_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._proceed = false;

  domAutomationController._readyForActions = false;
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;
  domAutomationController._originalLog = window.console.log;
  domAutomationController._messages = '';

  domAutomationController.log = function(msg) {
    domAutomationController._messages += msg + "\n";
    domAutomationController._originalLog.apply(window.console, [msg]);
  }

  domAutomationController.send = function(msg) {
    domAutomationController._proceed = true;
    let lmsg = msg.toLowerCase();
    if (lmsg == "ready") {
      domAutomationController._readyForActions = true;
    } else {
      domAutomationController._finished = true;
      if (lmsg == "success") {
        domAutomationController._succeeded = true;
      } else {
        domAutomationController._succeeded = false;
      }
    }
  }

  window.domAutomationController = domAutomationController;
"""

_GET_STATISTICS_EVENT_NAME = 'GetFrameStatisticsMedia'
_SWAP_CHAIN_PRESENT_EVENT_NAME = 'SwapChain::Present'
_BEGIN_OVERLAY_ACCESS_EVENT_NAME = 'SkiaOutputDeviceDComp::BeginOverlayAccess'
_PRESENT_SWAP_CHAIN_EVENT_NAME = 'IDXGISwapChain1::Present1'

_HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME =\
    'HTMLCanvasElement::NotifyListenersCanvasChanged'

_STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME =\
    'StaticBitmapImageToVideoFrameCopier::Convert'

_MFD3D11VC_CAPTURE_EVENT_NAME = 'CopyTextureToGpuMemoryBuffer'
_MFD3D11VC_MAP_EVENT_NAME = 'GpuMemoryBufferTrackerWin::DuplicateAsUnsafeRegion'
_MFD3D11VC_PRESENT_EVENT_NAME = 'DXGISharedHandleState::AcquireKeyedMutex'

# Caching events and constants
_GPU_HOST_STORE_BLOB_EVENT_NAME =\
    'GpuHostImpl::StoreBlobToDisk'
_WEBGPU_BLOB_CACHE_HIT_EVENT_NAME = \
    'DawnCachingInterface::CacheHit'
# Refers to GpuDiskCacheType::kDawnWebGPU see the following header:
#     gpu/ipc/common/gpu_disk_cache_type.h
_WEBGPU_CACHE_HANDLE_TYPE = 1

# Special 'other_args' keys that is used to pass additional arguments
_MIN_CACHE_HIT_KEY = 'min_cache_hits'
_PROFILE_DIR_KEY = 'profile_dir'
_PROFILE_TYPE_KEY = 'profile_type'


class _TraceTestOrigin(Enum):
  """Enum type for different origin types when navigating to URLs.

  The default enum DEFAULT resolves URLs using the explicit localhost IP,
  i.e. 127.0.0.1. LOCALHOST resolves URLs using 'localhost' instead. This is
  useful when we want the navigations to hit the same resource but appear to
  be from a different origin.

  As an implementation detail, the values of the enums correspond to the name of
  the SeriallyExecutedBrowserTestCase instance functions to get the URLs."""
  DEFAULT = 'UrlOfStaticFilePath'
  LOCALHOST = 'LocalhostUrlOfStaticFilePath'


@dataclasses.dataclass
class _TraceTestArguments():
  """Struct-like object for passing trace test arguments instead of dicts."""
  browser_args: List[str]
  category: str
  test_harness_script: str
  finish_js_condition: str
  success_eval_func: str
  other_args: dict
  restart_browser: bool = True
  origin: _TraceTestOrigin = _TraceTestOrigin.DEFAULT


@dataclasses.dataclass
class _CacheTraceTestArguments():
  """Struct-like object for passing persistent cache trace test arguments.

  Cache trace tests consist of a series of normal trace test invocations that
  are necessary in order to verify the expected caching behaviors. The tests
  start with a first load page which is generally used to populate cache
  entries. If |test_renavigation| is true, the same browser that opened the
  first load page is navigated to each cache page in |cache_pages| and the cache
  conditions are verified. Then, regardless of the value of |test_renavigation|,
  we iterate across the |cache_pages| again and restart the browser for each
  page using a new clean user data directory that is seeded with the contents
  from the first load page, and verify the cache conditions. The seeding just
  copies all files in the user data directory from the first load over, see
  *BrowserFinder classes for more details on the seeding:
    //third_party/catapult/telemetry/telemetry/internal/backends/chrome/

  The renavigation tests are suitable when we are verifying for cache hits since
  no new entries should be generated in the |cache_pages|. However, they are not
  suitable for cache miss cases because each |cache_page| may cause entries to
  be written to the cache, thereby causing subsequent |cache_pages| to see cache
  hits when we actually expect them to be misses. Note this is not a problem
  for the restarted browser case because each browser restart seeds a new
  temporary directory with only the contents after the first load page.
  """
  browser_args: List[str]
  category: str
  test_harness_script: str
  finish_js_condition: str
  first_load_eval_func: str
  cache_eval_func: str
  cache_pages: List[str]
  cache_page_origin: _TraceTestOrigin = _TraceTestOrigin.DEFAULT
  test_renavigation: bool = True

  def GenerateFirstLoadTest(self) -> _TraceTestArguments:
    """Returns the trace test arguments for the first load cache test."""
    return _TraceTestArguments(browser_args=self.browser_args,
                               category=self.category,
                               test_harness_script=self.test_harness_script,
                               finish_js_condition=self.finish_js_condition,
                               success_eval_func=self.first_load_eval_func,
                               other_args={},
                               restart_browser=True)

  def GenerateCacheHitTests(
      self, cache_args: Optional[dict]
  ) -> Generator[Tuple[str, _TraceTestArguments], None, None]:
    """Returns a generator for all cache hit trace tests.

    First pass of tests just do a re-navigation, second pass restarts with a
    seeded profile directory.
    """
    if self.test_renavigation:
      for cache_hit_page in self.cache_pages:
        yield (posixpath.join(gpu_data_relative_path, cache_hit_page),
               _TraceTestArguments(browser_args=self.browser_args,
                                   category=self.category,
                                   test_harness_script=self.test_harness_script,
                                   finish_js_condition=self.finish_js_condition,
                                   success_eval_func=self.cache_eval_func,
                                   other_args=cache_args,
                                   restart_browser=False,
                                   origin=self.cache_page_origin))
    for cache_hit_page in self.cache_pages:
      yield (posixpath.join(gpu_data_relative_path, cache_hit_page),
             _TraceTestArguments(browser_args=self.browser_args,
                                 category=self.category,
                                 test_harness_script=self.test_harness_script,
                                 finish_js_condition=self.finish_js_condition,
                                 success_eval_func=self.cache_eval_func,
                                 other_args=cache_args,
                                 restart_browser=True,
                                 origin=self.cache_page_origin))


class TraceIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests GPU traces are plumbed through properly.

  Also tests that GPU Device traces show up on devices that support them."""

  # All known prefixes used in GenerateGpuTests(). Necessary in order for the
  # unittests to work properly since some tests are normally only generated on
  # specific platforms.
  known_test_prefixes = frozenset([
      'OverlayModeTraceTest',
      'SwapChainTraceTest',
      'TraceTest',
      'VideoPathTraceTest',
      'WebGPUCachingTraceTest',
      'WebGLCanvasCaptureTraceTest',
      'WebGPUTraceTest',
  ])

  @classmethod
  def Name(cls) -> str:
    return 'trace_test'

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  def _GetSerialGlobs(self) -> Set[str]:
    serial_globs = set()
    if host_information.IsWindows():
      serial_globs |= {
          # Flaky when run in parallel on Windows. For NVIDIA, it's likely due
          # to the limited number of global overlay contexts supported. For
          # Intel, it could be due to that same issue or something else. AMD
          # may be able to run in parallel once we have an AMD fleet to confirm
          # with.
          'OverlayModeTraceTest_DirectComposition_Underlay*',
          'OverlayModeTraceTest_DirectComposition_Video*',
      }
    return serial_globs

  def _GetSerialTests(self) -> Set[str]:
    serial_tests = set()
    if host_information.IsMac():
      serial_tests |= {
          # Flaky when run in parallel on Mac.
          'WebGPUTraceTest_WebGPUCanvasOneCopyCapture',
          'WebGPUTraceTest_WebGPUCanvasDisableOneCopyCapture_Accelerated',
      }
    return serial_tests

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    # pylint: disable=too-many-branches

    # Include the device level trace tests, even though they're
    # currently skipped on all platforms, to give a hint that they
    # should perhaps be enabled in the future.
    namespace = trace_test_pages.TraceTestPages
    for p in namespace.DefaultPages('TraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category=cls._DisabledByDefaultTraceCategory('gpu.service'),
              test_harness_script=webgl_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckGLCategory',
              other_args=p.other_args)
      ])

    for p in namespace.VideoFromCanvasPages('WebGLCanvasCaptureTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category='blink',
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckWebGLCanvasCapture',
              other_args=p.other_args)
      ])

    for p in namespace.WebGPUCanvasCapturePages('WebGPUTraceTest'):
      yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
          _TraceTestArguments(
              browser_args=p.browser_args,
              category='blink',
              test_harness_script=basic_test_harness_script,
              finish_js_condition='domAutomationController._finished',
              success_eval_func='CheckWebGPUCanvasCapture',
              other_args=p.other_args)
      ])

    if host_information.IsWindows():
      for p in namespace.DirectCompositionPages('VideoPathTraceTest'):
        yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
            _TraceTestArguments(
                browser_args=p.browser_args,
                category=cls._DisabledByDefaultTraceCategory('gpu.service'),
                test_harness_script=basic_test_harness_script,
                finish_js_condition='domAutomationController._finished',
                success_eval_func='CheckVideoPath',
                other_args=p.other_args)
        ])
      # The increased swap count is necessary for tests to consistently pass on
      # NVIDIA since overlays can take ~35 frames to take effect. See
      # crbug.com/1505609.
      for p in namespace.DirectCompositionPages('OverlayModeTraceTest',
                                                swap_count=60):
        yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
            _TraceTestArguments(
                browser_args=p.browser_args,
                category=cls._DisabledByDefaultTraceCategory('gpu.service'),
                test_harness_script=basic_test_harness_script,
                finish_js_condition='domAutomationController._finished',
                success_eval_func='CheckOverlayMode',
                other_args=p.other_args)
        ])
      for p in namespace.LowLatencyPages('SwapChainTraceTest'):
        yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
            _TraceTestArguments(
                browser_args=p.browser_args,
                category='gpu',
                test_harness_script=basic_test_harness_script,
                finish_js_condition='domAutomationController._finished',
                success_eval_func='CheckSwapChainPath',
                other_args=p.other_args)
        ])
      for p in namespace.RootSwapChainTests('SwapChainTraceTest'):
        yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
            _TraceTestArguments(
                browser_args=p.browser_args,
                category='gpu',
                test_harness_script=basic_test_harness_script,
                finish_js_condition='domAutomationController._finished',
                success_eval_func='CheckSwapChainHasAlpha',
                other_args=p.other_args)
        ])
      for p in namespace.MediaFoundationD3D11VideoCaptureTests('TraceTest'):
        yield (p.name, posixpath.join(gpu_data_relative_path, p.url), [
            _TraceTestArguments(
                browser_args=p.browser_args,
                category='gpu,' +
                cls._DisabledByDefaultTraceCategory('video_and_image_capture'),
                test_harness_script=basic_test_harness_script,
                finish_js_condition='domAutomationController._finished',
                success_eval_func='CheckMediaFoundationD3D11VideoCapture',
                other_args=p.other_args)
        ])

    for test in namespace.WebGpuLoadReloadCachingTests(
        'WebGPUCachingTraceTest'):
      yield (test.name,
             posixpath.join(gpu_data_relative_path, test.first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=test.browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUFirstLoadCache',
                     cache_eval_func='CheckWebGPUCacheHits',
                     cache_pages=test.cache_pages,
                 )
             ])
    for test in namespace.WebGpuIncognitoCachingTests('WebGPUCachingTraceTest'):
      yield (test.name,
             posixpath.join(gpu_data_relative_path, test.first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=test.browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUCacheHits',
                     cache_eval_func='CheckNoWebGPUCacheHits',
                     cache_pages=test.cache_pages)
             ])
    for test in namespace.WebGpuDifferentOriginCachingTests(
        'WebGPUCachingTraceTest'):
      yield (test.name,
             posixpath.join(gpu_data_relative_path, test.first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=test.browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUFirstLoadCache',
                     cache_eval_func='CheckNoWebGPUCacheHits',
                     cache_pages=test.cache_pages,
                     cache_page_origin=_TraceTestOrigin.LOCALHOST,
                     test_renavigation=False)
             ])
    for test in namespace.WebGpuCrossOriginCacheMissTests(
        'WebGPUCachingTraceTest'):
      yield (test.name,
             posixpath.join(gpu_data_relative_path, test.first_load_page), [
                 _CacheTraceTestArguments(
                     browser_args=test.browser_args,
                     category='gpu',
                     test_harness_script=basic_test_harness_script,
                     finish_js_condition='domAutomationController._finished',
                     first_load_eval_func='CheckWebGPUFirstLoadCache',
                     cache_eval_func='CheckNoWebGPUCacheHits',
                     cache_pages=test.cache_pages,
                     test_renavigation=False)
             ])

  def _RunActualGpuTraceTest(self,
                             test_path: str,
                             args: _TraceTestArguments,
                             profile_dir: Optional[str] = None,
                             profile_type: Optional[str] = None) -> dict:
    """Returns a dictionary generated via the success evaluation."""
    if args.restart_browser:
      # The version of this test in the old GPU test harness restarted the
      # browser after each test, so continue to do that to match its behavior
      # by default for legacy tests.
      self.RestartBrowserWithArgs(args.browser_args,
                                  profile_dir=profile_dir,
                                  profile_type=profile_type)

    # Set up tracing.
    config = tracing_config.TracingConfig()
    config.chrome_trace_config.category_filter.AddExcludedCategory('*')
    if args.category.find(',') != -1:
      config.chrome_trace_config.category_filter.AddFilterString(args.category)
    else:
      config.chrome_trace_config.category_filter.AddFilter(args.category)
    config.enable_chrome_trace = True
    tab = self.tab
    tab.browser.platform.tracing_controller.StartTracing(config, 60)

    # Perform page navigation.
    url = getattr(self, args.origin.value)(test_path)
    tab.Navigate(url, script_to_evaluate_on_commit=args.test_harness_script)

    try:
      tab.action_runner.WaitForJavaScriptCondition(args.finish_js_condition,
                                                   timeout=60)
    finally:
      test_messages = tab.EvaluateJavaScript(
          'domAutomationController._messages')
      if test_messages:
        logging.info('Logging messages from the test:\n%s', test_messages)

    # Stop tracing.
    timeline_data = tab.browser.platform.tracing_controller.StopTracing()

    # Evaluate success.
    if args.success_eval_func:
      timeline_model = model_module.TimelineModel(timeline_data)
      event_iter = timeline_model.IterAllEvents(
          event_type_predicate=timeline_model.IsSliceOrAsyncSlice)
      prefixed_func_name = '_EvaluateSuccess_' + args.success_eval_func
      results = getattr(self, prefixed_func_name)(args.category, event_iter,
                                                  args.other_args)
      return results if results else {}
    return {}

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    params = args[0]
    if isinstance(params, _TraceTestArguments):
      self._RunActualGpuTraceTest(test_path, params)
    elif isinstance(params, _CacheTraceTestArguments):
      # Create a new temporary directory for each cache test that is run.
      cache_profile_dir = tempfile.TemporaryDirectory()

      # Run the first load page and get the number of expected cache hits.
      load_params = params.GenerateFirstLoadTest()
      results =\
        self._RunActualGpuTraceTest(test_path,
                                    load_params,
                                    profile_dir=cache_profile_dir.name,
                                    profile_type='exact')

      # Generate and run the cache hit tests using the seeded cache dir.
      for (hit_path, trace_params) in params.GenerateCacheHitTests(results):
        self._RunActualGpuTraceTest(hit_path,
                                    trace_params,
                                    profile_dir=cache_profile_dir.name,
                                    profile_type='clean')

  @classmethod
  def SetUpProcess(cls) -> None:
    super(TraceIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs(data_paths)

  @classmethod
  def GenerateBrowserArgs(cls, additional_args: List[str]) -> List[str]:
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(TraceIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([
        cba.ENABLE_LOGGING,
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
        # --test-type=gpu is used to suppress the "stability and security will
        # suffer" infobar caused by --enable-gpu-benchmarking which can
        # interfere with these tests.
        cba.TEST_TYPE_GPU,
    ])
    return default_args

  @staticmethod
  def _SwapChainPresentationModeListToStr(
      presentation_mode_list: List[int]) -> str:
    modes = [
        overlay_support.PresentationModeEventToStr(m)
        for m in presentation_mode_list
    ]
    return f'[{",".join(modes)}]'

  @staticmethod
  def _DisabledByDefaultTraceCategory(category: str) -> str:
    return 'disabled-by-default-%s' % category

  #########################################
  # The test success evaluation functions

  def _EvaluateSuccess_CheckGLCategory(self, category: str,
                                       event_iterator: Iterator,
                                       other_args: dict) -> None:
    del other_args  # Unused in this particular success evaluation.
    for event in event_iterator:
      if (event.category == category
          and event.args.get('gl_category', None) == 'gpu_toplevel'):
        break
    else:
      self.fail('Trace markers for GPU category %s were not found' % category)

  def _GetVideoExpectations(self, other_args: dict) -> '_VideoExpectations':
    """Helper for creating expectations for CheckVideoPath and CheckOverlayMode.

    Args:
      other_args: The |other_args| arg passed into the test.

    Returns:
      A _VideoExpectations instance with zero_copy, pixel_format, no_overlay,
      and presentation_mode filled in.
    """
    gpu = self.browser.GetSystemInfo().gpu.devices[0]
    overlay_bot_config = overlay_support.GetOverlayConfigForGpu(gpu)

    expected = _VideoExpectations()
    expected.zero_copy = other_args.get('zero_copy', None)
    expected.pixel_format = other_args.get('pixel_format', None)
    expected.no_overlay = other_args.get('no_overlay', False)
    video_rotation = other_args.get('video_rotation',
                                    overlay_support.VideoRotation.UNROTATED)
    video_is_not_scaled = other_args.get('full_size', False)
    codec = other_args.get('codec', overlay_support.ZeroCopyCodec.UNSPECIFIED)

    if overlay_bot_config.supports_overlays:
      expected.pixel_format = overlay_bot_config.GetExpectedPixelFormat(
          forced_pixel_format=expected.pixel_format)
      expected.presentation_mode = (
          overlay_bot_config.GetExpectedPresentationMode(
              expected_pixel_format=expected.pixel_format,
              video_rotation=video_rotation))

      if expected.zero_copy is None and not expected.no_overlay:
        expected.zero_copy = overlay_bot_config.GetExpectedZeroCopyUsage(
            expected_pixel_format=expected.pixel_format,
            video_rotation=video_rotation,
            fullsize=video_is_not_scaled,
            codec=codec)

    return expected

  def _EvaluateSuccess_CheckVideoPath(self, category: str,
                                      event_iterator: Iterator,
                                      other_args: dict) -> None:
    """Verifies Chrome goes down the code path as expected.

    Depending on whether hardware overlays are supported or not, which formats
    are supported in overlays, whether video is downscaled or not, whether
    video is rotated or not, Chrome's video presentation code path can be
    different.
    """
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    other_args = other_args or {}
    expected = self._GetVideoExpectations(other_args)

    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _SWAP_CHAIN_PRESENT_EVENT_NAME:
        continue
      if expected.no_overlay:
        self.fail('Expected no overlay got %s' % _SWAP_CHAIN_PRESENT_EVENT_NAME)
      detected_pixel_format = event.args.get('PixelFormat', None)
      if detected_pixel_format is None:
        self.fail('PixelFormat is missing from event %s' %
                  _SWAP_CHAIN_PRESENT_EVENT_NAME)
      if expected.pixel_format != detected_pixel_format:
        self.fail('SwapChain pixel format mismatch, expected %s got %s' %
                  (expected.pixel_format, detected_pixel_format))
      detected_zero_copy = event.args.get('ZeroCopy', None)
      if detected_zero_copy is None:
        self.fail('ZeroCopy is missing from event %s' %
                  _SWAP_CHAIN_PRESENT_EVENT_NAME)
      if expected.zero_copy != detected_zero_copy:
        self.fail('ZeroCopy mismatch, expected %s got %s' %
                  (expected.zero_copy, detected_zero_copy))
      break
    else:
      if expected.no_overlay:
        return
      self.fail(
          'Events with name %s were not found' % _SWAP_CHAIN_PRESENT_EVENT_NAME)

  def _EvaluateSuccess_CheckOverlayMode(self, category: str,
                                        event_iterator: Iterator,
                                        other_args: dict) -> None:
    """Verifies video frames are promoted to overlays when supported."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    other_args = other_args or {}
    expected = self._GetVideoExpectations(other_args)

    presentation_mode_history = []
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _GET_STATISTICS_EVENT_NAME:
        continue
      if expected.no_overlay:
        self.fail('Expected no overlay got %s' % _GET_STATISTICS_EVENT_NAME)
      detected_presentation_mode = event.args.get('CompositionMode', None)
      if detected_presentation_mode is None:
        self.fail('PresentationMode is missing from event %s' %
                  _GET_STATISTICS_EVENT_NAME)
      presentation_mode_history.append(detected_presentation_mode)

    if expected.no_overlay:
      return

    valid_entry_found = False
    for index, mode in enumerate(reversed(presentation_mode_history)):
      # Be more tolerant for the beginning frames in non-overlay mode.
      # Only check the last three entries.
      if index >= 3:
        break
      if mode in (overlay_support.PresentationModeEvent.NONE,
                  overlay_support.PresentationModeEvent.GET_STATISTICS_FAILED):
        # Be more tolerant to avoid test flakiness
        continue
      if (overlay_support.PresentationModeEventToStr(mode)
          != expected.presentation_mode):
        self.fail('SwapChain presentation mode mismatch, expected %s got %s' %
                  (expected.presentation_mode,
                   TraceIntegrationTest._SwapChainPresentationModeListToStr(
                       presentation_mode_history)))
      valid_entry_found = True
    if not valid_entry_found:
      self.fail(
          'No valid frame statistics being collected: %s' % TraceIntegrationTest
          ._SwapChainPresentationModeListToStr(presentation_mode_history))

  def _EvaluateSuccess_CheckSwapChainPath(self, category: str,
                                          event_iterator: Iterator,
                                          other_args: dict) -> None:
    """Verifies that swap chains are used as expected for low latency canvas."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    gpu = self.browser.GetSystemInfo().gpu.devices[0]
    overlay_bot_config = overlay_support.GetOverlayConfigForGpu(gpu)
    assert overlay_bot_config.direct_composition

    expect_no_overlay = other_args and other_args.get('no_overlay', False)
    expect_overlay = not expect_no_overlay
    found_overlay = False

    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _BEGIN_OVERLAY_ACCESS_EVENT_NAME:
        continue
      debug_label = event.args.get('debug_label', None)
      if debug_label == 'SwapChainBuffer':
        found_overlay = True
        break
    if expect_overlay and not found_overlay:
      self.fail(
          'Overlay expected but not found: matching %s events were not found' %
          _BEGIN_OVERLAY_ACCESS_EVENT_NAME)
    elif expect_no_overlay and found_overlay:
      self.fail(
          'Overlay not expected but found: matching %s events were found' %
          _BEGIN_OVERLAY_ACCESS_EVENT_NAME)

  def _EvaluateSuccess_CheckSwapChainHasAlpha(self, category: str,
                                              event_iterator: Iterator,
                                              other_args: dict) -> None:
    """Verified that all DXGI swap chains are presented with the expected alpha
    mode."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    gpu = self.browser.GetSystemInfo().gpu.devices[0]
    overlay_bot_config = overlay_support.GetOverlayConfigForGpu(gpu)
    assert overlay_bot_config.direct_composition

    expect_has_alpha = other_args and other_args.get('has_alpha', False)

    has_present_swap_chain_event_with_has_alpha = False

    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _PRESENT_SWAP_CHAIN_EVENT_NAME:
        continue

      got_has_alpha = event.args.get('has_alpha', None)
      if got_has_alpha is not None:
        has_present_swap_chain_event_with_has_alpha = True

        if expect_has_alpha != got_has_alpha:
          self.fail(
              f'Expected events with name {_PRESENT_SWAP_CHAIN_EVENT_NAME} with'
              f' has_alpha expected {expect_has_alpha}, got {got_has_alpha}')

    # It's also considered a failure if we did not see the expected event.
    if not has_present_swap_chain_event_with_has_alpha:
      self.fail(
          f'Expected events with name {_PRESENT_SWAP_CHAIN_EVENT_NAME} and '
          'has_alpha value, but were not found')

  def _EvaluateSuccess_CheckWebGLCanvasCapture(self, category: str,
                                               event_iterator: Iterator,
                                               other_args: dict) -> None:
    if other_args is None:
      return
    expected_one_copy = other_args.get('one_copy', None)
    expected_accelerated_two_copy = other_args.get('accelerated_two_copy', None)
    if expected_one_copy and expected_accelerated_two_copy:
      self.fail('one_copy and accelerated_two_copy are mutually exclusive')

    found_one_copy_event = False
    found_accelerated_two_copy_event = False
    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue

      if (expected_one_copy is not None and event.name ==
          _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME):
        detected_one_copy = event.args.get('one_copy_canvas_capture', None)

        if detected_one_copy is not None:
          found_one_copy_event = True
          if expected_one_copy != detected_one_copy:
            self.fail('one_copy_canvas_capture mismatch, expected %s got %s' %
                      (expected_one_copy, detected_one_copy))

      elif (expected_accelerated_two_copy is not None
            and event.name == _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME):
        detected_accelerated_two_copy = event.args.get(
            'accelerated_frame_pool_copy', None)

        if detected_accelerated_two_copy is not None:
          found_accelerated_two_copy_event = True
          if expected_accelerated_two_copy != detected_accelerated_two_copy:
            self.fail(
                'accelerated_frame_pool_copy mismatch, expected %s got %s' %
                (expected_accelerated_two_copy, detected_accelerated_two_copy))

    if expected_one_copy is not None and found_one_copy_event is False:
      self.fail('%s events with one_copy_canvas_capture were not found' %
                _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME)

    if (expected_accelerated_two_copy is not None
        and found_accelerated_two_copy_event is False):
      self.fail('%s events with accelerated_frame_pool_copy were not found' %
                _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME)

  def _EvaluateSuccess_CheckWebGPUCanvasCapture(self, category: str,
                                                event_iterator: Iterator,
                                                other_args: dict) -> None:
    expected_one_copy = other_args.get('one_copy', None)
    expected_accelerated_two_copy = other_args.get('accelerated_two_copy', None)
    if expected_one_copy and expected_accelerated_two_copy:
      self.fail('one_copy and accelerated_two_copy are mutually exclusive')

    found_one_copy_event = False
    found_accelerated_two_copy_event = False
    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue

      if (expected_one_copy is not None and event.name ==
          _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME):
        detected_one_copy = event.args.get('one_copy_canvas_capture', None)

        if detected_one_copy is not None:
          found_one_copy_event = True
          if expected_one_copy != detected_one_copy:
            self.fail('one_copy_canvas_capture mismatch, expected %s got %s' %
                      (expected_one_copy, detected_one_copy))

      elif (expected_accelerated_two_copy is not None
            and event.name == _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME):
        detected_accelerated_two_copy = event.args.get(
            'accelerated_frame_pool_copy', None)

        if detected_accelerated_two_copy is not None:
          found_accelerated_two_copy_event = True
          if expected_accelerated_two_copy != detected_accelerated_two_copy:
            self.fail(
                'accelerated_frame_pool_copy mismatch, expected %s got %s' %
                (expected_accelerated_two_copy, detected_accelerated_two_copy))

    if expected_one_copy is not None and found_one_copy_event is False:
      self.fail('%s events with one_copy_canvas_capture were not found' %
                _HTML_CANVAS_NOTIFY_LISTENERS_CANVAS_CHANGED_EVENT_NAME)

    if (expected_accelerated_two_copy is not None
        and found_accelerated_two_copy_event is False):
      self.fail('%s events with accelerated_frame_pool_copy were not found' %
                _STATIC_BITMAP_TO_VID_FRAME_CONVERT_EVENT_NAME)

  def _EvaluateSuccess_CheckWebGPUFirstLoadCache(self, category: str,
                                                 event_iterator: Iterator,
                                                 _other_args: dict) -> dict:
    stored_blobs = 0
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == _GPU_HOST_STORE_BLOB_EVENT_NAME:
        if event.args.get('handle_type', None) == _WEBGPU_CACHE_HANDLE_TYPE:
          stored_blobs += 1
    if stored_blobs == 0:
      self.fail('Expected at least 1 cache entry to be written.')
    return {_MIN_CACHE_HIT_KEY: stored_blobs}

  def _EvaluateSuccess_CheckWebGPUCacheHits(self, category: str,
                                            event_iterator: Iterator,
                                            other_args: dict) -> None:
    cache_hits = 0
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == _GPU_HOST_STORE_BLOB_EVENT_NAME:
        if event.args.get('handle_type', None) == _WEBGPU_CACHE_HANDLE_TYPE:
          self.fail('Unexpected WebGPU cache entry was stored on reloaded page')
      if event.name == _WEBGPU_BLOB_CACHE_HIT_EVENT_NAME:
        cache_hits += 1
    stored_blobs = other_args.get(_MIN_CACHE_HIT_KEY, 1)
    if cache_hits == 0 or cache_hits < stored_blobs:
      self.fail('WebGPU cache hits (%d) is 0 or less than blobs stored (%d).' %
                (cache_hits, stored_blobs))

  def _EvaluateSuccess_CheckNoWebGPUCacheHits(self, category: str,
                                              event_iterator: Iterator,
                                              _other_args: dict) -> None:
    cache_hits = 0
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name == _WEBGPU_BLOB_CACHE_HIT_EVENT_NAME:
        cache_hits += 1
    if cache_hits != 0:
      self.fail('Expected 0 WebGPU cache hits, but got %d.' % cache_hits)

  def _EvaluateSuccess_CheckMediaFoundationD3D11VideoCapture(
      self, category: str, event_iterator: Iterator, _other_args: dict) -> None:
    del category  # Unused.
    os_version = self.browser.platform.GetOSVersionName()
    assert os_version
    if os_version.lower() not in ['win10', 'win11']:
      self.skipTest(
          'MediaFoundationD3D11VideoCapture only available on win 10+')

    js_succeeded = self.tab.EvaluateJavaScript(
        'domAutomationController._succeeded')
    self.assertTrue(js_succeeded)

    found_events = {
        _MFD3D11VC_CAPTURE_EVENT_NAME: False,
        _MFD3D11VC_MAP_EVENT_NAME: False,
        _MFD3D11VC_PRESENT_EVENT_NAME: False,
    }

    for event in event_iterator:
      if event.name in found_events:
        found_events[event.name] = True

    for event_name, found in found_events.items():
      if not found:
        self.fail(f'No {event_name} events found')

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'trace_test_expectations.txt')
    ]


@dataclasses.dataclass
class _VideoExpectations():
  """Struct-like object for passing around video test expectations."""
  pixel_format: Optional[str] = None
  zero_copy: Optional[bool] = None
  no_overlay: Optional[bool] = None
  presentation_mode: Optional[str] = None


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
