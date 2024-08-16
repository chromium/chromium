// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gl_utils.h"

#include <algorithm>
#include <unordered_set>

#include "build/build_config.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/service/error_state.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_copy_texture_chromium.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <sys/stat.h>
#include "ui/gl/gl_surface_egl.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

const int kASTCBlockSize = 16;
const int kS3TCBlockWidth = 4;
const int kS3TCBlockHeight = 4;
const int kS3TCDXT1BlockSize = 8;
const int kS3TCDXT3AndDXT5BlockSize = 16;
const int kEACAndETC2BlockSize = 4;
const int kBPTCBlockWidth = 4;
const int kBPTCBlockHeight = 4;
const int kRGTCBlockWidth = 4;
const int kRGTCBlockHeight = 4;

typedef struct {
  int blockWidth;
  int blockHeight;
} ASTCBlockArray;

const ASTCBlockArray kASTCBlockArray[] = {
    {4, 4}, /* GL_COMPRESSED_RGBA_ASTC_4x4_KHR */
    {5, 4}, /* and GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR */
    {5, 5},  {6, 5},  {6, 6},  {8, 5},   {8, 6},   {8, 8},
    {10, 5}, {10, 6}, {10, 8}, {10, 10}, {12, 10}, {12, 12}};

bool IsValidPVRTCSize(GLint level, GLsizei size) {
  return GLES2Util::IsPOT(size);
}

bool IsValidS3TCSizeForWebGLAndANGLE(GLint level, GLsizei size) {
  // WebGL and ANGLE only allow multiple-of-4 sizes for the base level. See
  // WEBGL_compressed_texture_s3tc and ANGLE_compressed_texture_dxt*
  return (level > 0) || (size % kS3TCBlockWidth == 0);
}

const char* GetDebugSourceString(GLenum source) {
  switch (source) {
    case GL_DEBUG_SOURCE_API:
      return "OpenGL";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      return "Window System";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      return "Shader Compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      return "Third Party";
    case GL_DEBUG_SOURCE_APPLICATION:
      return "Application";
    case GL_DEBUG_SOURCE_OTHER:
      return "Other";
    default:
      return "UNKNOWN";
  }
}

const char* GetDebugTypeString(GLenum type) {
  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      return "Deprecated behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      return "Undefined behavior";
    case GL_DEBUG_TYPE_PORTABILITY:
      return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE:
      return "Performance";
    case GL_DEBUG_TYPE_OTHER:
      return "Other";
    case GL_DEBUG_TYPE_MARKER:
      return "Marker";
    default:
      return "UNKNOWN";
  }
}

const char* GetDebugSeverityString(GLenum severity) {
  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      return "High";
    case GL_DEBUG_SEVERITY_MEDIUM:
      return "Medium";
    case GL_DEBUG_SEVERITY_LOW:
      return "Low";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      return "Notification";
    default:
      return "UNKNOWN";
  }
}
}  // namespace

bool PrecisionMeetsSpecForHighpFloat(GLint rangeMin,
                                     GLint rangeMax,
                                     GLint precision) {
  return (rangeMin >= 62) && (rangeMax >= 62) && (precision >= 16);
}

void QueryShaderPrecisionFormat(GLenum shader_type,
                                GLenum precision_type,
                                GLint* range,
                                GLint* precision) {
  switch (precision_type) {
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      // These values are for a 32-bit twos-complement integer format.
      range[0] = 31;
      range[1] = 30;
      *precision = 0;
      break;
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
      // These values are for an IEEE single-precision floating-point format.
      range[0] = 127;
      range[1] = 127;
      *precision = 23;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  // This function is sometimes defined even though it's really just
  // a stub, so we need to set range and precision as if it weren't
  // defined before calling it.
  // On Mac OS with some GPUs, calling this generates a
  // GL_INVALID_OPERATION error. Avoid calling it on non-GLES2
  // platforms.
  glGetShaderPrecisionFormat(shader_type, precision_type, range, precision);

  // TODO(brianderson): Make the following official workarounds.

  // Some drivers have bugs where they report the ranges as a negative number.
  // Taking the absolute value here shouldn't hurt because negative numbers
  // aren't expected anyway.
  range[0] = abs(range[0]);
  range[1] = abs(range[1]);

  // If the driver reports a precision for highp float that isn't actually
  // highp, don't pretend like it's supported because shader compilation will
  // fail anyway.
  if (precision_type == GL_HIGH_FLOAT &&
      !PrecisionMeetsSpecForHighpFloat(range[0], range[1], *precision)) {
    range[0] = 0;
    range[1] = 0;
    *precision = 0;
  }
}

void PopulateNumericCapabilities(Capabilities* caps,
                                 const FeatureInfo* feature_info) {
  DCHECK(caps != nullptr);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps->max_texture_size);
}

