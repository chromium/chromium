# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes related to the possible matching algorithms for Skia Gold."""

import math
from typing import List, Optional, Union


class Parameters():
  """Constants for Skia Gold algorithm parameters.

  These correspond to the constants defined in goldctl's
  imgmatching/constants.go.
  """
  # The max number of pixels in an image that can differ and still allow the
  # fuzzy comparison to pass.
  MAX_DIFFERENT_PIXELS = 'fuzzy_max_different_pixels'
  # The max RGBA sum difference between two pixels that is still considered
  # valid. For example, if a pixel differs by (1, 2, 3, 0), then the threshold
  # would need to be 6 or higher in order for the fuzzy comparison to pass.
  # Mutually exclusive with PIXEL_PER_CHANNEL_DELTA_THRESHOLD.
  PIXEL_DELTA_THRESHOLD = 'fuzzy_pixel_delta_threshold'
  # The max per-channel RGBA difference between two pixels that is still
  # considered valid. For example, if a pixel differs by (1, 2, 3, 0), then the
  # threshold would need to be 3 or higher in order for the fuzzy comparison to
  # pass. Mutually exclusive with PIXEL_DELTA_THRESHOLD.
  PIXEL_PER_CHANNEL_DELTA_THRESHOLD = 'fuzzy_pixel_per_channel_delta_threshold'
  # How many pixels along the border of the image to ignore. 0 is typical for
  # most tests. 1 is useful for tests that have edges that go all the way to the
  # borders of the image, as Sobel filters do not get applied to pixels that are
  # on the image border. Larger values are supported, but don't have much of a
  # use case.
  IGNORED_BORDER_THICKNESS = 'fuzzy_ignored_border_thickness'
  # A number in the range [0, 255] specifying how much of an image should be
  # blacked out when using a Sobel filter. 0 results in the most pixels being
  # blacked out, while 255 results in no pixels being blacked out, i.e. no
  # Sobel filter functionality.
  EDGE_THRESHOLD = 'sobel_edge_threshold'
  # A number in the range [1, sqrt(MAX_INT32)] specifying how large the sample
  # area should be for the sample_area algorithm.
  SAMPLE_AREA_WIDTH = 'sample_area_width'
  # A number in the range [0, SAMPLE_AREA_WIDTH^2] specifying how many pixels
  # are allowed to differ in the sample area and not cause the matching to fail
  # in the sample_area algorithm.
  SAMPLE_AREA_MAX_DIFFERENT_PIXELS_PER_AREA = (
      'sample_area_max_different_pixels_per_area')
  # An optional number in the range [0, 255] specifying how much a pair of
  # pixels between the two images can differ on a single channel and still be
  # considered identical when using the sample_area algorithm.
  SAMPLE_AREA_CHANNEL_DELTA_THRESHOLD = 'sample_area_channel_delta_threshold'


class SkiaGoldMatchingAlgorithm():
  ALGORITHM_KEY = 'image_matching_algorithm'
  """Abstract base class for all algorithms."""

  def GetCmdline(self) -> List[str]:
    """Gets command line parameters for the algorithm.

    Returns:
      A list of strings representing the algorithm's parameters. The returned
      list is suitable for extending an existing goldctl imgtest add
      commandline, which will cause goldctl to use the specified algorithm
      instead of the default.
    """
    return _GenerateOptionalKey(SkiaGoldMatchingAlgorithm.ALGORITHM_KEY,
                                self.Name())

  def Name(self) -> str:
    """Returns a string representation of the algorithm."""
    raise NotImplementedError()


class ExactMatchingAlgorithm(SkiaGoldMatchingAlgorithm):
  """Class for the default exact matching algorithm in Gold."""

  def GetCmdline(self) -> List[str]:
    return []

  def Name(self) -> str:
    return 'exact'


