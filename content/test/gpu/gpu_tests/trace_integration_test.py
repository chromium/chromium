# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from gpu_tests import common_browser_args as cba
from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import pixel_test_pages

from telemetry.timeline import model as model_module
from telemetry.timeline import tracing_config

gpu_relative_path = "content/test/data/gpu/"

data_paths = [
    os.path.join(path_util.GetChromiumSrcDir(), gpu_relative_path),
    os.path.join(path_util.GetChromiumSrcDir(), 'media', 'test', 'data')
]

webgl_test_harness_script = r"""
  var domAutomationController = {};

  domAutomationController._finished = false;

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

# Presentation mode enums match DXGI_FRAME_PRESENTATION_MODE
_SWAP_CHAIN_PRESENTATION_MODE_COMPOSED = 0
_SWAP_CHAIN_PRESENTATION_MODE_OVERLAY = 1
_SWAP_CHAIN_PRESENTATION_MODE_NONE = 2
_SWAP_CHAIN_PRESENTATION_MODE_COMPOSITION_FAILURE = 3
# The following is defined for Chromium testing internal use.
_SWAP_CHAIN_GET_FRAME_STATISTICS_MEDIA_FAILED = -1

_GET_STATISTICS_EVENT_NAME = 'GetFrameStatisticsMedia'
_SWAP_CHAIN_PRESENT_EVENT_NAME = 'SwapChain::Present'
_PRESENT_TO_SWAP_CHAIN_EVENT_NAME = 'SwapChainPresenter::PresentToSwapChain'
_PRESENT_MAIN_SWAP_CHAIN_EVENT_NAME =\
    'DirectCompositionChildSurfaceWin::PresentSwapChain'


class _TraceTestArguments(object):
  """Struct-like object for passing trace test arguments instead of dicts."""

  def __init__(  # pylint: disable=too-many-arguments
      self, browser_args, category, test_harness_script, finish_js_condition,
      success_eval_func, other_args):
    self.browser_args = browser_args
    self.category = category
    self.test_harness_script = test_harness_script
    self.finish_js_condition = finish_js_condition
    self.success_eval_func = success_eval_func
    self.other_args = other_args


class TraceIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  """Tests GPU traces are plumbed through properly.

  Also tests that GPU Device traces show up on devices that support them."""

  @classmethod
  def Name(cls):
    return 'trace_test'

  @classmethod
  def GenerateGpuTests(cls, options):
    # Include the device level trace tests, even though they're
    # currently skipped on all platforms, to give a hint that they
    # should perhaps be enabled in the future.
    namespace = pixel_test_pages.PixelTestPages
    for p in namespace.DefaultPages('TraceTest'):
      yield (p.name, gpu_relative_path + p.url,
             _TraceTestArguments(
                 browser_args=p.browser_args,
                 category=cls._DisabledByDefaultTraceCategory('gpu.service'),
                 test_harness_script=webgl_test_harness_script,
                 finish_js_condition='domAutomationController._finished',
                 success_eval_func='CheckGLCategory',
                 other_args=p.other_args))
    for p in namespace.DirectCompositionPages('VideoPathTraceTest'):
      yield (p.name, gpu_relative_path + p.url,
             _TraceTestArguments(
                 browser_args=p.browser_args,
                 category=cls._DisabledByDefaultTraceCategory('gpu.service'),
                 test_harness_script=basic_test_harness_script,
                 finish_js_condition='domAutomationController._finished',
                 success_eval_func='CheckVideoPath',
                 other_args=p.other_args))
    for p in namespace.LowLatencyPages('SwapChainTraceTest'):
      yield (p.name, gpu_relative_path + p.url,
             _TraceTestArguments(
                 browser_args=p.browser_args,
                 category='gpu',
                 test_harness_script=basic_test_harness_script,
                 finish_js_condition='domAutomationController._finished',
                 success_eval_func='CheckSwapChainPath',
                 other_args=p.other_args))
    for p in namespace.DirectCompositionPages('OverlayModeTraceTest'):
      if p.other_args and p.other_args.get('video_is_rotated', False):
        # For all drivers we tested, when a video is rotated, frames won't
        # be promoted to hardware overlays.
        continue
      yield (p.name, gpu_relative_path + p.url,
             _TraceTestArguments(
                 browser_args=p.browser_args,
                 category=cls._DisabledByDefaultTraceCategory('gpu.service'),
                 test_harness_script=basic_test_harness_script,
                 finish_js_condition='domAutomationController._finished',
                 success_eval_func='CheckOverlayMode',
                 other_args=p.other_args))
    for p in namespace.ForceFullDamagePages('SwapChainTraceTest'):
      yield (p.name, gpu_relative_path + p.url,
             _TraceTestArguments(
                 browser_args=p.browser_args,
                 category='gpu',
                 test_harness_script=basic_test_harness_script,
                 finish_js_condition='domAutomationController._finished',
                 success_eval_func='CheckMainSwapChainPath',
                 other_args=p.other_args))

  def RunActualGpuTest(self, test_path, *args):
    test_params = args[0]

    # The version of this test in the old GPU test harness restarted
    # the browser after each test, so continue to do that to match its
    # behavior.
    self.RestartBrowserWithArgs(test_params.browser_args)

    # Set up tracing.
    config = tracing_config.TracingConfig()
    config.chrome_trace_config.category_filter.AddExcludedCategory('*')
    config.chrome_trace_config.category_filter.AddFilter(test_params.category)
    config.enable_chrome_trace = True
    tab = self.tab
    tab.browser.platform.tracing_controller.StartTracing(config, 60)

    # Perform page navigation.
    url = self.UrlOfStaticFilePath(test_path)
    tab.Navigate(url,
                 script_to_evaluate_on_commit=test_params.test_harness_script)
    tab.action_runner.WaitForJavaScriptCondition(
        test_params.finish_js_condition, timeout=30)

    # Stop tracing.
    timeline_data = tab.browser.platform.tracing_controller.StopTracing()

    # Evaluate success.
    if test_params.success_eval_func:
      timeline_model = model_module.TimelineModel(timeline_data)
      event_iter = timeline_model.IterAllEvents(
          event_type_predicate=timeline_model.IsSliceOrAsyncSlice)
      prefixed_func_name = '_EvaluateSuccess_' + test_params.success_eval_func
      getattr(self, prefixed_func_name)(test_params.category, event_iter,
                                        test_params.other_args)

  @classmethod
  def SetUpProcess(cls):
    super(TraceIntegrationTest, cls).SetUpProcess()
    path_util.SetupTelemetryPaths()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs(data_paths)

  @classmethod
  def GenerateBrowserArgs(cls, additional_args):
    """Adds default arguments to |additional_args|.

    See the parent class' method documentation for additional information.
    """
    default_args = super(TraceIntegrationTest,
                         cls).GenerateBrowserArgs(additional_args)
    default_args.extend([
        '--enable-logging',
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
        # All bots are connected with a power source, however, we want to to
        # test with the code path that's enabled with battery power.
        cba.DISABLE_VP_SCALING,
    ])
    return default_args

  def _GetAndAssertOverlayBotConfig(self):
    overlay_bot_config = self.GetOverlayBotConfig()
    if overlay_bot_config is None:
      self.fail('Overlay bot config can not be determined')
    assert overlay_bot_config.get('direct_composition', False)
    return overlay_bot_config

  @staticmethod
  def _SwapChainPresentationModeToStr(presentation_mode):
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_COMPOSED:
      return 'COMPOSED'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_OVERLAY:
      return 'OVERLAY'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_NONE:
      return 'NONE'
    if presentation_mode == _SWAP_CHAIN_PRESENTATION_MODE_COMPOSITION_FAILURE:
      return 'COMPOSITION_FAILURE'
    if presentation_mode == _SWAP_CHAIN_GET_FRAME_STATISTICS_MEDIA_FAILED:
      return 'GET_STATISTICS_FAILED'
    return str(presentation_mode)

  @staticmethod
  def _SwapChainPresentationModeListToStr(presentation_mode_list):
    list_str = None
    for mode in presentation_mode_list:
      mode_str = TraceIntegrationTest._SwapChainPresentationModeToStr(mode)
      if list_str is None:
        list_str = mode_str
      else:
        list_str = '%s,%s' % (list_str, mode_str)
    return '[%s]' % list_str

  @staticmethod
  def _DisabledByDefaultTraceCategory(category):
    return 'disabled-by-default-%s' % category

  #########################################
  # The test success evaluation functions

  def _EvaluateSuccess_CheckGLCategory(self, category, event_iterator,
                                       other_args):
    del other_args  # Unused in this particular success evaluation.
    for event in event_iterator:
      if (event.category == category
          and event.args.get('gl_category', None) == 'gpu_toplevel'):
        break
    else:
      self.fail('Trace markers for GPU category %s were not found' % category)

  def _GetVideoPathExpectations(self, other_args):
    """Helper method to get expectations for CheckVideoPath.

    Args:
      other_args: The |other_args| arg passed into the test.

    Returns:
      A _VideoExpectations instance with zero_copy, pixel_format, and no_overlay
      filled in.
    """
    overlay_bot_config = self._GetAndAssertOverlayBotConfig()
    expect_yuy2 = other_args.get('expect_yuy2', False)
    expected = _VideoExpectations()
    expected.zero_copy = other_args.get('zero_copy', False)
    expected.pixel_format = "NV12"
    expected.no_overlay = other_args.get('no_overlay', False)

    supports_nv12_overlays = False
    if overlay_bot_config.get('supports_overlays', False):
      supports_yuy2_overlays = False
      if overlay_bot_config['yuy2_overlay_support'] != 'NONE':
        supports_yuy2_overlays = True
      if overlay_bot_config['nv12_overlay_support'] != 'NONE':
        supports_nv12_overlays = True
      assert supports_yuy2_overlays or supports_nv12_overlays
      if expect_yuy2 or not supports_nv12_overlays:
        if overlay_bot_config['yuy2_overlay_support'] != 'SOFTWARE':
          expected.pixel_format = "YUY2"
    if not supports_nv12_overlays or overlay_bot_config[
        'nv12_overlay_support'] == 'SOFTWARE':
      expected.zero_copy = False

    return expected

  def _EvaluateSuccess_CheckVideoPath(self, category, event_iterator,
                                      other_args):
    """Verifies Chrome goes down the code path as expected.

    Depending on whether hardware overlays are supported or not, which formats
    are supported in overlays, whether video is downscaled or not, whether
    video is rotated or not, Chrome's video presentation code path can be
    different.
    """
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    other_args = other_args or {}
    expected = self._GetVideoPathExpectations(other_args)

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

  def _GetOverlayModeExpectations(self, other_args):
    """Helper method to get expectations for CheckOverlayMode.

    Args:
      other_args: The |other_args| arg passed into the test.

    Returns:
      A _VideoExpectations instance with presentation_mode and no_overlay filled
      in.
    """
    overlay_bot_config = self._GetAndAssertOverlayBotConfig()
    expected = _VideoExpectations()
    expected.presentation_mode = _SWAP_CHAIN_PRESENTATION_MODE_COMPOSED
    expected.no_overlay = other_args.get('no_overlay', False)

    if overlay_bot_config.get('supports_overlays', False):
      if overlay_bot_config['nv12_overlay_support'] != 'SOFTWARE':
        expected.presentation_mode = _SWAP_CHAIN_PRESENTATION_MODE_OVERLAY
    return expected

  def _EvaluateSuccess_CheckOverlayMode(self, category, event_iterator,
                                        other_args):
    """Verifies video frames are promoted to overlays when supported."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    other_args = other_args or {}
    expected = self._GetOverlayModeExpectations(other_args)

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
    for index in range(len(presentation_mode_history)):
      mode = presentation_mode_history[index]
      if (mode == _SWAP_CHAIN_PRESENTATION_MODE_NONE
          or mode == _SWAP_CHAIN_GET_FRAME_STATISTICS_MEDIA_FAILED):
        # Be more tolerant to avoid test flakiness
        continue
      if mode != expected.presentation_mode:
        if index >= len(presentation_mode_history) // 2:
          # Be more tolerant for the first half frames in non-overlay mode.
          self.fail('SwapChain presentation mode mismatch, expected %s got %s' %
                    (TraceIntegrationTest._SwapChainPresentationModeToStr(
                        expected.presentation_mode),
                     TraceIntegrationTest._SwapChainPresentationModeListToStr(
                         presentation_mode_history)))
      valid_entry_found = True
    if not valid_entry_found:
      self.fail(
          'No valid frame statistics being collected: %s' % TraceIntegrationTest
          ._SwapChainPresentationModeListToStr(presentation_mode_history))

  def _EvaluateSuccess_CheckSwapChainPath(self, category, event_iterator,
                                          other_args):
    """Verifies that swap chains are used as expected for low latency canvas."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    overlay_bot_config = self.GetOverlayBotConfig()
    if overlay_bot_config is None:
      self.fail('Overlay bot config can not be determined')
    assert overlay_bot_config.get('direct_composition', False)

    expect_no_overlay = other_args and other_args.get('no_overlay', False)
    expect_overlay = not expect_no_overlay
    found_overlay = False

    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _PRESENT_TO_SWAP_CHAIN_EVENT_NAME:
        continue
      presentation_mode = event.args.get('image_type', None)
      if presentation_mode == 'swap chain':
        found_overlay = True
        break
    if expect_overlay and not found_overlay:
      self.fail(
          'Overlay expected but not found: matching %s events were not found' %
          _PRESENT_TO_SWAP_CHAIN_EVENT_NAME)
    elif expect_no_overlay and found_overlay:
      self.fail(
          'Overlay not expected but found: matching %s events were found' %
          _PRESENT_TO_SWAP_CHAIN_EVENT_NAME)

  def _EvaluateSuccess_CheckMainSwapChainPath(self, category, event_iterator,
                                              other_args):
    """Verified that Chrome's main swap chain is presented with full damage."""
    os_name = self.browser.platform.GetOSName()
    assert os_name and os_name.lower() == 'win'

    overlay_bot_config = self.GetOverlayBotConfig()
    if overlay_bot_config is None:
      self.fail('Overlay bot config can not be determined')
    assert overlay_bot_config.get('direct_composition', False)

    expect_full_damage = other_args and other_args.get('full_damage', False)

    partial_damage_encountered = False
    full_damage_encountered = False
    # Verify expectations through captured trace events.
    for event in event_iterator:
      if event.category != category:
        continue
      if event.name != _PRESENT_MAIN_SWAP_CHAIN_EVENT_NAME:
        continue
      dirty_rect = event.args.get('dirty_rect', None)
      if dirty_rect is None:
        continue
      if dirty_rect == 'full_damage':
        full_damage_encountered = True
      else:
        partial_damage_encountered = True

    # Today Chrome either run with full damage or partial damage, but not both.
    # This may change in the future.
    if (expect_full_damage != full_damage_encountered
        or expect_full_damage == partial_damage_encountered):
      self.fail('Expected events with name %s of %s, got others' %
                (_PRESENT_MAIN_SWAP_CHAIN_EVENT_NAME,
                 'full damage' if expect_full_damage else 'partial damage'))

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'trace_test_expectations.txt')
    ]


class _VideoExpectations(object):
  """Struct-like object for passing around video test expectations."""

  def __init__(self):
    self.pixel_format = None
    self.zero_copy = None
    self.no_overlay = None
    self.presentation_mode = None


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
