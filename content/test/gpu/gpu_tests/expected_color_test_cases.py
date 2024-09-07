# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from typing import Callable, Dict, List, Optional, Tuple, Union

from gpu_tests import common_browser_args as cba
from gpu_tests import crop_actions as ca
from gpu_tests import skia_gold_heartbeat_integration_test_base as sghitb

from telemetry.internal.browser import browser as browser_module

coordinate_tuple = collections.namedtuple('coordinate', ['x', 'y'])
size_tuple = collections.namedtuple('size', ['width', 'height'])
rgba_tuple = collections.namedtuple('rgba', ['r', 'g', 'b', 'a'])


class ExpectedColorExpectation():
  """Defines a single tested region within an image."""
  def __init__(self,
               location: Tuple[int, int],
               size: Tuple[int, int],
               color: Union[Tuple[int, int, int], Tuple[int, int, int, int]],
               tolerance: Optional[int] = None):
    """
    Args:
      location: A tuple of two ints denoting the upper left corner of the
          sampled region within the image.
      size: A tuple of two ints denoting the width and height of the sampled
          region within the image.
      color: A tuple of three or four ints denoting the expected RGB or RGBA
          value in the sampled region, respectively. If no alpha value is given,
          255 will be used as the default.
      tolerance: An int containing how much any one channel is allowed to
          differ and still be considered the same. Will override the test case's
          base tolerance if specified.
    """
    if len(color) == 3:
      color = (color[0], color[1], color[2], 255)

    self.location = coordinate_tuple(*location)
    self.size = size_tuple(*size)
    self.color = rgba_tuple(*color)
    self.tolerance = tolerance


class ExpectedColorTestCase(sghitb.SkiaGoldHeartbeatTestCase):
  """Defines a single expected color test."""
  def __init__(  # pylint: disable=too-many-arguments
      self,
      url: str,
      name: str,
      base_tolerance: int,
      expected_colors: List[ExpectedColorExpectation],
      crop_action: ca.BaseCropAction,
      *args,
      extra_browser_args: Optional[List[str]] = None,
      should_capture_full_screenshot_func: Optional[Callable[
          [browser_module.Browser], bool]] = None,
      scale_factor_overrides: Optional[Dict[str, float]] = None,
      **kwargs):
    """
    Args:
      url: A string containing the test page URL to load.
      name: A string containing the test name. Will be prefixed with an
          identifier for expected color tests.
      base_ tolerance: An int containing how much any one channel is allowed to
          differ and still be considered the same. This is the default tolerance
          that will be used for the test if individual color expectations do not
          override it.
      expected_colors: A list of all the ExpectedColorExpectations to check as
          part of the test.
      extra_browser_args: An optional list of strings containing any browser
          args to use while running the test.
      should_capture_full_screenshot_func: An optional function that takes a
          Telemetry Browser as input and returns a boolean. If this function
          returns True, a capture of the entire web contents will be taken.
          Otherwise, only the visible portion will be captured, which is more
          representative of behavior users will see. If this function is not
          specified, the visible-only code path will be used.
      scale_factor_overrides: An optional dict mapping device names to device
          pixel ratios to use instead of the one reported by the browser.
    """
    assert url
    assert name
    assert base_tolerance >= 0

    name = 'ExpectedColor_' + name
    super().__init__(name, *args, **kwargs)

    extra_browser_args = extra_browser_args or []
    if should_capture_full_screenshot_func is None:
      should_capture_full_screenshot_func = lambda _: False

    self.url = url
    self.base_tolerance = base_tolerance
    self.expected_colors = expected_colors
    self.crop_action = crop_action
    self.extra_browser_args = extra_browser_args
    self.ShouldCaptureFullScreenshot = should_capture_full_screenshot_func
    self.scale_factor_overrides = scale_factor_overrides or {}


def CaptureFullScreenshotOnFuchsia(browser: browser_module.Browser) -> bool:
  return browser.platform.GetOSName() == 'fuchsia'


