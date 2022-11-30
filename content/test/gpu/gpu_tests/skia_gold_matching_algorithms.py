# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Classes related to the possible matching algorithms for Skia Gold."""

from typing import List, Union


class Parameters():
  """Constants for Skia Gold algorithm parameters.

  These correspond to the constants defined in goldctl's
  imgmatching/constants.go.
  """
  # The max number of pixels in an image that can differ and still allow the
  # fuzzy comparison to pass.
  MAX_DIFFERENT_PIXELS = 'fuzzy_max_different_pixels'
  # The max RGBA difference between two pixels that is still considered valid.
  # For example, if a pixel differs by (1, 2, 3, 0), then the threshold would
  # need to be 6 or higher in order for the fuzzy comparison to pass.
  PIXEL_DELTA_THRESHOLD = 'fuzzy_pixel_delta_threshold'
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
               pixel_delta_threshold: int,
               ignored_border_thickness: int = 0):
    super().__init__()
    assert int(max_different_pixels) >= 0
    assert int(pixel_delta_threshold) >= 0
    assert int(ignored_border_thickness) >= 0
    self._max_different_pixels = max_different_pixels
    self._pixel_delta_threshold = pixel_delta_threshold
    self._ignored_border_thickness = ignored_border_thickness

  def GetCmdline(self) -> List[str]:
    retval = super().GetCmdline()
    retval.extend(
        _GenerateOptionalKey(Parameters.MAX_DIFFERENT_PIXELS,
                             self._max_different_pixels))
    retval.extend(
        _GenerateOptionalKey(Parameters.PIXEL_DELTA_THRESHOLD,
                             self._pixel_delta_threshold))
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

  def __init__(self,
               max_different_pixels: int,
               pixel_delta_threshold: int,
               edge_threshold: int,
               ignored_border_thickness: int = 0):
    super().__init__(max_different_pixels, pixel_delta_threshold,
                     ignored_border_thickness)
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
