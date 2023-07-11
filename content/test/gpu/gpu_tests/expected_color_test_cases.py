# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
from typing import Callable, List, Optional, Tuple, Union

from gpu_tests import common_browser_args as cba
from gpu_tests import skia_gold_integration_test_base

from telemetry.internal.browser import browser as browser_module
from telemetry.internal.browser import tab as tab_module

coordinate_tuple = collections.namedtuple('coordinate', ['x', 'y'])
size_tuple = collections.namedtuple('size', ['width', 'height'])
rgba_tuple = collections.namedtuple('rgba', ['r', 'g', 'b', 'a'])


class ExpectedColorExpectation():
  """Defines a single tested region within an image."""
  def __init__(self, location: Tuple[int, int], size: Tuple[int, int],
               color: Union[Tuple[int, int, int], Tuple[int, int, int, int]]):
    """
    Args:
      location: A tuple of two ints denoting the upper left corner of the
          sampled region within the image.
      size: A tuple of two ints denoting the width and height of the sampled
          region within the image.
      color: A tuple of three or four ints denoting the expected RGB or RGBA
          value in the sampled region, respectively. If no alpha value is given,
          255 will be used as the default.
    """
    if len(color) == 3:
      color = (color[0], color[1], color[2], 255)

    self.location = coordinate_tuple(*location)
    self.size = size_tuple(*size)
    self.color = rgba_tuple(*color)


class ExpectedColorTestCase(skia_gold_integration_test_base.SkiaGoldTestCase):
  """Defines a single expected color test."""
  def __init__(  # pylint: disable=too-many-arguments
      self,
      url: str,
      name: str,
      tolerance: int,
      pre_capture_action: Callable[['ExpectedColorTestCase', tab_module.Tab],
                                   None],
      expected_colors: List[ExpectedColorExpectation],
      *args,
      extra_browser_args: Optional[List[str]] = None,
      should_capture_full_screenshot_func: Optional[Callable[
          [browser_module.Browser], bool]] = None,
      **kwargs):
    """
    Args:
      url: A string containing the test page URL to load.
      name: A string containing the test name. Will be prefixed with an
          identifier for expected color tests.
      tolerance: An int containing how much any one channel is allowed to differ
          and still be considered the same.
      pre_capture_action: A function that takes an ExpectedColorTestCase and a
          Telemetry Tab as inputs. Will be run between |url| being loaded and
          the test image being captured.
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
    """
    assert url
    assert name
    assert tolerance >= 0
    assert expected_colors

    name = 'ExpectedColor_' + name
    super().__init__(name, *args, **kwargs)

    extra_browser_args = extra_browser_args or []
    if should_capture_full_screenshot_func is None:
      should_capture_full_screenshot_func = lambda _: False

    self.url = url
    self.tolerance = tolerance
    self.pre_capture_action = pre_capture_action
    self.expected_colors = expected_colors
    self.extra_browser_args = extra_browser_args
    self.ShouldCaptureFullScreenshot = should_capture_full_screenshot_func


def CaptureFullScreenshotOnFuchsia(browser: browser_module.Browser) -> bool:
  return browser.platform.GetOSName() == 'fuchsia'


def MapsTestCases() -> List[ExpectedColorTestCase]:
  def MapsPreCaptureAction(_: ExpectedColorTestCase,
                           tab: tab_module.Tab) -> None:
    action_runner = tab.action_runner
    action_runner.WaitForJavaScriptCondition('window.startTest != undefined')
    action_runner.EvaluateJavaScript('window.startTest()')
    action_runner.WaitForJavaScriptCondition('window.testDone', timeout=320)

    # Wait for the page to process immediate work and load tiles.
    action_runner.EvaluateJavaScript("""
        window.testCompleted = false;
        requestIdleCallback(
            () => window.testCompleted = true,
            { timeout : 10000 })""")
    action_runner.WaitForJavaScriptCondition('window.testCompleted', timeout=30)

  return [
      ExpectedColorTestCase(
          'tools/perf/page_sets/maps_perf_test/performance.html',
          'maps',
          10,
          MapsPreCaptureAction,
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
          extra_browser_args=[
              cba.ENSURE_FORCED_COLOR_PROFILE,
              cba.FORCE_BROWSER_CRASH_ON_GPU_CRASH,
              cba.FORCE_COLOR_PROFILE_SRGB,
          ],
          # Small Fuchsia screens result in an incomplete capture without this.
          should_capture_full_screenshot_func=CaptureFullScreenshotOnFuchsia),
  ]
