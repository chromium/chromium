#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=protected-access

import unittest
import unittest.mock as mock

from flake_suppressor import data_types
from flake_suppressor import results


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


class BaseResultsUnittest(unittest.TestCase):
  def setUp(self):
    self._local_patcher = mock.patch('flake_suppressor.results.expectations.'
                                     'GetExpectationFilesFromLocalCheckout')
    self._local_mock = self._local_patcher.start()
    self._local_mock.return_value = {}
    self.addCleanup(self._local_patcher.stop)


class AggregateResultsUnittest(BaseResultsUnittest):
  def testBasic(self):
    """Basic functionality test."""
    query_results = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            # The win-laptop tag is ignored, and thus should be removed in the
            # output.
            'typ_tags': ['win', 'nvidia', 'win-laptop'],
        },
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
                ('nvidia', 'win'): [
                    'http://ci.chromium.org/b/1111',
                    'http://ci.chromium.org/b/2222',
                ],
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
    self.assertEqual(results.AggregateResults(query_results), expected_output)

  def testWithFiltering(self):
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

    self.assertEqual(results.AggregateResults(query_results), expected_output)


class ConvertJsonResultsToResultObjectsUnittest(BaseResultsUnittest):
  def testBasic(self):
    """Basic functionality test."""
    r = [
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-1111',
            # The win-laptop tag is ignored, and thus should be removed in the
            # output.
            'typ_tags': ['win', 'nvidia', 'win-laptop'],
        },
        {
            'name': ('gpu_tests.webgl_conformance_integration_test.'
                     'WebGLConformanceIntegrationTest.'
                     'conformance/textures/misc/video-rotation.html'),
            'id':
            'build-2222',
            'typ_tags': ['nvidia', 'win'],
        },
    ]
    expected_results = [
        data_types.Result('webgl_conformance_integration_test',
                          'conformance/textures/misc/video-rotation.html',
                          ('nvidia', 'win'), '1111'),
        data_types.Result(
            'webgl_conformance_integration_test',
            'conformance/textures/misc/video-rotation.html',
            ('nvidia', 'win'),
            '2222',
        ),
    ]
    self.assertEqual(results._ConvertJsonResultsToResultObjects(r),
                     expected_results)

  def testDuplicateResults(self):
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
    self.assertEqual(results._ConvertJsonResultsToResultObjects(r),
                     expected_results)


class FilterOutSuppressedResultsUnittest(BaseResultsUnittest):
  def testNoSuppressedResults(self):
    """Tests functionality when no expectations apply to the given results."""
    self._local_mock.return_value = {
        'foo_expectations.txt': GENERIC_EXPECTATION_FILE_CONTENTS,
    }

    r = [
        data_types.Result('foo_integration_test', 'foo_test', tuple(['linux']),
                          'id'),
        data_types.Result('foo_integration_test', 'bar_test', tuple(['win']),
                          'id'),
        data_types.Result('bar_integration_test', 'foo_test', tuple(['win']),
                          'id')
    ]
    self.assertEqual(results._FilterOutSuppressedResults(r), r)

  def testSuppressedResults(self):
    """Tests functionality when expectations apply to the given results."""
    self._local_mock.return_value = {
        'foo_expectations.txt': GENERIC_EXPECTATION_FILE_CONTENTS,
    }

    r = [
        data_types.Result('foo_integration_test', 'foo_test', ('win', 'nvidia'),
                          'id'),
        data_types.Result('foo_integration_test', 'foo_test', tuple(['win']),
                          'id'),
        data_types.Result('foo_integration_test', 'bar_test', tuple(['win']),
                          'id'),
    ]

    expected_filtered_results = [
        data_types.Result('foo_integration_test', 'bar_test', tuple(['win']),
                          'id'),
    ]

    self.assertEqual(results._FilterOutSuppressedResults(r),
                     expected_filtered_results)


if __name__ == '__main__':
  unittest.main(verbosity=2)
