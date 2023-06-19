// Copyright 2018 The Chromium Authors
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

bool Validators::GpuRasterMsaaModeValidator::IsValid(
    const gpu::raster::MsaaMode value) const {
  switch (value) {
    case gpu::raster::MsaaMode::kNoMSAA:
    case gpu::raster::MsaaMode::kMSAA:
    case gpu::raster::MsaaMode::kDMSAA:
      return true;
  }
  return false;
}

Validators::Validators()
    : g_l_state(valid_g_l_state_table, std::size(valid_g_l_state_table)),
      texture_mag_filter_mode(valid_texture_mag_filter_mode_table,
                              std::size(valid_texture_mag_filter_mode_table)),
      texture_min_filter_mode(valid_texture_min_filter_mode_table,
                              std::size(valid_texture_min_filter_mode_table)),
      texture_parameter(valid_texture_parameter_table,
                        std::size(valid_texture_parameter_table)),
      texture_wrap_mode(valid_texture_wrap_mode_table,
                        std::size(valid_texture_wrap_mode_table)),
      gfx_buffer_usage(valid_gfx_buffer_usage_table,
                       std::size(valid_gfx_buffer_usage_table)) {}

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_IMPLEMENTATION_AUTOGEN_H_
