#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from flake_suppressor import results


class AggregateResultsUnittest(unittest.TestCase):
  def testBasic(self):
    """Basic functionality test."""
    query_results = [
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
                'nvidia__win': {
                    'typ_tags': ['nvidia', 'win'],
                    'build_url_list': [
                        'http://ci.chromium.org/b/1111',
                        'http://ci.chromium.org/b/2222',
                    ],
                },
                'amd__win': {
                    'typ_tags': ['amd', 'win'],
                    'build_url_list': ['http://ci.chromium.org/b/3333'],
                },
            },
            'conformance/textures/misc/texture-npot-video.html': {
                'nvidia__win': {
                    'typ_tags': ['nvidia', 'win'],
                    'build_url_list': ['http://ci.chromium.org/b/4444'],
                },
            },
        },
        'pixel_integration_test': {
            'Pixel_CSS3DBlueBox': {
                'nvidia__win': {
                    'typ_tags': ['nvidia', 'win'],
                    'build_url_list': ['http://ci.chromium.org/b/5555'],
                },
            },
        },
    }
    self.assertEqual(results.AggregateResults(query_results), expected_output)


if __name__ == '__main__':
  unittest.main(verbosity=2)
