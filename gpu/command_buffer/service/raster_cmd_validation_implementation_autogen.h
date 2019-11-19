// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_

static const GLenum valid_g_l_state_table[] = {
    GL_ACTIVE_TEXTURE,
};

bool Validators::QueryObjectParameterValidator::IsValid(
    const GLenum value) const {
  switch (value) {
    case GL_QUERY_RESULT_EXT:
    case GL_QUERY_RESULT_AVAILABLE_EXT:
    case GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT:
      return true;
  }
  return false;
}

bool Validators::QueryTargetValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      return true;
  }
  return false;
}

bool Validators::ResetStatusValidator::IsValid(const GLenum value) const {
  switch (value) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
    case GL_INNOCENT_CONTEXT_RESET_ARB:
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return true;
  }
  return false;
}

static const GLenum valid_texture_mag_filter_mode_table[] = {
    GL_NEAREST,
};

static const GLenum valid_texture_min_filter_mode_table[] = {
    GL_NEAREST,
};

static const GLenum valid_texture_parameter_table[] = {
    GL_TEXTURE_MAG_FILTER,
    GL_TEXTURE_MIN_FILTER,
    GL_TEXTURE_WRAP_S,
    GL_TEXTURE_WRAP_T,
};

static const GLenum valid_texture_wrap_mode_table[] = {
    GL_CLAMP_TO_EDGE,
};

static const gfx::BufferUsage valid_gfx_buffer_usage_table[] = {
    gfx::BufferUsage::GPU_READ,
    gfx::BufferUsage::SCANOUT,
    gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
};

static const viz::ResourceFormat valid_viz_resource_format_table[] = {
    viz::ResourceFormat::RGBA_8888,    viz::ResourceFormat::RGBA_4444,
    viz::ResourceFormat::BGRA_8888,    viz::ResourceFormat::ALPHA_8,
    viz::ResourceFormat::LUMINANCE_8,  viz::ResourceFormat::RGB_565,
    viz::ResourceFormat::BGR_565,      viz::ResourceFormat::RED_8,
    viz::ResourceFormat::RG_88,        viz::ResourceFormat::LUMINANCE_F16,
    viz::ResourceFormat::RGBA_F16,     viz::ResourceFormat::R16_EXT,
    viz::ResourceFormat::RGBX_8888,    viz::ResourceFormat::BGRX_8888,
    viz::ResourceFormat::RGBX_1010102, viz::ResourceFormat::BGRX_1010102,
    viz::ResourceFormat::YVU_420,      viz::ResourceFormat::YUV_420_BIPLANAR,
};

Validators::Validators()
    : g_l_state(valid_g_l_state_table, base::size(valid_g_l_state_table)),
      texture_mag_filter_mode(valid_texture_mag_filter_mode_table,
                              base::size(valid_texture_mag_filter_mode_table)),
      texture_min_filter_mode(valid_texture_min_filter_mode_table,
                              base::size(valid_texture_min_filter_mode_table)),
      texture_parameter(valid_texture_parameter_table,
                        base::size(valid_texture_parameter_table)),
      texture_wrap_mode(valid_texture_wrap_mode_table,
                        base::size(valid_texture_wrap_mode_table)),
      gfx_buffer_usage(valid_gfx_buffer_usage_table,
                       base::size(valid_gfx_buffer_usage_table)),
      viz_resource_format(valid_viz_resource_format_table,
                          base::size(valid_viz_resource_format_table)) {}

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
