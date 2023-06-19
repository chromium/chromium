// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_AUTOGEN_H_

ValueValidator<GLenum> g_l_state;
class QueryObjectParameterValidator {
 public:
  bool IsValid(const GLenum value) const;
};
QueryObjectParameterValidator query_object_parameter;

class QueryTargetValidator {
 public:
  bool IsValid(const GLenum value) const;
};
QueryTargetValidator query_target;

class ResetStatusValidator {
 public:
  bool IsValid(const GLenum value) const;
};
ResetStatusValidator reset_status;

ValueValidator<GLenum> texture_mag_filter_mode;
ValueValidator<GLenum> texture_min_filter_mode;
ValueValidator<GLenum> texture_parameter;
ValueValidator<GLenum> texture_wrap_mode;
ValueValidator<gfx::BufferUsage> gfx_buffer_usage;
class GpuRasterMsaaModeValidator {
 public:
  bool IsValid(const gpu::raster::MsaaMode value) const;
};
GpuRasterMsaaModeValidator gpu_raster_msaa_mode;

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_CMD_VALIDATION_AUTOGEN_H_