void PopulateGLCapabilities(GLCapabilities* caps,
                            const FeatureInfo* feature_info) {
  CHECK(caps);

  caps->VisitPrecisions([](GLenum shader, GLenum type,
                           GLCapabilities::ShaderPrecision* shader_precision) {
    GLint range[2] = {0, 0};
    GLint precision = 0;
    QueryShaderPrecisionFormat(shader, type, range, &precision);
    shader_precision->min_range = range[0];
    shader_precision->max_range = range[1];
    shader_precision->precision = precision;
  });

  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps->max_texture_size);

  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                &caps->max_combined_texture_image_units);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &caps->max_cube_map_texture_size);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                &caps->max_fragment_uniform_vectors);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &caps->max_renderbuffer_size);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &caps->max_texture_image_units);
  glGetIntegerv(GL_MAX_VARYING_VECTORS, &caps->max_varying_vectors);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps->max_vertex_attribs);
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                &caps->max_vertex_texture_image_units);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS,
                &caps->max_vertex_uniform_vectors);
  {
    GLint dims[2] = {0, 0};
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
    caps->max_viewport_width = dims[0];
    caps->max_viewport_height = dims[1];
  }
  glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                &caps->num_compressed_texture_formats);
  glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &caps->num_shader_binary_formats);
  glGetIntegerv(GL_BIND_GENERATES_RESOURCE_CHROMIUM,
                &caps->bind_generates_resource_chromium);
  if (feature_info->IsWebGL2OrES3OrHigherContext()) {
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &caps->max_3d_texture_size);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &caps->max_array_texture_layers);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &caps->max_color_attachments);
    glGetInteger64v(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
                    &caps->max_combined_fragment_uniform_components);
    glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,
                  &caps->max_combined_uniform_blocks);
    glGetInteger64v(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
                    &caps->max_combined_vertex_uniform_components);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &caps->max_draw_buffers);
    glGetInteger64v(GL_MAX_ELEMENT_INDEX, &caps->max_element_index);
    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &caps->max_elements_indices);
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &caps->max_elements_vertices);
    glGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                  &caps->max_fragment_input_components);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
                  &caps->max_fragment_uniform_blocks);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
                  &caps->max_fragment_uniform_components);
    glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &caps->max_program_texel_offset);
    glGetInteger64v(GL_MAX_SERVER_WAIT_TIMEOUT, &caps->max_server_wait_timeout);
    glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps->max_texture_lod_bias);
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                  &caps->max_transform_feedback_interleaved_components);
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                  &caps->max_transform_feedback_separate_attribs);
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                  &caps->max_transform_feedback_separate_components);
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &caps->max_uniform_block_size);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                  &caps->max_uniform_buffer_bindings);
    glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &caps->max_varying_components);
    glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                  &caps->max_vertex_output_components);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS,
                  &caps->max_vertex_uniform_blocks);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                  &caps->max_vertex_uniform_components);
    glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &caps->min_program_texel_offset);
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,
                  &caps->num_program_binary_formats);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                  &caps->uniform_buffer_offset_alignment);
    if (feature_info->IsES31ForTestingContext()) {
      glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS,
                    &caps->max_atomic_counter_buffer_bindings);
      glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS,
                    &caps->max_shader_storage_buffer_bindings);
      glGetIntegerv(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT,
                    &caps->shader_storage_buffer_offset_alignment);
    }
  }
  if (feature_info->feature_flags().multisampled_render_to_texture ||
      feature_info->feature_flags().chromium_framebuffer_multisample ||
      feature_info->IsWebGL2OrES3OrHigherContext()) {
    glGetIntegerv(GL_MAX_SAMPLES, &caps->max_samples);
  }

  if (feature_info->IsWebGL2OrES3OrHigherContext()) {
    caps->major_version = 3;
    if (feature_info->IsES31ForTestingContext()) {
      caps->minor_version = 1;
    } else {
      caps->minor_version = 0;
    }
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PopulateDRMCapabilities(Capabilities* caps,
                             const FeatureInfo* feature_info) {
  DCHECK(caps != nullptr);

  if (!gl::GLSurfaceEGL::GetGLDisplayEGL() ||
      !gl::GLSurfaceEGL::GetGLDisplayEGL()->IsInitialized() ||
      !gl::GLSurfaceEGL::GetGLDisplayEGL()
           ->ext->b_EGL_EXT_image_dma_buf_import_modifiers ||
      feature_info->workarounds()
          .disable_egl_ext_image_dma_buf_import_modifiers ||
      !gl::g_driver_egl.client_ext.b_EGL_EXT_device_query) {
    return;
  }

  EGLDisplay egl_display = gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay();
  DCHECK(egl_display != nullptr);

  EGLDeviceEXT egl_device;
  if (!eglQueryDisplayAttribEXT(egl_display, EGL_DEVICE_EXT,
                                (EGLAttrib*)&egl_device)) {
    return;
  }

  gfx::ExtensionSet device_extension_set;
  const char* device_extensions =
      eglQueryDeviceStringEXT(egl_device, EGL_EXTENSIONS);
  if (device_extensions) {
    device_extension_set = gfx::MakeExtensionSet(device_extensions);
  } else {
    device_extension_set = gfx::ExtensionSet();
  }

  std::string drm_render_node;
  if (gfx::HasExtension(device_extension_set,
                        "EGL_EXT_device_drm_render_node")) {
    const char* path =
        eglQueryDeviceStringEXT(egl_device, EGL_DRM_RENDER_NODE_FILE_EXT);
    if (path)
      drm_render_node = std::string(path);
  }
  if (drm_render_node.empty() &&
      gfx::HasExtension(device_extension_set, "EGL_EXT_device_drm")) {
    const char* path =
        eglQueryDeviceStringEXT(egl_device, EGL_DRM_DEVICE_FILE_EXT);
    if (path)
      drm_render_node = std::string(path);
  }

  if (!drm_render_node.empty()) {
    struct stat dev_stat;
    if (stat(drm_render_node.c_str(), &dev_stat) == 0) {
      static_assert(sizeof(dev_t) <= sizeof(caps->drm_device_id),
                    "unexpected dev_t size");
      DCHECK(dev_stat.st_rdev);
      caps->drm_device_id = dev_stat.st_rdev;
    }
  }

  EGLint num_formats = 0;
  if (eglQueryDmaBufFormatsEXT(egl_display, 0, nullptr, &num_formats) &&
      num_formats > 0) {
    std::vector<EGLint> formats_array(num_formats);
    bool res = eglQueryDmaBufFormatsEXT(egl_display, num_formats,
                                        formats_array.data(), &num_formats);
    DCHECK(res);

    for (EGLint format : formats_array) {
      std::vector<uint64_t> modifiers;
      EGLint num_modifiers = 0;
      if (eglQueryDmaBufModifiersEXT(egl_display, format, 0, nullptr, nullptr,
                                     &num_modifiers) &&
          num_modifiers > 0) {
        std::vector<EGLuint64KHR> modifiers_array(num_modifiers);
        res = eglQueryDmaBufModifiersEXT(egl_display, format, num_modifiers,
                                         modifiers_array.data(), nullptr,
                                         &num_modifiers);
        DCHECK(res);

        for (uint64_t modifier : modifiers_array) {
          modifiers.push_back(modifier);
        }
      }

      caps->drm_formats_and_modifiers.emplace(format, modifiers);
    }
  }
}
#endif

bool CheckUniqueAndNonNullIds(GLsizei n, const GLuint* client_ids) {
  if (n <= 0)
    return true;
  std::unordered_set<uint32_t> unique_ids(client_ids, client_ids + n);
  return (unique_ids.size() == static_cast<size_t>(n)) &&
         (unique_ids.find(0) == unique_ids.end());
}

const char* GetServiceVersionString(const FeatureInfo* feature_info) {
  if (feature_info->IsWebGL2OrES3Context())
    return "OpenGL ES 3.0 Chromium";
  else if (feature_info->IsES31ForTestingContext()) {
    return "OpenGL ES 3.1 Chromium";
  } else
    return "OpenGL ES 2.0 Chromium";
}

const char* GetServiceShadingLanguageVersionString(
    const FeatureInfo* feature_info) {
  if (feature_info->IsWebGL2OrES3Context())
    return "OpenGL ES GLSL ES 3.0 Chromium";
  else if (feature_info->IsES31ForTestingContext()) {
    return "OpenGL ES GLSL ES 3.1 Chromium";
  } else
    return "OpenGL ES GLSL ES 1.0 Chromium";
}

void LogGLDebugMessage(GLenum source,
                       GLenum type,
                       GLuint id,
                       GLenum severity,
                       GLsizei length,
                       const GLchar* message,
                       Logger* error_logger) {
  std::string id_string = GLES2Util::GetStringEnum(id);
  if (type == GL_DEBUG_TYPE_ERROR && source == GL_DEBUG_SOURCE_API) {
    error_logger->LogMessage(__FILE__, __LINE__,
                             " " + id_string + ": " + message);
  } else {
    error_logger->LogMessage(
        __FILE__, __LINE__,
        std::string("GL Driver Message (") + GetDebugSourceString(source) +
            ", " + GetDebugTypeString(type) + ", " + id_string + ", " +
            GetDebugSeverityString(severity) + "): " + message);
  }
}

void InitializeGLDebugLogging(bool log_non_errors,
                              GLDEBUGPROC callback,
                              const void* user_param) {
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

  glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE,
                        0, nullptr, GL_TRUE);

  if (log_non_errors) {
    // Enable logging of medium and high severity messages
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0,
                          nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM,
                          0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0,
                          nullptr, GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
  }

  glDebugMessageCallback(callback, user_param);
}

