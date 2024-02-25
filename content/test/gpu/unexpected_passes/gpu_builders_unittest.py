#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import unittest

from unexpected_passes import gpu_builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types


class BuilderRunsTestOfInterestUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self.instance = gpu_builders.GpuBuilders('webgl_conformance', False)

  def testMatch(self) -> None:
    """Tests that a match can be successfully found."""
    test_map = {
        'isolated_scripts': [
            {
                'args': [
                    'webgl_conformance',
                ],
                'test': 'telemetry_gpu_integration_test',
            },
        ],
    }
    self.assertTrue(self.instance._BuilderRunsTestOfInterest(test_map))

  def testMatchSkylab(self) -> None:
    """Tests that a match can be successfully found for Skylab builders."""
    test_map = {
        'skylab_tests': [
            {
                'args': [
                    'webgl_conformance',
                ],
                'test': 'telemetry_gpu_integration_test',
            },
        ],
    }
    self.assertTrue(self.instance._BuilderRunsTestOfInterest(test_map))

  def testNoMatchIsolate(self) -> None:
    """Tests that a match is not found if the isolate name is not valid."""
    test_map = {
        'isolated_scripts': [
            {
                'args': [
                    'webgl_conformance',
                ],
                'test': 'not_telemetry',
            },
        ],
    }
    self.assertFalse(self.instance._BuilderRunsTestOfInterest(test_map))

  def testNoMatchSkylabTest(self) -> None:
    """Tests that a match is not found for Skylab if test name is not valid."""
    test_map = {
        'skylab_tests': [
            {
                'args': [
                    'webgl_conformance',
                ],
                'test': 'not_telemetry',
            },
        ],
    }
    self.assertFalse(self.instance._BuilderRunsTestOfInterest(test_map))

  def testNoMatchSuite(self) -> None:
    """Tests that a match is not found if the suite name is not valid."""
    test_map = {
        'isolated_scripts': [
            {
                'args': [
                    'not_a_suite',
                ],
                'test': 'telemetry_gpu_integration_test',
            },
        ],
    }
    self.assertFalse(self.instance._BuilderRunsTestOfInterest(test_map))

  def testNoMatchSuiteSkylab(self) -> None:
    """Tests that a match is not found if Skylab suite name is not valid."""
    test_map = {
        'skylab_tests': [
            {
                'args': [
                    'not_a_suite',
                ],
                'test': 'telemetry_gpu_integration_test',
            },
        ],
    }
    self.assertFalse(self.instance._BuilderRunsTestOfInterest(test_map))

  def testAndroidSuffixes(self) -> None:
    """Tests that Android-specific isolates are added."""
    isolate_names = self.instance.GetIsolateNames()
    for isolate in isolate_names:
      if 'telemetry_gpu_integration_test' in isolate and 'android' in isolate:
        return
    self.fail('Did not find any Android-specific isolate names')


class GetNonChromiumBuildersUnittest(unittest.TestCase):
  def testStringsConvertedToBuilderEntries(self) -> None:
    """Tests that the easier-to-read strings get converted to BuilderEntry."""
    instance = gpu_builders.GpuBuilders('webgl_conformance', False)
    builder = data_types.BuilderEntry('Win V8 FYI Release (NVIDIA)',
                                      constants.BuilderTypes.CI, False)
    self.assertIn(builder, instance.GetNonChromiumBuilders())


if __name__ == '__main__':
  unittest.main(verbosity=2)
