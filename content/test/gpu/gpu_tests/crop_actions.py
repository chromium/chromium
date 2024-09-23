# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes for defining how to crop screenshots in pixel-related tests."""

import abc
from typing import Optional, Tuple

from gpu_tests import common_typing as ct

from telemetry.util import image_util


class BaseCropAction(abc.ABC):

  @abc.abstractmethod
  def CropScreenshot(self, screenshot: ct.Screenshot, dpr: float,
                     device_type: str, os_name: str) -> ct.Screenshot:
    """Return a cropped copy of |screenshot|.

    The exact behavior is dependent on the concrete class.
    """


class NoOpCropAction(BaseCropAction):

  def CropScreenshot(self, screenshot: ct.Screenshot, dpr: float,
                     device_type: str, os_name: str) -> ct.Screenshot:
    del dpr, device_type, os_name  # unused
    return screenshot


class FixedRectCropAction(BaseCropAction):
  """Crops screenshots to the given rectangle.

  The rectangle is first scaled based on the device pixel ratio.
  """
  # The value needed varies depending on device type, likely due to resolution:
  #   * Pixel 4: 10
  #   * Samsung A23: 11
  #   * Samsung S23: 12
  # Use the largest value for simplicity instead of attempting to change it
  # dynamically.
  SCROLLBAR_WIDTH = 12

  def __init__(self, x1: int, y1: int, x2: Optional[int], y2: Optional[int]):
    """
    Args:
      x1: An int specifying the x coordinate of the top left corner of the crop
          rectangle
      y1: An int specifying the y coordinate of the top left corner of the crop
          rectangle
      x2: An int specifying the x coordinate of the bottom right corner of the
          crop rectangle. Can be None to explicitly specify the right side of
          the image, although clamping will be performed regardless.
      y2: An int specifying the y coordinate of the bottom right corner of the
          crop rectangle. Can be None to explicitly specify the bottom of the
          image, although clamping will be performed regardless.
    """
    assert x1 >= 0
    assert y1 >= 0
    assert x2 is None or x2 > x1
    assert y2 is None or y2 > y1
    self._x1 = x1
    self._y1 = y1
    self._x2 = x2
    self._y2 = y2

  def CropScreenshot(self, screenshot: ct.Screenshot, dpr: float,
                     device_type: str, os_name: str) -> ct.Screenshot:
    del device_type, os_name  # unused
    start_x = int(self._x1 * dpr)
    start_y = int(self._y1 * dpr)

    # When actually clamping the value, it's possible we'll catch the
    # scrollbar, so account for its width in the clamp.
    max_x = image_util.Width(screenshot) - FixedRectCropAction.SCROLLBAR_WIDTH
    max_y = image_util.Height(screenshot)

    if self._x2 is None:
      end_x = max_x
    else:
      end_x = min(int(self._x2 * dpr), max_x)
    if self._y2 is None:
      end_y = max_y
    else:
      end_y = min(int(self._y2 * dpr), max_y)

    crop_width = end_x - start_x
    crop_height = end_y - start_y
    return image_util.Crop(screenshot, start_x, start_y, crop_width,
                           crop_height)


