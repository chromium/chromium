#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import unittest
import unittest.mock as mock

from flake_suppressor import gpu_expectations
from flake_suppressor import gpu_results
from flake_suppressor import gpu_tag_utils as tag_utils

from flake_suppressor_common import data_types
from flake_suppressor_common import tag_utils as common_tag_utils

GENERIC_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win ]
# results: [ Failure ]
crbug.com/1111 [ win ] foo_test [ Failure ]
"""

GPU_EXPECTATION_FILE_CONTENTS = """\
# tags: [ win ]
# tags: [ amd nvidia ]
# results: [ Failure ]
crbug.com/1111 [ win nvidia ] conformance/textures/misc/video-rotation.html [ Failure ]
"""


class GPUResultsUnittest(unittest.TestCase):
  def setUp(self) -> None:
    common_tag_utils.SetTagUtilsImplementation(tag_utils.GpuTagUtils)
    expectations_processor = gpu_expectations.GpuExpectationProcessor()
    self._results = gpu_results.GpuResultProcessor(expectations_processor)
    self._local_patcher = mock.patch(
        'flake_suppressor_common.results.expectations.'
        'ExpectationProcessor.GetLocalCheckoutExpectationFileContents')
    self._local_mock = self._local_patcher.start()
    self._local_mock.return_value = {}
    self.addCleanup(self._local_patcher.stop)


class AggregateResultsUnittest(GPUResultsUnittest):
  def testWithFiltering(self) -> None:
    """Tests that results are properly filtered out."""
    self._local_mock.return_value = {
        'webgl_conformance_expectations.txt': GPU_EXPECTATION_FILE_CONTENTS,
    }
    query_results = [
        # Expected to be removed.
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            'typ_tags': ['win', 'nvidia'],
        },
        # Expected to be removed.
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-2222',
            'typ_tags': ['win', 'nvidia'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-3333',
            'typ_tags': ['win', 'amd'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/texture-npot-video.html'),
            'id':
            'build-4444',
            'typ_tags': ['win', 'nvidia'],
        },
        {
            'name': ('gpu_tests.pixel_integration_test.PixelIntegrationTest.'
                     'Pixel_CSS3DBlueBox'),
            'id':
            'build-5555',
            'typ_tags': ['win', 'nvidia'],
        },
    ]

    expected_output = {
        'webgl_conformance_integration_test': {
            'conformance/textures/misc/video-rotation.html': {
                ('amd', 'win'): ['http://ci.chromium.org/b/3333'],
            },
            'conformance/textures/misc/texture-npot-video.html': {
                ('nvidia', 'win'): ['http://ci.chromium.org/b/4444'],
            },
        },
        'pixel_integration_test': {
            'Pixel_CSS3DBlueBox': {
                ('nvidia', 'win'): ['http://ci.chromium.org/b/5555'],
            },
        },
    }

    self.assertEqual(self._results.AggregateResults(query_results),
                     expected_output)


class ConvertJsonResultsToResultObjectsUnittest(GPUResultsUnittest):
  def testDuplicateResults(self) -> None:
    """Tests that duplicate results are not merged."""
    r = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            'typ_tags': ['win', 'nvidia'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            'typ_tags': ['win', 'nvidia'],
        },
    ]
    expected_results = [
        data_types.Result('webgl_conformance_integration_test',
                          'conformance/textures/misc/video-rotation.html',
                          ('nvidia', 'win'), '1111'),
        data_types.Result('webgl_conformance_integration_test',
                          'conformance/textures/misc/video-rotation.html',
                          ('nvidia', 'win'), '1111'),
    ]
    self.assertEqual(self._results._ConvertJsonResultsToResultObjects(r),
                     expected_results)


if __name__ == '__main__':
  unittest.main(verbosity=2)
