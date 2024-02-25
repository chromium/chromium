# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This dictionary of GPU information was reformatted from the output
# of the webgl_conformance tests run on the win_chromium_rel_ng trybot
# (the step run on Windows with AMD GPU). It helps test
# telemetry.internal.platform's GPUInfo class, and specifically the
# attributes it expects to find in the dictionary; if the code changes
# in an incompatible way, tests using this fake GPU info will begin
# failing, indicating this fake data must be updated.
#
# To regenerate this less manually, import pdb in
# telemetry/internal/platform/gpu_info.py and add a call to
# pdb.set_trace() in GPUInfo.FromDict before the return statement.
# Print the attrs dictionary in the debugger and copy/paste the result
# on the right-hand side of this assignment. Then run:
#
#   pyformat [this file name] | sed -e "s/'/'/g"
#
# and put the output into this file.

from __future__ import print_function

# yapf: disable
FAKE_GPU_INFO = {
    'feature_status':
        {
            '2d_canvas': 'enabled',
            'flash_3d': 'enabled',
            'flash_stage3d': 'enabled',
            'flash_stage3d_baseline': 'enabled',
            'gpu_compositing': 'enabled',
            'multiple_raster_threads': 'enabled_on',
            'rasterization': 'disabled_software',
            'video_decode': 'enabled',
            'video_encode': 'enabled',
            'webgl': 'enabled'
        },
    'aux_attributes':
        {
            'amd_switchable': False,
            'basic_info_state': 1,
            'context_info_state': 1,
            'direct_rendering': True,
            'driver_date': '11-20-2014',
            'driver_vendor': 'Advanced Micro Devices, Inc.',
            'driver_version': '14.501.1003.0',
            'dx_diagnostics_info_state': 0,
            'gl_extensions':
                'GL_OES_element_index_uint GL_OES_packed_depth_stencil '
                'GL_OES_get_program_binary GL_OES_rgb8_rgba8 '
                'GL_EXT_texture_format_BGRA8888 GL_EXT_read_format_bgra '
                'GL_NV_pixel_buffer_object GL_OES_mapbuffer '
                'GL_EXT_map_buffer_range GL_OES_texture_half_float '
                'GL_OES_texture_half_float_linear GL_OES_texture_float '
                'GL_OES_texture_float_linear GL_EXT_texture_rg '
                'GL_ANGLE_texture_compression_dxt1 '
                'GL_ANGLE_texture_compression_dxt3 '
                'GL_ANGLE_texture_compression_dxt5 GL_EXT_sRGB '
                'GL_ANGLE_depth_texture GL_EXT_texture_storage '
                'GL_OES_texture_npot GL_EXT_draw_buffers '
                'GL_EXT_texture_filter_anisotropic '
                'GL_EXT_occlusion_query_boolean GL_NV_fence GL_EXT_robustness '
                'GL_EXT_blend_minmax GL_ANGLE_framebuffer_blit '
                'GL_ANGLE_framebuffer_multisample GL_ANGLE_instanced_arrays '
                'GL_ANGLE_pack_reverse_row_order GL_OES_standard_derivatives '
                'GL_EXT_shader_texture_lod GL_EXT_frag_depth '
                'GL_ANGLE_texture_usage GL_ANGLE_translated_shader_source '
                'GL_EXT_debug_marker GL_OES_EGL_image',
            'gl_renderer':
                'ANGLE (AMD Radeon HD 6450 Direct3D11 vs_5_0 ps_5_0)',
            'gl_reset_notification_strategy': 33362,
            'gl_vendor': 'Google Inc.',
            'gl_version': 'OpenGL ES 2.0 (ANGLE 2.1.0.c5b2ba53591c)',
            'gl_ws_extensions':
                'EGL_EXT_create_context_robustness '
                'EGL_ANGLE_d3d_share_handle_client_buffer '
                'EGL_ANGLE_surface_d3d_texture_2d_share_handle '
                'EGL_ANGLE_query_surface_pointer EGL_ANGLE_window_fixed_size '
                'EGL_NV_post_sub_buffer EGL_KHR_create_context '
                'EGL_EXT_device_query EGL_KHR_image EGL_KHR_image_base '
                'EGL_KHR_gl_texture_2D_image EGL_KHR_gl_texture_cubemap_image '
                'EGL_KHR_gl_renderbuffer_image EGL_KHR_get_all_proc_addresses',
            'gl_ws_vendor': 'Google Inc. (adapter LUID: 0000000000007924)',
            'gl_ws_version': '1.4 (ANGLE 2.1.0.c5b2ba53591c)',
            'in_process_gpu': False,
            'initialization_time': 2.503214,
            'jpeg_decode_accelerator_supported': False,
            'max_msaa_samples': '4',
            'max_resolution_height': 1088,
            'max_resolution_width': 1920,
            'min_resolution_height': 48,
            'min_resolution_width': 48,
            'optimus': False,
            'pixel_shader_version': '5.0',
            'profile': 12,
            'sandboxed': True,
            'software_rendering': False,
            'vertex_shader_version': '5.0'
        },
    'devices':
        [
            {
                'device_string': '',
                'vendor_id': 4098.0,
                'device_id': 26489.0,
                'vendor_string': ''
            },
            {
                'device_string': '',
                'vendor_id': 4139.0,
                'device_id': 1332.0,
                'vendor_string': ''
            },
        ],
    'driver_bug_workarounds':
        [
            'exit_on_context_lost',
            'force_cube_complete',
            'scalarize_vec_and_mat_constructor_args',
        ]
}
# yapf: enable