class NonWhiteContentCropAction(BaseCropAction):
  """Crops screenshots to remove all white (background) content."""
  OFF_WHITE_TOP_ROW_DEVICES = {
      # Samsung A13.
      'SM-A135M',
      # Samsung A23.
      'SM-A235M',
  }

  def __init__(self, initial_crop: Optional[BaseCropAction] = None):
    """
    Args:
      initial_crop: An initial crop to perform before removing the background.
          Intended to reduce the amount of work done finding the non-white
          content if the content of interest is known to be small relative to
          the entire screenshot.
    """
    self._initial_crop = initial_crop

  def CropScreenshot(self, screenshot: ct.Screenshot, dpr: float,
                     device_type: str, os_name: str) -> ct.Screenshot:
    # The bottom corners of Mac screenshots have black triangles due to the
    # rounded corners of Mac windows. So, crop the bottom few rows off now to
    # get rid of those.
    if os_name == 'mac':
      screenshot = image_util.Crop(screenshot, 0, 0,
                                   image_util.Width(screenshot),
                                   image_util.Height(screenshot) - 20)
    # GPU tests typically capture screenshots from the OS level codepath instead
    # of directly from the web contents. This is because capturing from the
    # web contents may cause the content to be re-rendered, which may hide bugs.
    # A side effect of this is that browser UI is barely visible in the first
    # row of pixels on some devices, which will affect our ability to detect
    # the white background. So, preemptively crop off the top row on such
    # devices.
    if device_type in NonWhiteContentCropAction.OFF_WHITE_TOP_ROW_DEVICES:
      screenshot = image_util.Crop(screenshot, 0, 1,
                                   image_util.Width(screenshot),
                                   image_util.Height(screenshot) - 1)
    if self._initial_crop:
      screenshot = self._initial_crop.CropScreenshot(screenshot, dpr,
                                                     device_type, os_name)

    x1, y1, x2, y2 = _GetNonWhiteCropBoundaries(screenshot)
    return image_util.Crop(screenshot, x1, y1, x2 - x1, y2 - y1)


def _GetNonWhiteCropBoundaries(
    screenshot: ct.Screenshot) -> Tuple[int, int, int, int]:
  """Returns the boundaries to crop the screenshot to.

  Specifically, we look for the boundaries where the white background
  transitions into the (non-white) content we care about.

  Returns:
    A 4-tuple (x1, y1, x2, y2) denoting the top left and bottom right
    coordinates to crop to.
  """
  img_height = image_util.Height(screenshot)
  img_width = image_util.Width(screenshot)

  # Accessing pixels directly via image_util.GetPixelColor is weirdly slow,
  # likely due to the underlying implementation (some numpy data type) not
  # being great for random access. So, we instead get the pixels as a single
  # byte array (whose pixel order is left to right, top to bottom) and
  # manually calculate the offsets for each pixel ourselves. This results in
  # the boundary calculation being ~13x faster.
  pixel_data = image_util.Pixels(screenshot)
  channels = image_util.Channels(screenshot)

  # We include start/end as optional arguments as an optimization for finding
  # the lower right corner. If the original image is large and the non-white
  # portions are small and in the upper left (which is the most common case),
  # checking every row/column for white can take a while.
  def RowIsWhite(row, start=None, end=None):
    row_offset = row * img_width * channels
    start = start or 0
    end = end or img_width
    for col in range(start, end):
      col_offset = col * channels
      pixel_index = row_offset + col_offset
      r = pixel_data[pixel_index]
      g = pixel_data[pixel_index + 1]
      b = pixel_data[pixel_index + 2]
      if r != 255 or g != 255 or b != 255:
        return False
    return True

  def ColumnIsWhite(column, start=None, end=None):
    column_offset = column * channels
    start = start or 0
    end = end or img_height
    for row in range(start, end):
      row_offset = row * img_width * channels
      pixel_index = row_offset + column_offset
      r = pixel_data[pixel_index]
      g = pixel_data[pixel_index + 1]
      b = pixel_data[pixel_index + 2]
      if r != 255 or g != 255 or b != 255:
        return False
    return True

  x1 = y1 = 0
  x2 = img_width
  y2 = img_height
  for column in range(img_width):
    if not ColumnIsWhite(column):
      x1 = column
      break
  else:
    raise RuntimeError(
        'Attempted to crop to non-white content in an all white image')

  for row in range(img_height):
    if not RowIsWhite(row, start=x1):
      y1 = row
      break

  # We work from the right/bottom of the image here in case there are multiple
  # things that need to be tested separated by whitespace like is the case for
  # many video-related tests.
  for column in range(img_width - 1, x1 - 1, -1):
    if not ColumnIsWhite(column, start=y1):
      x2 = column + 1
      break

  for row in range(img_height - 1, y1 - 1, -1):
    if not RowIsWhite(row, start=x1, end=x2):
      y2 = row + 1
      break
  return x1, y1, x2, y2
