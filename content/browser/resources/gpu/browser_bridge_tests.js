// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const commandLineFlags = [
  '--flag-switches-begin', '--show-composited-layer-borders',
  '--flag-switches-end'
];
const commandLineStr = './out/Debug/chrome ' + commandLineFlags.join(' ');

const glValueArray = [
  'GL_ARB_compatibility',
  'GL_ARB_copy_buffer',
  'GL_ARB_depth_buffer_float',
  'GL_ARB_depth_clamp',
  'GL_ARB_depth_texture',
  'GL_ARB_draw_buffers',
  'GL_ARB_draw_elements_base_vertex',
  'GL_ARB_draw_instanced',
  'GL_ARB_fragment_coord_conventions',
  'GL_ARB_fragment_program',
  'GL_ARB_fragment_program_shadow',
  'GL_ARB_fragment_shader',
  'GL_ARB_framebuffer_object',
  'GL_ARB_framebuffer_sRGB',
  'GL_ARB_geometry_shader4',
  'GL_ARB_half_float_pixel',
  'GL_ARB_half_float_vertex',
  'GL_ARB_imaging',
  'GL_ARB_map_buffer_range',
  'GL_ARB_multisample',
  'GL_ARB_multitexture',
  'GL_ARB_occlusion_query',
  'GL_ARB_pixel_buffer_object',
  'GL_ARB_point_parameters',
  'GL_ARB_point_sprite',
  'GL_ARB_provoking_vertex',
  'GL_ARB_seamless_cube_map',
  'GL_ARB_shader_objects',
  'GL_ARB_shading_language_100',
  'GL_ARB_shadow',
  'GL_ARB_sync',
  'GL_ARB_texture_border_clamp',
  'GL_ARB_texture_buffer_object',
  'GL_ARB_texture_compression',
  'GL_ARB_texture_compression_rgtc',
  'GL_ARB_texture_cube_map',
  'GL_ARB_texture_env_add',
  'GL_ARB_texture_env_combine',
  'GL_ARB_texture_env_crossbar',
  'GL_ARB_texture_env_dot3',
  'GL_ARB_texture_float',
  'GL_ARB_texture_mirrored_repeat',
  'GL_ARB_texture_multisample',
  'GL_ARB_texture_non_power_of_two',
  'GL_ARB_texture_rectangle',
  'GL_ARB_texture_rg',
  'GL_ARB_transpose_matrix',
  'GL_ARB_uniform_buffer_object',
  'GL_ARB_vertex_array_bgra',
  'GL_ARB_vertex_array_object',
  'GL_ARB_vertex_buffer_object',
  'GL_ARB_vertex_program',
  'GL_ARB_vertex_shader',
  'GL_ARB_window_pos',
  'GL_ATI_draw_buffers',
  'GL_ATI_texture_float',
  'GL_ATI_texture_mirror_once',
  'GL_S3_s3tc',
  'GL_EXT_texture_env_add',
  'GL_EXT_abgr',
  'GL_EXT_bgra',
  'GL_EXT_bindable_uniform',
  'GL_EXT_blend_color',
  'GL_EXT_blend_equation_separate',
  'GL_EXT_blend_func_separate',
  'GL_EXT_blend_minmax',
  'GL_EXT_blend_subtract',
  'GL_EXT_compiled_vertex_array',
  'GL_EXT_Cg_shader',
  'GL_EXT_depth_bounds_test',
  'GL_EXT_direct_state_access',
  'GL_EXT_draw_buffers2',
  'GL_EXT_draw_instanced',
  'GL_EXT_draw_range_elements',
  'GL_EXT_fog_coord',
  'GL_EXT_framebuffer_blit',
  'GL_EXT_framebuffer_multisample',
  'GL_EXTX_framebuffer_mixed_formats',
  'GL_EXT_framebuffer_object',
  'GL_EXT_framebuffer_sRGB',
  'GL_EXT_geometry_shader4',
  'GL_EXT_gpu_program_parameters',
  'GL_EXT_gpu_shader4',
  'GL_EXT_multi_draw_arrays',
  'GL_EXT_packed_depth_stencil',
  'GL_EXT_packed_float',
  'GL_EXT_packed_pixels',
  'GL_EXT_pixel_buffer_object',
  'GL_EXT_point_parameters',
  'GL_EXT_provoking_vertex',
  'GL_EXT_rescale_normal',
  'GL_EXT_secondary_color',
  'GL_EXT_separate_shader_objects',
  'GL_EXT_separate_specular_color',
  'GL_EXT_shadow_funcs',
  'GL_EXT_stencil_two_side',
  'GL_EXT_stencil_wrap',
  'GL_EXT_texture3D',
  'GL_EXT_texture_array',
  'GL_EXT_texture_buffer_object',
  'GL_EXT_texture_compression_latc',
  'GL_EXT_texture_compression_rgtc',
  'GL_EXT_texture_compression_s3tc',
  'GL_EXT_texture_cube_map',
  'GL_EXT_texture_edge_clamp',
  'GL_EXT_texture_env_combine',
  'GL_EXT_texture_env_dot3',
  'GL_EXT_texture_filter_anisotropic',
  'GL_EXT_texture_integer',
  'GL_EXT_texture_lod',
  'GL_EXT_texture_lod_bias',
  'GL_EXT_texture_mirror_clamp',
  'GL_EXT_texture_object',
  'GL_EXT_texture_shared_exponent',
  'GL_EXT_texture_sRGB',
  'GL_EXT_texture_swizzle',
  'GL_EXT_timer_query',
  'GL_EXT_vertex_array',
  'GL_EXT_vertex_array_bgra',
  'GL_IBM_rasterpos_clip',
  'GL_IBM_texture_mirrored_repeat',
  'GL_KTX_buffer_region',
  'GL_NV_blend_square',
  'GL_NV_conditional_render',
  'GL_NV_copy_depth_to_color',
  'GL_NV_copy_image',
  'GL_NV_depth_buffer_float',
  'GL_NV_depth_clamp',
  'GL_NV_explicit_multisample',
  'GL_NV_fence',
  'GL_NV_float_buffer',
  'GL_NV_fog_distance',
  'GL_NV_fragment_program',
  'GL_NV_fragment_program_option',
  'GL_NV_fragment_program2',
  'GL_NV_framebuffer_multisample_coverage',
  'GL_NV_geometry_shader4',
  'GL_NV_gpu_program4',
  'GL_NV_half_float',
  'GL_NV_light_max_exponent',
  'GL_NV_multisample_coverage',
  'GL_NV_multisample_filter_hint',
  'GL_NV_occlusion_query',
  'GL_NV_packed_depth_stencil',
  'GL_NV_parameter_buffer_object',
  'GL_NV_parameter_buffer_object2',
  'GL_NV_pixel_data_range',
  'GL_NV_point_sprite',
  'GL_NV_primitive_restart',
  'GL_NV_register_combiners',
  'GL_NV_register_combiners2',
  'GL_NV_shader_buffer_load',
  'GL_NV_texgen_reflection',
  'GL_NV_texture_barrier',
  'GL_NV_texture_compression_vtc',
  'GL_NV_texture_env_combine4',
  'GL_NV_texture_expand_normal',
  'GL_NV_texture_rectangle',
  'GL_NV_texture_shader',
  'GL_NV_texture_shader2',
  'GL_NV_texture_shader3',
  'GL_NV_transform_feedback',
  'GL_NV_vertex_array_range',
  'GL_NV_vertex_array_range2',
  'GL_NV_vertex_buffer_unified_memory',
  'GL_NV_vertex_program',
  'GL_NV_vertex_program1_1',
  'GL_NV_vertex_program2',
  'GL_NV_vertex_program2_option',
  'GL_NV_vertex_program3',
  'GL_NVX_conditional_render',
  'GL_NVX_gpu_memory_info',
  'GL_SGIS_generate_mipmap',
  'GL_SGIS_texture_lod',
  'GL_SGIX_depth_texture',
  'GL_SGIX_shadow',
  'GL_SUN_slice_accum'
];
(function() {
const dataSets = [
  {
    name: 'full_data_linux',
    gpuInfo: {
      basic_info: [
        {description: 'Initialization time', value: '111'},
        {description: 'Vendor Id', value: '0x10de'},
        {description: 'Device Id', value: '0x0658'},
        {description: 'Driver vendor', value: 'NVIDIA'},
        {description: 'Driver version', value: '195.36.24'},
        {description: 'Driver date', value: ''},
        {description: 'Pixel shader version', value: '1.50'},
        {description: 'Vertex shader version', value: '1.50'},
        {description: 'GL version', value: '3.2'},
        {description: 'GL_VENDOR', value: 'NVIDIA Corporation'},
        {description: 'GL_RENDERER', value: 'Quadro FX 380/PCI/SSE2'},
        {description: 'GL_VERSION', value: '3.2.0 NVIDIA 195.36.24'}, {
          description: 'GL_EXTENSIONS',
          value: glValueArray.join(' '),
        }
      ],
      featureStatus: {
        featureStatus: [
          {'status': 'enabled', name: '2d_canvas'},
          {'status': 'enabled', name: '3d_css'},
          {'status': 'enabled', name: 'compositing'},
          {'status': 'enabled', name: 'webgl'},
          {'status': 'enabled', name: 'multisampling'}
        ],
        problems: []
      }
    },
    clientInfo: {
      command_line: commandLineStr,
      version: 'Chrome/12.0.729.0',
    },
    logMessages: []
  },
  {
    name: 'no_data',
    gpuInfo: undefined,
    clientInfo: undefined,
    logMessages: undefined
  },
  {
    name: 'logs',
    gpuInfo: undefined,
    clientInfo: undefined,
    logMessages: [{header: 'foo', message: 'Bar'}]
  },

  // tests for 'status'
  {
    name: 'feature_states',
    gpuInfo: {
      basic_info: undefined,
      featureStatus: {
        featureStatus: [
          {'status': 'disabled_off', name: '2d_canvas'},
          {'status': 'unavailable_software', name: '3d_css'},
          {'status': 'disabled_software', name: 'compositing'},
          {'status': 'software', name: 'compositing'},
          {'status': 'unavailable_off', name: 'webgl'},
          {'status': 'enabled', name: 'multisampling'}
        ],
        problems: [
          {description: 'Something wrong', crBugs: [], webkitBugs: []},
          {description: 'SomethingElse', crBugs: [], webkitBugs: []}, {
            description: 'WebKit and Chrome bug',
            crBugs: [23456],
            webkitBugs: [789, 2123]
          }
        ]
      }
    },
    clientInfo: undefined,
    logMessages: []
  }

];

const selectEl = document.createElement('select');
for (let i = 0; i < dataSets.length; ++i) {
  const optionEl = document.createElement('option');
  optionEl.textContent = dataSets[i].name;
  optionEl.dataSet = dataSets[i];
  selectEl.add(optionEl);
}
selectEl.addEventListener('change', function() {
  browserBridge.applySimulatedData_(dataSets[selectEl.selectedIndex]);
});
selectEl.addEventListener('keydown', function() {
  window.setTimeout(function() {
    browserBridge.applySimulatedData_(dataSets[selectEl.selectedIndex]);
  }, 0);
});

const controlEl = document.createElement('div');
const textEl = document.createElement('span');
textEl.textContent = 'GPU Info:';
controlEl.appendChild(textEl);
controlEl.appendChild(selectEl);

document.querySelector('#debug-div')
    .appendChild(controlEl, document.body.firstChild);

browserBridge.applySimulatedData_(dataSets[0]);
})();
