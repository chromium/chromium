# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Class for WebGL 1 conformance tests."""

import os
import sys
from typing import Any, List, Set
import unittest

from gpu_tests import gpu_integration_test
from gpu_tests import webgl_conformance_integration_test_base
from gpu_tests import common_typing as ct


class WebGL1ConformanceIntegrationTest(
    webgl_conformance_integration_test_base.WebGLConformanceIntegrationTestBase
):
  @classmethod
  def Name(cls) -> str:
    return 'webgl1_conformance'

  def _GetSerialGlobs(self) -> Set[str]:
    return super()._GetSerialGlobs() | set()

  def _GetSerialTests(self) -> Set[str]:
    return super()._GetSerialTests() | set()

  @classmethod
  def _SetClassVariablesFromOptions(cls, options: ct.ParsedCmdArgs) -> None:
    super()._SetClassVariablesFromOptions(options)
    assert cls._webgl_version == 1

  @classmethod
  def _GetExtensionList(cls) -> List[str]:
    return [
        'ANGLE_instanced_arrays',
        'EXT_blend_minmax',
        'EXT_clip_control',
        'EXT_color_buffer_half_float',
        'EXT_depth_clamp',
        'EXT_disjoint_timer_query',
        'EXT_float_blend',
        'EXT_frag_depth',
        'EXT_polygon_offset_clamp',
        'EXT_shader_texture_lod',
        'EXT_sRGB',
        'EXT_texture_compression_bptc',
        'EXT_texture_compression_rgtc',
        'EXT_texture_filter_anisotropic',
        'EXT_texture_mirror_clamp_to_edge',
        'KHR_parallel_shader_compile',
        'OES_element_index_uint',
        'OES_fbo_render_mipmap',
        'OES_standard_derivatives',
        'OES_texture_float',
        'OES_texture_float_linear',
        'OES_texture_half_float',
        'OES_texture_half_float_linear',
        'OES_vertex_array_object',
        'WEBGL_blend_func_extended',
        'WEBGL_color_buffer_float',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_etc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_depth_texture',
        'WEBGL_draw_buffers',
        'WEBGL_lose_context',
        'WEBGL_multi_draw',
        'WEBGL_polygon_mode',
    ]

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webgl_conformance_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
