# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for WebGL 2 conformance tests."""

import os
import sys
from typing import Any, List, Set
import unittest

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import webgl_conformance_integration_test_base


class WebGL2ConformanceIntegrationTest(
    webgl_conformance_integration_test_base.WebGLConformanceIntegrationTestBase
):
  @classmethod
  def Name(cls) -> str:
    return 'webgl2_conformance'

  def _GetSerialGlobs(self) -> Set[str]:
    return super()._GetSerialGlobs() | set()

  def _GetSerialTests(self) -> Set[str]:
    return super()._GetSerialTests() | set()

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    super()._SetClassVariablesFromOptions(options)
    assert cls._webgl_version == 2

  @classmethod
  def _GetExtensionList(cls) -> List[str]:
    return [
        'EXT_color_buffer_float',
        'EXT_color_buffer_half_float',
        'EXT_disjoint_timer_query_webgl2',
        'EXT_float_blend',
        'EXT_texture_compression_bptc',
        'EXT_texture_compression_rgtc',
        'EXT_texture_filter_anisotropic',
        'EXT_texture_norm16',
        'KHR_parallel_shader_compile',
        'OES_draw_buffers_indexed',
        'OES_texture_float_linear',
        'OVR_multiview2',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_etc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_draw_instanced_base_vertex_base_instance',
        'WEBGL_lose_context',
        'WEBGL_multi_draw',
        'WEBGL_multi_draw_instanced_base_vertex_base_instance',
        'WEBGL_provoking_vertex',
        'WEBGL_video_texture',
        'WEBGL_webcodecs_video_frame',
    ]

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webgl2_conformance_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
