# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import unittest

from gpu_tests import skia_gold_matching_algorithms as algo


class ExactMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdline(self) -> None:
    a = algo.ExactMatchingAlgorithm()
    self.assertEqual(a.GetCmdline(), [])


class FuzzyMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdline(self) -> None:
    a = algo.FuzzyMatchingAlgorithm(1, 2, 3)
    cmdline = a.GetCmdline()
    self.assertEqual(cmdline, [
        '--add-test-optional-key',
        'image_matching_algorithm:fuzzy',
        '--add-test-optional-key',
        'fuzzy_max_different_pixels:1',
        '--add-test-optional-key',
        'fuzzy_pixel_delta_threshold:2',
        '--add-test-optional-key',
        'fuzzy_ignored_border_thickness:3',
    ])

  def testInvalidArgs(self) -> None:
    with self.assertRaises(AssertionError):
      algo.FuzzyMatchingAlgorithm(-1, 0)
    with self.assertRaises(AssertionError):
      algo.FuzzyMatchingAlgorithm(0, -1)
    with self.assertRaises(AssertionError):
      algo.FuzzyMatchingAlgorithm(0, 0, -1)


class SobelMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdline(self) -> None:
    a = algo.SobelMatchingAlgorithm(1, 2, 3, 4)
    cmdline = a.GetCmdline()
    self.assertEqual(cmdline, [
        '--add-test-optional-key',
        'image_matching_algorithm:sobel',
        '--add-test-optional-key',
        'fuzzy_max_different_pixels:1',
        '--add-test-optional-key',
        'fuzzy_pixel_delta_threshold:2',
        '--add-test-optional-key',
        'fuzzy_ignored_border_thickness:4',
        '--add-test-optional-key',
        'sobel_edge_threshold:3',
    ])

  def testInvalidArgs(self) -> None:
    with self.assertRaises(AssertionError):
      algo.SobelMatchingAlgorithm(1, 2, -1)
    with self.assertRaises(AssertionError):
      algo.SobelMatchingAlgorithm(1, 2, 256)
    with self.assertRaises(RuntimeError):
      algo.SobelMatchingAlgorithm(1, 2, 255)


class SampleAreaMatchingAlgorithmTest(unittest.TestCase):
  def testGetCmdlineNoTolerance(self) -> None:
    a = algo.SampleAreaMatchingAlgorithm(2, 1)
    cmdline = a.GetCmdline()
    self.assertEqual(cmdline, [
        '--add-test-optional-key',
        'image_matching_algorithm:sample_area',
        '--add-test-optional-key',
        'sample_area_width:2',
        '--add-test-optional-key',
        'sample_area_max_different_pixels_per_area:1',
    ])

  def testGetCmdlineWithTolerance(self) -> None:
    a = algo.SampleAreaMatchingAlgorithm(2, 1, 3)
    cmdline = a.GetCmdline()
    self.assertEqual(cmdline, [
        '--add-test-optional-key',
        'image_matching_algorithm:sample_area',
        '--add-test-optional-key',
        'sample_area_width:2',
        '--add-test-optional-key',
        'sample_area_max_different_pixels_per_area:1',
        '--add-test-optional-key',
        'sample_area_channel_delta_threshold:3',
    ])

  def testInvalidArgs(self) -> None:
    # sample_area_width.
    with self.assertRaises(AssertionError):
      algo.SampleAreaMatchingAlgorithm(0, 1, 3)
    with self.assertRaises(AssertionError):
      algo.SampleAreaMatchingAlgorithm(int(math.sqrt(2**31 - 1) + 1), 1, 3)
    with self.assertRaises(AssertionError):
      algo.SampleAreaMatchingAlgorithm(2, -1, 3)
    # max_different_pixels_per_area.
    with self.assertRaises(AssertionError):
      algo.SampleAreaMatchingAlgorithm(2, 5, 3)
    with self.assertRaises(RuntimeError):
      algo.SampleAreaMatchingAlgorithm(2, 4, 3)
    # sample_area_tolerance.
    with self.assertRaises(AssertionError):
      algo.SampleAreaMatchingAlgorithm(2, 1, -1)
    with self.assertRaises(AssertionError):
      algo.SampleAreaMatchingAlgorithm(2, 1, 256)
    with self.assertRaises(RuntimeError):
      algo.SampleAreaMatchingAlgorithm(2, 1, 255)


if __name__ == '__main__':
  unittest.main(verbosity=2)