bool ValidContextLostReason(GLenum reason) {
  switch (reason) {
    case GL_NO_ERROR:
    case GL_GUILTY_CONTEXT_RESET_ARB:
    case GL_INNOCENT_CONTEXT_RESET_ARB:
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return true;
    default:
      return false;
  }
}

error::ContextLostReason GetContextLostReasonFromResetStatus(
    GLenum reset_status) {
  switch (reset_status) {
    case GL_NO_ERROR:
      // TODO(kbr): improve the precision of the error code in this case.
      // Consider delegating to context for error code if MakeCurrent fails.
      return error::kUnknown;
    case GL_GUILTY_CONTEXT_RESET_ARB:
      return error::kGuilty;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      return error::kInnocent;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return error::kUnknown;
  }

  NOTREACHED_IN_MIGRATION();
  return error::kUnknown;
}

bool GetCompressedTexSizeInBytes(const char* function_name,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLsizei* size_in_bytes,
                                 ErrorState* error_state) {
  base::CheckedNumeric<GLsizei> bytes_required(0);

  switch (format) {
    case GL_ATC_RGB_AMD:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_ETC1_RGB8_OES:
      bytes_required = (width + kS3TCBlockWidth - 1) / kS3TCBlockWidth;
      bytes_required *= (height + kS3TCBlockHeight - 1) / kS3TCBlockHeight;
      bytes_required *= kS3TCDXT1BlockSize;
      break;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: {
      const int index =
          (format < GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
              ? static_cast<int>(format - GL_COMPRESSED_RGBA_ASTC_4x4_KHR)
              : static_cast<int>(format -
                                 GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);

      const int kBlockWidth = kASTCBlockArray[index].blockWidth;
      const int kBlockHeight = kASTCBlockArray[index].blockHeight;

      bytes_required = (width + kBlockWidth - 1) / kBlockWidth;
      bytes_required *= (height + kBlockHeight - 1) / kBlockHeight;

      bytes_required *= kASTCBlockSize;
      break;
    }
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      bytes_required = (width + kS3TCBlockWidth - 1) / kS3TCBlockWidth;
      bytes_required *= (height + kS3TCBlockHeight - 1) / kS3TCBlockHeight;
      bytes_required *= kS3TCDXT3AndDXT5BlockSize;
      break;
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
      bytes_required = std::max(width, 8);
      bytes_required *= std::max(height, 8);
      bytes_required *= 4;
      bytes_required += 7;
      bytes_required /= 8;
      break;
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
      bytes_required = std::max(width, 16);
      bytes_required *= std::max(height, 8);
      bytes_required *= 2;
      bytes_required += 7;
      bytes_required /= 8;
      break;

    // ES3 formats.
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      bytes_required =
          (width + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *=
          (height + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *= 8;
      bytes_required *= depth;
      break;
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      bytes_required =
          (width + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *=
          (height + kEACAndETC2BlockSize - 1) / kEACAndETC2BlockSize;
      bytes_required *= 16;
      bytes_required *= depth;
      break;
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT:
    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT:
      bytes_required = (width + kBPTCBlockWidth - 1) / kBPTCBlockWidth;
      bytes_required *= (height + kBPTCBlockHeight - 1) / kBPTCBlockHeight;
      bytes_required *= 16;
      bytes_required *= depth;
      break;
    case GL_COMPRESSED_RED_RGTC1_EXT:
    case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
      bytes_required = (width + kRGTCBlockWidth - 1) / kRGTCBlockWidth;
      bytes_required *= (height + kRGTCBlockHeight - 1) / kRGTCBlockHeight;
      bytes_required *= 8;
      bytes_required *= depth;
      break;
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
      bytes_required = (width + kRGTCBlockWidth - 1) / kRGTCBlockWidth;
      bytes_required *= (height + kRGTCBlockHeight - 1) / kRGTCBlockHeight;
      bytes_required *= 16;
      bytes_required *= depth;
      break;
    default:
      if (function_name && error_state) {
        ERRORSTATE_SET_GL_ERROR_INVALID_ENUM(error_state, function_name, format,
                                             "format");
      }
      return false;
  }

  if (!bytes_required.IsValid()) {
    if (function_name && error_state) {
      ERRORSTATE_SET_GL_ERROR(error_state, GL_INVALID_VALUE, function_name,
                              "invalid size");
    }
    return false;
  }

  *size_in_bytes = bytes_required.ValueOrDefault(0);
  return true;
}

bool ValidateCompressedFormatTarget(GLenum target, GLenum format) {
  if (target == GL_TEXTURE_3D) {
    // Formats not supporting 3D Tex
    switch (format) {
      // ES 3.1, Section 8.7, page 169.
      case GL_COMPRESSED_R11_EAC:
      case GL_COMPRESSED_SIGNED_R11_EAC:
      case GL_COMPRESSED_RG11_EAC:
      case GL_COMPRESSED_SIGNED_RG11_EAC:
      case GL_COMPRESSED_RGB8_ETC2:
      case GL_COMPRESSED_SRGB8_ETC2:
      case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      case GL_COMPRESSED_RGBA8_ETC2_EAC:
      case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      // GL_EXT_texture_compression_s3tc
      case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
      case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
      case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
      case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
      case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
      case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      // GL_EXT_texture_compression_rgtc
      case GL_COMPRESSED_RED_RGTC1_EXT:
      case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
      case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
      case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
        return false;
      // GL_KHR_texture_compression_astc_hdr, TEXTURE_3D is not supported
      // without HDR profile. This is guaranteed to be validated beforehand
      // in GLES2DecoderImpl::TexStorageImpl before calling this.
      default:
        break;
    }
  }

  return true;
}

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
                                        const char** error_message) {
  if (!ValidateCompressedFormatTarget(target, format)) {
    *error_message = "target invalid for format";
    return false;
  }

  if (xoffset < 0 || yoffset < 0 || zoffset < 0) {
    *error_message = "x/y/z offset < 0";
    return false;
  }

  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: {
      const int kBlockWidth = 4;
      const int kBlockHeight = 4;
      if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight)) {
        *error_message = "xoffset or yoffset not multiple of 4";
        return false;
      }
      GLsizei tex_width = 0;
      GLsizei tex_height = 0;
      if (!texture->GetLevelSize(target, level, &tex_width, &tex_height,
                                 nullptr) ||
          width - xoffset > tex_width || height - yoffset > tex_height) {
        *error_message = "dimensions out of range";
        return false;
      }
      if ((((width % kBlockWidth) != 0) && (width + xoffset != tex_width)) ||
          (((height % kBlockHeight) != 0) &&
           (height + yoffset != tex_height))) {
        *error_message = "dimensions do not align to a block boundary";
        return false;
      }
      return true;
    }
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: {
      const int index =
          (format < GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR)
              ? static_cast<int>(format - GL_COMPRESSED_RGBA_ASTC_4x4_KHR)
              : static_cast<int>(format -
                                 GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);

      const int kBlockWidth = kASTCBlockArray[index].blockWidth;
      const int kBlockHeight = kASTCBlockArray[index].blockHeight;

      if ((xoffset % kBlockWidth) || (yoffset % kBlockHeight)) {
        *error_message = "xoffset or yoffset not multiple of 4";
        return false;
      }
      GLsizei tex_width = 0;
      GLsizei tex_height = 0;
      if (!texture->GetLevelSize(target, level, &tex_width, &tex_height,
                                 nullptr) ||
          width - xoffset > tex_width || height - yoffset > tex_height) {
        *error_message = "dimensions out of range";
        return false;
      }
      if ((((width % kBlockWidth) != 0) && (width + xoffset != tex_width)) ||
          (((height % kBlockHeight) != 0) &&
           (height + yoffset != tex_height))) {
        *error_message = "dimensions do not align to a block boundary";
        return false;
      }
      return true;
    }
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD: {
      *error_message = "not supported for ATC textures";
      return false;
    }
    case GL_ETC1_RGB8_OES: {
      *error_message = "not supported for ECT1_RGB8_OES textures";
      return false;
    }
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG: {
      if ((xoffset != 0) || (yoffset != 0)) {
        *error_message = "xoffset and yoffset must be zero";
        return false;
      }
      GLsizei tex_width = 0;
      GLsizei tex_height = 0;
      if (!texture->GetLevelSize(target, level, &tex_width, &tex_height,
                                 nullptr) ||
          width != tex_width || height != tex_height) {
        *error_message =
            "dimensions must match existing texture level dimensions";
        return false;
      }
      return ValidateCompressedTexDimensions(target, level, width, height, 1,
                                             format, error_message);
    }

    // ES3 formats
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    case GL_COMPRESSED_RED_RGTC1_EXT:
    case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT: {
      if (target == GL_TEXTURE_3D) {
        *error_message = "target == GL_TEXTURE_3D is not allowed";
        return false;
      }
      [[fallthrough]];
    }
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT:
    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT: {
      const int kBlockSize = 4;
      GLsizei tex_width, tex_height;
      if (!texture->GetLevelSize(target, level, &tex_width, &tex_height,
                                 nullptr) ||
          (xoffset % kBlockSize) || (yoffset % kBlockSize) ||
          ((width % kBlockSize) && xoffset + width != tex_width) ||
          ((height % kBlockSize) && yoffset + height != tex_height)) {
        *error_message =
            "dimensions must match existing texture level dimensions";
        return false;
      }
      return true;
    }
    default:
      *error_message = "unknown compressed texture format";
      return false;
  }
}