class FuzzyMatchingAlgorithm(SkiaGoldMatchingAlgorithm):
  """Class for the fuzzy matching algorithm in Gold."""

  def __init__(self,
               max_different_pixels: int,
               pixel_delta_threshold: int = 0,
               pixel_per_channel_delta_threshold: int = 0,
               ignored_border_thickness: int = 0):
    super().__init__()
    assert max_different_pixels >= 0
    assert pixel_delta_threshold >= 0
    assert pixel_per_channel_delta_threshold >= 0
    assert not (pixel_delta_threshold > 0
                and pixel_per_channel_delta_threshold > 0)
    assert ignored_border_thickness >= 0
    self._max_different_pixels = max_different_pixels
    self._pixel_delta_threshold = pixel_delta_threshold
    self._pixel_per_channel_delta_threshold = pixel_per_channel_delta_threshold
    self._ignored_border_thickness = ignored_border_thickness

  def GetCmdline(self) -> List[str]:
    retval = super().GetCmdline()
    retval.extend(
        _GenerateOptionalKey(Parameters.MAX_DIFFERENT_PIXELS,
                             self._max_different_pixels))
    if self._pixel_delta_threshold:
      retval.extend(
          _GenerateOptionalKey(Parameters.PIXEL_DELTA_THRESHOLD,
                               self._pixel_delta_threshold))
    if self._pixel_per_channel_delta_threshold:
      retval.extend(
          _GenerateOptionalKey(Parameters.PIXEL_PER_CHANNEL_DELTA_THRESHOLD,
                               self._pixel_per_channel_delta_threshold))
    retval.extend(
        _GenerateOptionalKey(Parameters.IGNORED_BORDER_THICKNESS,
                             self._ignored_border_thickness))
    return retval

  def Name(self) -> str:
    return 'fuzzy'


class SobelMatchingAlgorithm(FuzzyMatchingAlgorithm):
  """Class for the Sobel filter matching algorithm in Gold.

  Technically a superset of the fuzzy matching algorithm.
  """

  def __init__(self, edge_threshold: int, *args, **kwargs):
    super().__init__(*args, **kwargs)
    assert int(edge_threshold) >= 0
    assert int(edge_threshold) <= 255
    if edge_threshold == 255:
      raise RuntimeError(
          'Sobel matching with edge threshold set to 255 is the same as fuzzy '
          'matching.')
    self._edge_threshold = edge_threshold

  def GetCmdline(self) -> List[str]:
    retval = super().GetCmdline()
    retval.extend(
        _GenerateOptionalKey(Parameters.EDGE_THRESHOLD, self._edge_threshold))
    return retval

  def Name(self) -> str:
    return 'sobel'


def _GenerateOptionalKey(key: str, value: Union[int, str]) -> List[str]:
  return ['--add-test-optional-key', '%s:%s' % (key, value)]


class SampleAreaMatchingAlgorithm(SkiaGoldMatchingAlgorithm):
  """Class for the sample_area matching algorithm in Gold."""

  def __init__(self,
               sample_area_width: int,
               max_different_pixels_per_area: int,
               sample_area_channel_delta_threshold: Optional[int] = None):
    super().__init__()
    assert sample_area_width >= 1
    assert sample_area_width <= math.sqrt(2**31 - 1)
    assert max_different_pixels_per_area >= 0
    assert max_different_pixels_per_area <= sample_area_width**2
    if max_different_pixels_per_area == sample_area_width**2:
      raise RuntimeError(
          'sample_area matching with a max different pixels per area set to '
          'the sample area size is equivalent to auto-approving any image.')
    if sample_area_channel_delta_threshold is not None:
      assert sample_area_channel_delta_threshold >= 0
      assert sample_area_channel_delta_threshold <= 255
      if sample_area_channel_delta_threshold == 255:
        raise RuntimeError(
            'sample area matching with a tolerance of 255 is equivalent to '
            'auto-approving any image.')
    self._sample_area_width = sample_area_width
    self._max_different_pixels_per_area = max_different_pixels_per_area
    self._sample_area_channel_delta_threshold = (
        sample_area_channel_delta_threshold)

  def GetCmdline(self) -> List[str]:
    retval = super().GetCmdline()
    retval.extend(
        _GenerateOptionalKey(Parameters.SAMPLE_AREA_WIDTH,
                             self._sample_area_width))
    retval.extend(
        _GenerateOptionalKey(
            Parameters.SAMPLE_AREA_MAX_DIFFERENT_PIXELS_PER_AREA,
            self._max_different_pixels_per_area))
    if self._sample_area_channel_delta_threshold is not None:
      retval.extend(
          _GenerateOptionalKey(Parameters.SAMPLE_AREA_CHANNEL_DELTA_THRESHOLD,
                               self._sample_area_channel_delta_threshold))
    return retval

  def Name(self) -> str:
    return 'sample_area'