def MapsTestCases() -> List[ExpectedColorTestCase]:
  class TestActionStartMapsTest(sghitb.TestAction):

    def Run(
        self, test_case: ExpectedColorTestCase, tab_data: sghitb.TabData,
        loop_state: sghitb.LoopState,
        test_instance: sghitb.SkiaGoldHeartbeatIntegrationTestBase
    ) -> None:  # pytype: disable=signature-mismatch
      sghitb.EvalInTestIframe(
          tab_data.tab, """
        function _checkIfTestCanStart() {
          if (window.startTest !== undefined) {
            window.startTest();
            _checkIfReadyForIdleCallback();
          } else {
            setTimeout(_checkIfTestCanStart, 100);
          }
        }

        function _checkIfReadyForIdleCallback() {
          if (window.testDone) {
            _setUpIdleCallback();
          } else {
            setTimeout(_checkIfReadyForIdleCallback, 100);
          }
        }

        function _setUpIdleCallback() {
          requestIdleCallback(() => {
            window.domAutomationController.send('SUCCESS');
          }, { timeout: 10000 });
        }

        _checkIfTestCanStart();
        """)

  return [
      ExpectedColorTestCase(
          'tools/perf/page_sets/maps_perf_test/performance.html',
          'maps',
          10,
          [
              # Light green.
              ExpectedColorExpectation(
                  location=(35, 239), size=(1, 1), color=(214, 232, 186)),
              # Darker green.
              ExpectedColorExpectation(
                  location=(318, 103), size=(1, 1), color=(203, 230, 163)),
              # Blue (lake).
              ExpectedColorExpectation(
                  location=(585, 385), size=(1, 1), color=(163, 205, 255)),
              # Gray.
              ExpectedColorExpectation(
                  location=(87, 109), size=(1, 1), color=(232, 232, 232)),
              # Tan.
              ExpectedColorExpectation(
                  location=(336, 255), size=(1, 1), color=(240, 237, 230)),
          ],
          test_actions=[
              sghitb.TestActionWaitForInnerTestPageLoad(),
              TestActionStartMapsTest(),
              sghitb.TestActionWaitForFinish(sghitb.DEFAULT_GLOBAL_TIMEOUT),
          ],
          crop_action=ca.NonWhiteContentCropAction(),
          extra_browser_args=[
              cba.ENSURE_FORCED_COLOR_PROFILE,
              cba.FORCE_BROWSER_CRASH_ON_GPU_CRASH,
              cba.FORCE_COLOR_PROFILE_SRGB,
          ],
          # Small Fuchsia screens result in an incomplete capture without this.
          should_capture_full_screenshot_func=CaptureFullScreenshotOnFuchsia,
          # Certain devices, particularly Android devices, report incorrect
          # device pixel ratios. These overrides are the correct values for
          # such devices calculated using the fact that this test should produce
          # an 800x600 image on a device with a DPR of 1.
          scale_factor_overrides={
              'Nexus 5': 1.105,
              'Nexus 5X': 1.105,
              # NVIDIA Shield.
              'sb_na_wf': 1.226,
              'Pixel 2': 1.1067,
              'Pixel 4': 1.1025,
              'Pixel 6': 1.10375,
              # Samsung A13.
              'SM-A135M': 1.1025,
              # Samsung A23.
              'SM-A235M': 1.1025,
              # Samsung S23.
              'SM-S911U1': 1.1,
              # Motorola Moto G Power 5G.
              'moto g power 5G - 2023': 1.1,
          }),
  ]


def MediaRecorderTestCases() -> List[ExpectedColorTestCase]:
  red = (255, 0, 0)
  green = (0, 255, 0)
  blue = (0, 0, 255)
  yellow = (255, 255, 0)

  # Check pixels within two 128x128 elements that are horizontally adjacent with
  # a 4 pixel gap between them. Use a 10-pixel buffer between the sample areas
  # and edges/color borders.
  canvas_expected_colors = [
      # Source canvas (left image).
      ExpectedColorExpectation(location=(10, 10), size=(44, 44), color=yellow),
      ExpectedColorExpectation(location=(78, 10), size=(44, 44), color=red),
      ExpectedColorExpectation(location=(10, 78), size=(44, 44), color=blue),
      ExpectedColorExpectation(location=(78, 78), size=(44, 44), color=green),
      # Destination video (right image).
      ExpectedColorExpectation(location=(142, 10), size=(44, 44), color=yellow),
      ExpectedColorExpectation(location=(206, 10), size=(44, 44), color=red),
      ExpectedColorExpectation(location=(142, 78), size=(44, 44), color=blue),
      ExpectedColorExpectation(location=(206, 78),
                               size=(44, 44),
                               color=green,
                               tolerance=100),
  ]

  # Check pixels within two 240x135 elements that are vertically adjacent with
  # a 15 pixel gap between them. Use a 10-pixel buffer between the sample areas
  # and edges/color borders.
  video_expected_colors = [
      # Destination video (top image).
      ExpectedColorExpectation(location=(10, 10), size=(100, 47), color=yellow),
      ExpectedColorExpectation(location=(130, 10), size=(100, 47), color=red),
      ExpectedColorExpectation(location=(10, 78), size=(100, 47), color=blue),
      ExpectedColorExpectation(location=(130, 78),
                               size=(100, 47),
                               color=green,
                               tolerance=100),
      # Source video (bottom image).
      ExpectedColorExpectation(location=(10, 160), size=(100, 47),
                               color=yellow),
      ExpectedColorExpectation(location=(130, 160), size=(100, 47), color=red),
      ExpectedColorExpectation(location=(10, 228), size=(100, 47), color=blue),
      ExpectedColorExpectation(location=(130, 228),
                               size=(100, 47),
                               color=green,
                               tolerance=100),
  ]

  # Full cycle capture-encode-decode test for MediaRecorder capturing canvas.
  # Large tolerances are used since the test only cares that the colors are
  # vaguely red/blue/green/yellow.
  return [
      ExpectedColorTestCase(
          'content/test/data/gpu/pixel_media_recorder_from_canvas_2d.html',
          'MediaRecorderFrom2DCanvas',
          60,
          canvas_expected_colors,
          crop_action=ca.NonWhiteContentCropAction(),
      ),
      ExpectedColorTestCase(
          'content/test/data/gpu/pixel_media_recorder_from_video_element.html',
          'MediaRecorderFromVideoElement',
          60,
          video_expected_colors,
          crop_action=ca.NonWhiteContentCropAction(),
      ),
      ExpectedColorTestCase(
          'content/test/data/gpu/pixel_media_recorder_from_video_element.html',
          'MediaRecorderFromVideoElementWithOoprCanvasDisabled',
          60,
          video_expected_colors,
          crop_action=ca.NonWhiteContentCropAction(),
          extra_browser_args=['--disable-features=CanvasOopRasterization']),
  ]