bool ValidateCompressedTexDimensions(GLenum target,
                                     GLint level,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLenum format,
                                     const char** error_message) {
  if (!ValidateCompressedFormatTarget(target, format)) {
    *error_message = "target invalid for format";
    return false;
  }
  switch (format) {
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
      if (!IsValidS3TCSizeForWebGLAndANGLE(level, width) ||
          !IsValidS3TCSizeForWebGLAndANGLE(level, height)) {
        *error_message = "width or height invalid for level";
        return false;
      }
      return true;
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_ETC1_RGB8_OES:
      if (width <= 0 || height <= 0) {
        *error_message = "width or height invalid for level";
        return false;
      }
      return true;
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
      if (!IsValidPVRTCSize(level, width) || !IsValidPVRTCSize(level, height)) {
        *error_message = "width or height invalid for level";
        return false;
      }
      return true;

    // ES3 formats.
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      if (width < 0 || height < 0 || depth < 0) {
        *error_message = "width, height, or depth invalid";
        return false;
      }
      return true;
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT:
    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT:
      if (width < 0 || height < 0 || depth < 0) {
        *error_message = "width, height, or depth invalid";
        return false;
      }
      if (level == 0 &&
          !(width % kBPTCBlockWidth == 0 && height % kBPTCBlockHeight == 0)) {
        *error_message = "width or height is not a multiple of four";
        return false;
      }
      return true;
    case GL_COMPRESSED_RED_RGTC1_EXT:
    case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
      if (width < 0 || height < 0 || depth < 0) {
        *error_message = "width, height, or depth invalid";
        return false;
      }
      if (level == 0 &&
          !(width % kRGTCBlockWidth == 0 && height % kRGTCBlockHeight == 0)) {
        *error_message = "width or height is not a multiple of four";
        return false;
      }
      return true;
    default:
      return false;
  }
}

