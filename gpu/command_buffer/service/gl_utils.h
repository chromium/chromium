// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file includes all the necessary GL headers and implements some useful
// utilities.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_bindings.h"

// Define this for extra GL error debugging (slower).
// #define GL_ERROR_DEBUGGING
#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR()                                                \
  do {                                                                  \
    GLenum gl_error = glGetError();                                     \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_error; \
  } while (0)
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#endif  // GL_ERROR_DEBUGGING

namespace gl {
struct GLVersionInfo;
}

namespace gpu {

struct Capabilities;

namespace gles2 {

class ErrorState;
class FeatureInfo;
class Logger;
class Texture;
enum class CopyTextureMethod;

// clang-format off
constexpr GLfloat kIdentityMatrix[16] = { 1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, 1.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 1.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, 1.0f };
// clang-format on

struct CALayerSharedState {
  float opacity;
  bool is_clipped;
  gfx::Rect clip_rect;
  gfx::RRectF rounded_corner_bounds;
  int sorting_context_id;
  gfx::Transform transform;
};

std::vector<int> GetAllGLErrors();

bool PrecisionMeetsSpecForHighpFloat(GLint rangeMin,
                                     GLint rangeMax,
                                     GLint precision);
void QueryShaderPrecisionFormat(const gl::GLVersionInfo& gl_version_info,
                                GLenum shader_type,
                                GLenum precision_type,
                                GLint* range,
                                GLint* precision);

// Using the provided feature info, query the numeric limits of the underlying
// GL and fill in the members of the Capabilities struct.  Does not perform any
// extension checks.
void PopulateNumericCapabilities(Capabilities* caps,
                                 const FeatureInfo* feature_info);

bool CheckUniqueAndNonNullIds(GLsizei n, const GLuint* client_ids);

const char* GetServiceVersionString(const FeatureInfo* feature_info);
const char* GetServiceShadingLanguageVersionString(
    const FeatureInfo* feature_info);

void LogGLDebugMessage(GLenum source,
                       GLenum type,
                       GLuint id,
                       GLenum severity,
                       GLsizei length,
                       const GLchar* message,
                       Logger* error_logger);
void InitializeGLDebugLogging(bool log_non_errors,
                              GLDEBUGPROC callback,
                              const void* user_param);

bool ValidContextLostReason(GLenum reason);
error::ContextLostReason GetContextLostReasonFromResetStatus(
    GLenum reset_status);

bool GetCompressedTexSizeInBytes(const char* function_name,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLsizei* size_in_bytes,
                                 ErrorState* error_state);

bool ValidateCompressedTexSubDimensions(GLenum target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLint zoffset,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLenum format,
                                        Texture* texture,
                                        const char** error_message);

bool ValidateCompressedTexDimensions(GLenum target,
                                     GLint level,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     const char** error_message);

bool ValidateCopyTexFormatHelper(const FeatureInfo* feature_info,
                                 GLenum internal_format,
                                 GLenum read_format,
                                 GLenum read_type,
                                 std::string* output_error_msg);

CopyTextureMethod GetCopyTextureCHROMIUMMethod(const FeatureInfo* feature_info,
                                               GLenum source_target,
                                               GLint source_level,
                                               GLenum source_internal_format,
                                               GLenum source_type,
                                               GLenum dest_target,
                                               GLint dest_level,
                                               GLenum dest_internal_format,
                                               bool flip_y,
                                               bool premultiply_alpha,
                                               bool unpremultiply_alpha,
                                               bool dither);

bool ValidateCopyTextureCHROMIUMInternalFormats(const FeatureInfo* feature_info,
                                                GLenum source_internal_format,
                                                GLenum dest_internal_format,
                                                std::string* output_error_msg);

GLenum GetTextureBindingQuery(GLenum texture_type);

gfx::OverlayTransform GetGFXOverlayTransform(GLenum plane_transform);

bool GetGFXBufferFormat(GLenum internal_format, gfx::BufferFormat* out_format);
bool GetGFXBufferUsage(GLenum buffer_usage, gfx::BufferUsage* out_usage);

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GL_UTILS_H_