bool ValidateCopyTexFormatHelper(const FeatureInfo* feature_info,
                                 GLenum internal_format,
                                 GLenum read_format,
                                 GLenum read_type,
                                 std::string* output_error_msg) {
  DCHECK(output_error_msg);
  if (read_format == 0) {
    *output_error_msg = std::string("no valid color image");
    return false;
  }
  // Check we have compatible formats.
  uint32_t channels_exist = GLES2Util::GetChannelsForFormat(read_format);
  uint32_t channels_needed = GLES2Util::GetChannelsForFormat(internal_format);
  if (!channels_needed ||
      (channels_needed & channels_exist) != channels_needed) {
    *output_error_msg = std::string("incompatible format");
    return false;
  }
  if (feature_info->IsWebGL2OrES3OrHigherContext()) {
    GLint color_encoding =
        GLES2Util::GetColorEncodingFromInternalFormat(read_format);
    bool float_mismatch = feature_info->ext_color_buffer_float_available()
                              ? (GLES2Util::IsIntegerFormat(internal_format) !=
                                 GLES2Util::IsIntegerFormat(read_format))
                              : GLES2Util::IsFloatFormat(internal_format);
    if (color_encoding !=
            GLES2Util::GetColorEncodingFromInternalFormat(internal_format) ||
        float_mismatch ||
        (GLES2Util::IsSignedIntegerFormat(internal_format) !=
         GLES2Util::IsSignedIntegerFormat(read_format)) ||
        (GLES2Util::IsUnsignedIntegerFormat(internal_format) !=
         GLES2Util::IsUnsignedIntegerFormat(read_format))) {
      *output_error_msg = std::string("incompatible format");
      return false;
    }
  }
  if ((channels_needed & (GLES2Util::kDepth | GLES2Util::kStencil)) != 0) {
    *output_error_msg =
        std::string("can not be used with depth or stencil textures");
    return false;
  }
  if (feature_info->IsWebGL2OrES3OrHigherContext() ||
      (feature_info->feature_flags().chromium_color_buffer_float_rgb &&
       internal_format == GL_RGB32F) ||
      (feature_info->feature_flags().chromium_color_buffer_float_rgba &&
       internal_format == GL_RGBA32F)) {
    if (GLES2Util::IsSizedColorFormat(internal_format)) {
      int sr, sg, sb, sa;
      GLES2Util::GetColorFormatComponentSizes(read_format, read_type, &sr, &sg,
                                              &sb, &sa);
      DCHECK(sr > 0 || sg > 0 || sb > 0 || sa > 0);
      int dr, dg, db, da;
      GLES2Util::GetColorFormatComponentSizes(internal_format, 0, &dr, &dg, &db,
                                              &da);
      DCHECK(dr > 0 || dg > 0 || db > 0 || da > 0);
      if ((dr > 0 && sr != dr) || (dg > 0 && sg != dg) ||
          (db > 0 && sb != db) || (da > 0 && sa != da)) {
        *output_error_msg = std::string("incompatible color component sizes");
        return false;
      }
    }
  }
  return true;
}

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
                                               bool unpremultiply_alpha) {
  bool premultiply_alpha_change = premultiply_alpha ^ unpremultiply_alpha;
  bool source_format_color_renderable =
      Texture::ColorRenderable(feature_info, source_internal_format, false);
  bool dest_format_color_renderable =
      Texture::ColorRenderable(feature_info, dest_internal_format, false);
  std::string output_error_msg;

  switch (dest_internal_format) {
#if BUILDFLAG(IS_MAC)
    // RGB5_A1 is not color-renderable on NVIDIA Mac, see
    // https://crbug.com/676209.
    case GL_RGB5_A1:
      return CopyTextureMethod::DRAW_AND_READBACK;
#endif
    // RGB9_E5 isn't accepted by glCopyTexImage2D if underlying context is ES.
    case GL_RGB9_E5:
      return CopyTextureMethod::DRAW_AND_READBACK;
    // SRGB format has color-space conversion issue. WebGL spec doesn't define
    // clearly if linear-to-srgb color space conversion is required or not when
    // uploading DOM elements to SRGB textures. WebGL conformance test expects
    // no linear-to-srgb conversion, while current GPU path for
    // CopyTextureCHROMIUM does the conversion. Do a fallback path before the
    // issue is resolved. see https://github.com/KhronosGroup/WebGL/issues/2165.
    // TODO(qiankun.miao@intel.com): revisit this once the above issue is
    // resolved.
    case GL_SRGB_EXT:
    case GL_SRGB_ALPHA_EXT:
    case GL_SRGB8:
    case GL_SRGB8_ALPHA8:
      if (feature_info->IsWebGLContext())
        return CopyTextureMethod::DRAW_AND_READBACK;
      break;
    default:
      break;
  }

  // CopyTexImage* should not allow internalformat of GL_BGRA_EXT and
  // GL_BGRA8_EXT. https://crbug.com/663086.
  bool copy_tex_image_format_valid =
      source_internal_format != GL_BGRA_EXT &&
      dest_internal_format != GL_BGRA_EXT &&
      source_internal_format != GL_BGRA8_EXT &&
      dest_internal_format != GL_BGRA8_EXT &&
      ValidateCopyTexFormatHelper(feature_info, dest_internal_format,
                                  source_internal_format, source_type,
                                  &output_error_msg);

  // The ES3 spec is vague about whether or not glCopyTexImage2D from a
  // GL_RGB10_A2 attachment to an unsized internal format is valid. Most drivers
  // interpreted the explicit call out as not valid (and dEQP actually checks
  // this), so avoid DIRECT_COPY in that case.
  if (source_internal_format == GL_RGB10_A2 &&
      dest_internal_format != source_internal_format) {
    copy_tex_image_format_valid = false;
  }

  // TODO(qiankun.miao@intel.com): for WebGL 2.0 or OpenGL ES 3.0, both
  // DIRECT_DRAW path for dest_level > 0 and DIRECT_COPY path for source_level >
  // 0 are not available due to a framebuffer completeness bug:
  // https://crbug.com/678526. Once the bug is fixed, the limitation for WebGL
  // 2.0 and OpenGL ES 3.0 can be lifted. For WebGL 1.0 or OpenGL ES 2.0,
  // DIRECT_DRAW path isn't available for dest_level > 0 due to level > 0 isn't
  // supported by glFramebufferTexture2D in ES2 context. DIRECT_DRAW path isn't
  // available for cube map dest texture either due to it may be cube map
  // incomplete. Go to DRAW_AND_COPY path in these cases.
  if (source_target == GL_TEXTURE_2D &&
      (dest_target == GL_TEXTURE_2D || dest_target == GL_TEXTURE_CUBE_MAP) &&
      source_format_color_renderable && copy_tex_image_format_valid &&
      source_level == 0 && !flip_y && !premultiply_alpha_change) {
    auto source_texture_type = GLES2Util::GetGLReadPixelsImplementationType(
        source_internal_format, source_target);
    auto dest_texture_type = GLES2Util::GetGLReadPixelsImplementationType(
        dest_internal_format, dest_target);
    if (source_texture_type != GL_UNSIGNED_SHORT ||
        source_texture_type == dest_texture_type) {
      // https://crbug.com/1042239. As it is stated in the latest OpenGL ES 3.2
      // spec (Oct 22, 2019) it is optional for implementation to support
      // conversion between unmatched source and dest effective internal format.
      // R16 to R16F direct copy failure is seen on Android Nvidia shield
      // devices. So we won't use DIRECT_COPY for this format.
      return CopyTextureMethod::DIRECT_COPY;
    }
  }
  if (dest_format_color_renderable && dest_level == 0 &&
      dest_target != GL_TEXTURE_CUBE_MAP)
    return CopyTextureMethod::DIRECT_DRAW;

  // Draw to a fbo attaching level 0 of an intermediate texture,
  // then copy from the fbo to dest texture level with glCopyTexImage2D.
  return CopyTextureMethod::DRAW_AND_COPY;
}

bool ValidateCopyTextureCHROMIUMInternalFormats(const FeatureInfo* feature_info,
                                                GLenum source_internal_format,
                                                GLenum dest_internal_format,
                                                std::string* output_error_msg) {
  bool valid_dest_format = false;
  // TODO(qiankun.miao@intel.com): ALPHA, LUMINANCE and LUMINANCE_ALPHA formats
  // are not supported on GL core profile. See https://crbug.com/577144. Enable
  // the workaround for glCopyTexImage and glCopyTexSubImage in
  // gles2_cmd_copy_tex_image.cc for glCopyTextureCHROMIUM implementation.
  switch (dest_internal_format) {
    case GL_RGB:
    case GL_RGBA:
    case GL_RGB8:
    case GL_RGBA8:
      valid_dest_format = true;
      break;
    case GL_BGRA_EXT:
    case GL_BGRA8_EXT:
      valid_dest_format =
          feature_info->feature_flags().ext_texture_format_bgra8888;
      break;
    case GL_SRGB_EXT:
    case GL_SRGB_ALPHA_EXT:
      valid_dest_format = feature_info->feature_flags().ext_srgb;
      break;
    case GL_R8:
    case GL_R8UI:
    case GL_RG8:
    case GL_RG8UI:
    case GL_SRGB8:
    case GL_RGB565:
    case GL_RGB8UI:
    case GL_SRGB8_ALPHA8:
    case GL_RGB5_A1:
    case GL_RGBA4:
    case GL_RGBA8UI:
    case GL_RGB10_A2:
      valid_dest_format = feature_info->IsWebGL2OrES3OrHigherContext();
      break;
    case GL_RGB9_E5:
    case GL_R16F:
    case GL_R32F:
    case GL_RG16F:
    case GL_RG32F:
    case GL_RGB16F:
    case GL_RGBA16F:
    case GL_R11F_G11F_B10F:
      valid_dest_format = feature_info->ext_color_buffer_float_available();
      break;
    case GL_RGB32F:
      valid_dest_format =
          feature_info->ext_color_buffer_float_available() ||
          feature_info->feature_flags().chromium_color_buffer_float_rgb;
      break;
    case GL_RGBA32F:
      valid_dest_format =
          feature_info->ext_color_buffer_float_available() ||
          feature_info->feature_flags().chromium_color_buffer_float_rgba;
      break;
    case GL_ALPHA:
    case GL_LUMINANCE:
    case GL_LUMINANCE_ALPHA:
      valid_dest_format = true;
      break;
    default:
      valid_dest_format = false;
      break;
  }

  // TODO(aleksandar.stojiljkovic): Use sized internal formats:
  // https://crbug.com/628064
  bool valid_source_format =
      source_internal_format == GL_RED || source_internal_format == GL_ALPHA ||
      source_internal_format == GL_RGB || source_internal_format == GL_RGBA ||
      source_internal_format == GL_RGB8 || source_internal_format == GL_RGBA8 ||
      source_internal_format == GL_LUMINANCE ||
      source_internal_format == GL_LUMINANCE_ALPHA ||
      source_internal_format == GL_BGRA_EXT ||
      source_internal_format == GL_BGRA8_EXT ||
      source_internal_format == GL_R16_EXT ||
      source_internal_format == GL_RG16_EXT ||
      source_internal_format == GL_RGBA16_EXT ||
      source_internal_format == GL_RGB10_A2;
  if (!valid_source_format) {
    *output_error_msg = "invalid source internal format " +
                        GLES2Util::GetStringEnum(source_internal_format);
    return false;
  }
  if (!valid_dest_format) {
    *output_error_msg = "invalid dest internal format " +
                        GLES2Util::GetStringEnum(dest_internal_format);
    return false;
  }

  return true;
}

GLenum GetTextureBindingQuery(GLenum texture_type) {
  switch (texture_type) {
    case GL_TEXTURE_2D:
      return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_2D_ARRAY:
      return GL_TEXTURE_BINDING_2D_ARRAY;
    case GL_TEXTURE_2D_MULTISAMPLE:
      return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
      return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
    case GL_TEXTURE_3D:
      return GL_TEXTURE_BINDING_3D;
    case GL_TEXTURE_EXTERNAL_OES:
      return GL_TEXTURE_BINDING_EXTERNAL_OES;
    case GL_TEXTURE_RECTANGLE:
      return GL_TEXTURE_BINDING_RECTANGLE;
    case GL_TEXTURE_CUBE_MAP:
      return GL_TEXTURE_BINDING_CUBE_MAP;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

bool IsASTCFormat(GLenum internal_format) {
  switch (internal_format) {
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
      return true;
    default:
      break;
  }
  return false;
}

// This is only called in Texture::SetLevelInfo in texture_manager.cc
// where there is no direct access to decoder->IsCompressedTextureFormat
// or feature_info->validators()->compressed_texture_format.IsValid
bool IsCompressedTextureFormat(GLenum internal_format) {
  switch (internal_format) {
    // S3TC
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
    // ASTC
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
    // BPTC
    case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGBA_BPTC_UNORM_EXT:
    case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT:
    case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT:
    // RGTC
    case GL_COMPRESSED_RED_RGTC1_EXT:
    case GL_COMPRESSED_SIGNED_RED_RGTC1_EXT:
    case GL_COMPRESSED_RED_GREEN_RGTC2_EXT:
    case GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT:
    // ETC2/EAC
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    // ETC1
    case GL_ETC1_RGB8_OES:
    // PVRTC
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
    // ATC
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
      return true;
    default:
      break;
  }
  return false;
}

Texture* CreateGLES2TextureWithLightRef(GLuint service_id, GLenum target) {
  Texture* texture = new Texture(service_id);
  texture->SetLightweightRef();
  texture->SetTarget(target, 1 /*max_levels=*/);
  texture->set_min_filter(GL_LINEAR);
  texture->set_mag_filter(GL_LINEAR);
  texture->set_wrap_t(GL_CLAMP_TO_EDGE);
  texture->set_wrap_s(GL_CLAMP_TO_EDGE);
  return texture;
}

}  // namespace gles2
}  // namespace gpu
