// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/feature_info.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_version_info.h"

#if !BUILDFLAG(IS_MAC)
#include "ui/gl/gl_fence_egl.h"
#else
#include "base/mac/mac_util.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

class ScopedPixelUnpackBufferOverride {
 public:
  explicit ScopedPixelUnpackBufferOverride(bool has_pixel_buffers,
                                           GLuint binding_override)
      : orig_binding_(-1) {
    if (has_pixel_buffers) {
      GLint orig_binding = 0;
      glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &orig_binding);
      if (static_cast<GLuint>(orig_binding) != binding_override) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, binding_override);
        orig_binding_ = orig_binding;
      }
    }
  }

  ~ScopedPixelUnpackBufferOverride() {
    if (orig_binding_ != -1) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(orig_binding_));
    }
  }

 private:
    GLint orig_binding_;
};

bool IsWebGLDrawBuffersSupported(bool webglCompatibilityContext,
                                 GLenum depth_texture_internal_format,
                                 GLenum depth_stencil_texture_internal_format) {
  // This is called after we make sure GL_EXT_draw_buffers is supported.
  GLint max_draw_buffers = 0;
  GLint max_color_attachments = 0;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);
  glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
  if (max_draw_buffers < 4 || max_color_attachments < 4) {
    return false;
  }

  GLint fb_binding = 0;
  GLint tex_binding = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb_binding);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex_binding);

  GLuint fbo;
  glGenFramebuffersEXT(1, &fbo);
  glBindFramebufferEXT(GL_FRAMEBUFFER, fbo);

  GLuint depth_stencil_texture = 0;
  if (depth_stencil_texture_internal_format != GL_NONE) {
    glGenTextures(1, &depth_stencil_texture);
    glBindTexture(GL_TEXTURE_2D, depth_stencil_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, depth_stencil_texture_internal_format, 1, 1,
                 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  }

  GLuint depth_texture = 0;
  if (depth_texture_internal_format != GL_NONE) {
    glGenTextures(1, &depth_texture);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, depth_texture_internal_format, 1, 1, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
  }

  GLint max_allowed_buffers = std::min(max_draw_buffers, max_color_attachments);
  std::vector<GLuint> colors(max_allowed_buffers, 0);
  glGenTextures(max_allowed_buffers, colors.data());

  bool result = true;
  for (GLint i = 0; i < max_allowed_buffers; ++i) {
    GLint color = colors[i];
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                              GL_TEXTURE_2D, color, 0);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) !=
        GL_FRAMEBUFFER_COMPLETE) {
      result = false;
      break;
    }
    if (depth_texture != 0) {
      glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_TEXTURE_2D, depth_texture, 0);
      if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) !=
          GL_FRAMEBUFFER_COMPLETE) {
        result = false;
        break;
      }
      glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                GL_TEXTURE_2D, 0, 0);
    }
    if (depth_stencil_texture != 0) {
      // WebGL compatibility contexts do not allow the same texture bound to the
      // DEPTH and STENCIL attachment points, instead the texture must be bound
      // to the DEPTH_STENCIL.
      if (webglCompatibilityContext) {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_TEXTURE_2D, depth_stencil_texture, 0);
      } else {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_TEXTURE_2D, depth_stencil_texture, 0);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_TEXTURE_2D, depth_stencil_texture, 0);
      }
      if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) !=
          GL_FRAMEBUFFER_COMPLETE) {
        result = false;
        break;
      }
      if (webglCompatibilityContext) {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_TEXTURE_2D, 0, 0);
      } else {
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_TEXTURE_2D, 0, 0);
      }
    }
  }

  glBindFramebufferEXT(GL_FRAMEBUFFER, static_cast<GLuint>(fb_binding));
  glDeleteFramebuffersEXT(1, &fbo);

  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex_binding));
  glDeleteTextures(1, &depth_texture);
  glDeleteTextures(1, &depth_stencil_texture);
  glDeleteTextures(colors.size(), colors.data());

  DCHECK(glGetError() == GL_NO_ERROR);

  return result;
}

}  // anonymous namespace.

FeatureInfo::FeatureFlags::FeatureFlags() = default;

FeatureInfo::FeatureInfo() {
  InitializeBasicState(base::CommandLine::InitializedForCurrentProcess()
                           ? base::CommandLine::ForCurrentProcess()
                           : nullptr);
}

FeatureInfo::FeatureInfo(
    const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds,
    const GpuFeatureInfo& gpu_feature_info)
    : workarounds_(gpu_driver_bug_workarounds) {
  InitializeBasicState(base::CommandLine::InitializedForCurrentProcess()
                           ? base::CommandLine::ForCurrentProcess()
                           : nullptr);

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  feature_flags_.chromium_image_ycbcr_420v = base::Contains(
      gpu_feature_info.supported_buffer_formats_for_allocation_and_texturing,
      gfx::BufferFormat::YUV_420_BIPLANAR);
#elif BUILDFLAG(IS_APPLE)
  feature_flags_.chromium_image_ycbcr_420v = true;
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  feature_flags_.chromium_image_ycbcr_p010 = base::Contains(
      gpu_feature_info.supported_buffer_formats_for_allocation_and_texturing,
      gfx::BufferFormat::P010);
#elif BUILDFLAG(IS_APPLE)
  feature_flags_.chromium_image_ycbcr_p010 = true;
#endif
}

void FeatureInfo::InitializeBasicState(const base::CommandLine* command_line) {
  if (!command_line)
    return;

  feature_flags_.enable_shader_name_hashing =
      !command_line->HasSwitch(switches::kDisableShaderNameHashing);

  const auto useGL = command_line->GetSwitchValueASCII(switches::kUseGL);
  const auto useANGLE = command_line->GetSwitchValueASCII(switches::kUseANGLE);

  feature_flags_.is_swiftshader_for_webgl =
      (useGL == gl::kGLImplementationANGLEName) &&
      (useANGLE == gl::kANGLEImplementationSwiftShaderForWebGLName);

  // The shader translator is needed to translate from WebGL-conformant GLES SL
  // to normal GLES SL, enforce WebGL conformance, translate from GLES SL 1.0 to
  // target context GLSL, implement emulation of OpenGL ES features on OpenGL,
  // etc.
  // The flag here is for testing only.
  disable_shader_translator_ =
      command_line->HasSwitch(switches::kDisableGLSLTranslator);
}

void FeatureInfo::Initialize(ContextType context_type,
                             bool is_passthrough_cmd_decoder,
                             const DisallowedFeatures& disallowed_features,
                             bool force_reinitialize) {
  if (initialized_) {
    DCHECK_EQ(context_type, context_type_);
    DCHECK_EQ(is_passthrough_cmd_decoder, is_passthrough_cmd_decoder_);
    DCHECK(disallowed_features == disallowed_features_);
    if (!force_reinitialize)
      return;
  }

  disallowed_features_ = disallowed_features;
  context_type_ = context_type;
  is_passthrough_cmd_decoder_ = is_passthrough_cmd_decoder;
  InitializeFeatures();
  initialized_ = true;
}

void FeatureInfo::InitializeForTesting(
    const DisallowedFeatures& disallowed_features) {
  initialized_ = false;
  Initialize(CONTEXT_TYPE_OPENGLES2, false /* is_passthrough_cmd_decoder */,
             disallowed_features);
}

void FeatureInfo::InitializeForTesting() {
  initialized_ = false;
  Initialize(CONTEXT_TYPE_OPENGLES2, false /* is_passthrough_cmd_decoder */,
             DisallowedFeatures());
}

void FeatureInfo::InitializeForTesting(ContextType context_type) {
  initialized_ = false;
  Initialize(context_type, false /* is_passthrough_cmd_decoder */,
             DisallowedFeatures());
}

bool IsGL_REDSupportedOnFBOs() {
#if BUILDFLAG(IS_MAC)
  // The glTexImage2D call below can hang on Mac so skip this since it's only
  // really needed to workaround a Mesa issue. See https://crbug.com/1158744.
  return true;
#else

#if BUILDFLAG(IS_ANDROID)
  return true;
#endif

  DCHECK(glGetError() == GL_NO_ERROR);
  // Skia uses GL_RED with frame buffers, unfortunately, Mesa claims to support
  // GL_EXT_texture_rg, but it doesn't support it on frame buffers.  To fix
  // this, we try it, and if it fails, we don't expose GL_EXT_texture_rg.
  GLint fb_binding = 0;
  GLint tex_binding = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb_binding);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex_binding);

  GLuint textureId = 0;
  glGenTextures(1, &textureId);
  glBindTexture(GL_TEXTURE_2D, textureId);
  // Attach an 8x8 texture to the framebuffer to try to work around
  // crashes in MediaTek and PowerVR drivers on Android, presumably
  // because of implicit minimum framebuffer sizes. crbug.com/1406096
  //
  // Also, don't pass data in, to avoid having to set/reset
  // GL_UNPACK_ALIGNMENT and other state. The contents of the
  // framebuffer are unimportant for the completeness check.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED_EXT, 8, 8, 0, GL_RED_EXT,
               GL_UNSIGNED_BYTE, nullptr);
  GLuint textureFBOID = 0;
  glGenFramebuffersEXT(1, &textureFBOID);
  glBindFramebufferEXT(GL_FRAMEBUFFER, textureFBOID);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            textureId, 0);
  bool result =
      glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glDeleteFramebuffersEXT(1, &textureFBOID);
  glDeleteTextures(1, &textureId);

  glBindFramebufferEXT(GL_FRAMEBUFFER, static_cast<GLuint>(fb_binding));
  glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex_binding));

  DCHECK(glGetError() == GL_NO_ERROR);

  return result;
#endif  // BUILDFLAG(IS_MAC)
}

void FeatureInfo::EnableEXTFloatBlend() {
  if (!feature_flags_.ext_float_blend) {
    AddExtensionString("GL_EXT_float_blend");
    feature_flags_.ext_float_blend = true;
  }
}

void FeatureInfo::EnableEXTColorBufferFloat() {
  if (!ext_color_buffer_float_available_)
    return;
  AddExtensionString("GL_EXT_color_buffer_float");
  validators_.render_buffer_format.AddValue(GL_R16F);
  validators_.render_buffer_format.AddValue(GL_RG16F);
  validators_.render_buffer_format.AddValue(GL_RGBA16F);
  validators_.render_buffer_format.AddValue(GL_R32F);
  validators_.render_buffer_format.AddValue(GL_RG32F);
  validators_.render_buffer_format.AddValue(GL_RGBA32F);
  validators_.render_buffer_format.AddValue(GL_R11F_G11F_B10F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(GL_R16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(GL_RG16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_RGBA16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(GL_R32F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(GL_RG32F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_RGBA32F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_R11F_G11F_B10F);
  feature_flags_.enable_color_buffer_float = true;
}

void FeatureInfo::EnableEXTColorBufferHalfFloat() {
  if (!ext_color_buffer_half_float_available_)
    return;
  AddExtensionString("GL_EXT_color_buffer_half_float");
  validators_.render_buffer_format.AddValue(GL_R16F);
  validators_.render_buffer_format.AddValue(GL_RG16F);
  validators_.render_buffer_format.AddValue(GL_RGB16F);
  validators_.render_buffer_format.AddValue(GL_RGBA16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(GL_R16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(GL_RG16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_RGB16F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_RGBA16F);
  feature_flags_.enable_color_buffer_half_float = true;
}

void FeatureInfo::EnableEXTTextureFilterAnisotropic() {
  if (!ext_texture_filter_anisotropic_available_)
    return;
  AddExtensionString("GL_EXT_texture_filter_anisotropic");
  validators_.texture_parameter.AddValue(GL_TEXTURE_MAX_ANISOTROPY_EXT);
  validators_.g_l_state.AddValue(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
  if (IsWebGL2OrES3OrHigherContext()) {
    validators_.sampler_parameter.AddValue(GL_TEXTURE_MAX_ANISOTROPY_EXT);
  }
}

void FeatureInfo::EnableCHROMIUMColorBufferFloatRGBA() {
  if (!feature_flags_.chromium_color_buffer_float_rgba)
    return;
  validators_.texture_internal_format.AddValue(GL_RGBA32F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_RGBA32F);
  AddExtensionString("GL_CHROMIUM_color_buffer_float_rgba");
}

void FeatureInfo::EnableCHROMIUMColorBufferFloatRGB() {
  if (!feature_flags_.chromium_color_buffer_float_rgb)
    return;
  validators_.texture_internal_format.AddValue(GL_RGB32F);
  validators_.texture_sized_color_renderable_internal_format.AddValue(
      GL_RGB32F);
  AddExtensionString("GL_CHROMIUM_color_buffer_float_rgb");
}

void FeatureInfo::EnableOESDrawBuffersIndexed() {
  if (!feature_flags_.oes_draw_buffers_indexed) {
    AddExtensionString("GL_OES_draw_buffers_indexed");
    feature_flags_.oes_draw_buffers_indexed = true;
  }
}

void FeatureInfo::EnableOESFboRenderMipmap() {
  if (!feature_flags_.oes_fbo_render_mipmap) {
    AddExtensionString("GL_OES_fbo_render_mipmap");
    feature_flags_.oes_fbo_render_mipmap = true;
  }
}

void FeatureInfo::EnableOESTextureFloatLinear() {
  if (!oes_texture_float_linear_available_)
    return;
  AddExtensionString("GL_OES_texture_float_linear");
  feature_flags_.enable_texture_float_linear = true;
  validators_.texture_sized_texture_filterable_internal_format.AddValue(
      GL_R32F);
  validators_.texture_sized_texture_filterable_internal_format.AddValue(
      GL_RG32F);
  validators_.texture_sized_texture_filterable_internal_format.AddValue(
      GL_RGB32F);
  validators_.texture_sized_texture_filterable_internal_format.AddValue(
      GL_RGBA32F);
}

void FeatureInfo::EnableOESTextureHalfFloatLinear() {
  if (!oes_texture_half_float_linear_available_)
    return;
  AddExtensionString("GL_OES_texture_half_float_linear");
  feature_flags_.enable_texture_half_float_linear = true;

  // TODO(capn) : Re-enable this once we have ANGLE+SwiftShader supporting
  // IOSurfaces.
  if (workarounds_.disable_half_float_for_gmb)
    return;
  feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::RGBA_F16);
}

void FeatureInfo::EnableANGLEInstancedArrayIfPossible(
    const gfx::ExtensionSet& extensions) {
  if (!feature_flags_.angle_instanced_arrays) {
    if (gfx::HasExtension(extensions, "GL_ANGLE_instanced_arrays") ||
        gl_version_info_->is_es3) {
      AddExtensionString("GL_ANGLE_instanced_arrays");
      feature_flags_.angle_instanced_arrays = true;
      validators_.vertex_attribute.AddValue(
          GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE);
    }
  }
}

void FeatureInfo::EnableWEBGLMultiDrawIfPossible(
    const gfx::ExtensionSet& extensions) {
  if (!feature_flags_.webgl_multi_draw) {
    if (!is_passthrough_cmd_decoder_ ||
        gfx::HasExtension(extensions, "GL_ANGLE_multi_draw")) {
      if (gfx::HasExtension(extensions, "GL_ANGLE_instanced_arrays") ||
          feature_flags_.angle_instanced_arrays || gl_version_info_->is_es3) {
        feature_flags_.webgl_multi_draw = true;
        AddExtensionString("GL_WEBGL_multi_draw");

        EnableANGLEInstancedArrayIfPossible(extensions);
      }
    }
  }
}

void FeatureInfo::InitializeFeatures() {
  // Figure out what extensions to turn on.
  std::string extensions_string(gl::GetGLExtensionsFromCurrentContext());
  gfx::ExtensionSet extensions(gfx::MakeExtensionSet(extensions_string));

  const char* version_str =
      reinterpret_cast<const char*>(glGetString(GL_VERSION));
  const char* renderer_str =
      reinterpret_cast<const char*>(glGetString(GL_RENDERER));

  gl_version_info_ = std::make_unique<gl::GLVersionInfo>(
      version_str, renderer_str, extensions);

  bool enable_es3 = IsWebGL2OrES3OrHigherContext();

  // Really it's part of core OpenGL 2.1 and up, but let's assume the
  // extension is still advertised.
  bool has_pixel_buffers =
      gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_NV_pixel_buffer_object");

  // If ES3 or pixel buffer objects are enabled by the driver, we have to assume
  // the unpack buffer binding may be changed on the underlying context. This is
  // true whether or not this particular decoder exposes PBOs, as it could be
  // changed on another decoder that does expose them.
  ScopedPixelUnpackBufferOverride scoped_pbo_override(has_pixel_buffers, 0);

  AddExtensionString("GL_CHROMIUM_async_pixel_transfers");
  AddExtensionString("GL_CHROMIUM_bind_uniform_location");
  AddExtensionString("GL_CHROMIUM_command_buffer_query");
  AddExtensionString("GL_CHROMIUM_copy_texture");
  AddExtensionString("GL_CHROMIUM_deschedule");
  AddExtensionString("GL_CHROMIUM_get_error_query");
  AddExtensionString("GL_CHROMIUM_lose_context");
  AddExtensionString("GL_CHROMIUM_pixel_transfer_buffer_object");
  AddExtensionString("GL_CHROMIUM_rate_limit_offscreen_context");
  AddExtensionString("GL_CHROMIUM_resize");
  AddExtensionString("GL_CHROMIUM_resource_safe");
  AddExtensionString("GL_CHROMIUM_strict_attribs");
  AddExtensionString("GL_CHROMIUM_texture_mailbox");
  AddExtensionString("GL_CHROMIUM_trace_marker");

  // Pre es3, there are no PBOS and all unpack state is handled in client side.
  // With es3, unpack state is needed in server side. We always mark these
  // enums as valid and pass them to drivers only when a valid PBO is bound.
  // UNPACK_ROW_LENGTH, UNPACK_SKIP_ROWS, and UNPACK_SKIP_PIXELS are enabled,
  // but there is no need to add them to pixel_store validtor.
  AddExtensionString("GL_EXT_unpack_subimage");

  // OES_vertex_array_object is emulated if not present natively,
  // so the extension string is always exposed.
  AddExtensionString("GL_OES_vertex_array_object");

  if (gfx::HasExtension(extensions, "GL_ANGLE_translated_shader_source")) {
    feature_flags_.angle_translated_shader_source = true;
  }

  // The validating command decoder always supports
  // ANGLE_translated_shader_source regardless of the underlying context. But
  // passthrough relies on the underlying ANGLE context which can't always
  // support it (e.g. on Vulkan shaders are translated directly to binary
  // SPIR-V).
  if (!is_passthrough_cmd_decoder_ ||
      feature_flags_.angle_translated_shader_source) {
    AddExtensionString("GL_ANGLE_translated_shader_source");
  }

  // Check if we should allow GL_EXT_texture_compression_dxt1 and
  // GL_EXT_texture_compression_s3tc.
  bool enable_dxt1 = false;
  bool enable_dxt3 = false;
  bool enable_dxt5 = false;
  bool have_s3tc =
      gfx::HasExtension(extensions, "GL_EXT_texture_compression_s3tc");
  bool have_dxt3 =
      have_s3tc ||
      gfx::HasExtension(extensions, "GL_ANGLE_texture_compression_dxt3");
  bool have_dxt5 =
      have_s3tc ||
      gfx::HasExtension(extensions, "GL_ANGLE_texture_compression_dxt5");

  if (gfx::HasExtension(extensions, "GL_EXT_texture_compression_dxt1") ||
      gfx::HasExtension(extensions, "GL_ANGLE_texture_compression_dxt1") ||
      have_s3tc) {
    enable_dxt1 = true;
  }
  if (have_dxt3) {
    enable_dxt3 = true;
  }
  if (have_dxt5) {
    enable_dxt5 = true;
  }

  if (enable_dxt1) {
    feature_flags_.ext_texture_format_dxt1 = true;

    AddExtensionString("GL_ANGLE_texture_compression_dxt1");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);

    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  }

  if (enable_dxt3) {
    // The difference between GL_EXT_texture_compression_s3tc and
    // GL_ANGLE_texture_compression_dxt3 is that the former
    // requires on the fly compression. The latter does not.
    AddExtensionString("GL_ANGLE_texture_compression_dxt3");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
  }

  if (enable_dxt5) {
    feature_flags_.ext_texture_format_dxt5 = true;

    // The difference between GL_EXT_texture_compression_s3tc and
    // GL_ANGLE_texture_compression_dxt5 is that the former
    // requires on the fly compression. The latter does not.
    AddExtensionString("GL_ANGLE_texture_compression_dxt5");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
  }

  bool have_bptc =
      gfx::HasExtension(extensions, "GL_EXT_texture_compression_bptc");
  if (have_bptc) {
    feature_flags_.ext_texture_compression_bptc = true;
    AddExtensionString("GL_EXT_texture_compression_bptc");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_BPTC_UNORM_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGBA_BPTC_UNORM_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT);
  }

  bool have_rgtc =
      gfx::HasExtension(extensions, "GL_EXT_texture_compression_rgtc");
  if (have_rgtc) {
    feature_flags_.ext_texture_compression_rgtc = true;
    AddExtensionString("GL_EXT_texture_compression_rgtc");
    validators_.compressed_texture_format.AddValue(GL_COMPRESSED_RED_RGTC1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SIGNED_RED_RGTC1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RED_GREEN_RGTC2_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RED_RGTC1_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SIGNED_RED_RGTC1_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RED_GREEN_RGTC2_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT);
  }

  bool have_astc =
      gfx::HasExtension(extensions, "GL_KHR_texture_compression_astc_ldr");
  if (have_astc) {
    feature_flags_.ext_texture_format_astc = true;
    AddExtensionString("GL_KHR_texture_compression_astc_ldr");

    bool have_astc_hdr =
        gfx::HasExtension(extensions, "GL_KHR_texture_compression_astc_hdr");
    if (have_astc_hdr) {
      feature_flags_.ext_texture_format_astc_hdr = true;
      AddExtensionString("GL_KHR_texture_compression_astc_hdr");
    }

    // GL_COMPRESSED_RGBA_ASTC(0x93B0 ~ 0x93BD)
    GLint astc_format_it = GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
    GLint astc_format_max = GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
    for (; astc_format_it <= astc_format_max; astc_format_it++) {
      validators_.compressed_texture_format.AddValue(astc_format_it);
      validators_.texture_internal_format_storage.AddValue(astc_format_it);
    }

    // GL_COMPRESSED_SRGB8_ALPHA8_ASTC(0x93D0 ~ 0x93DD)
    astc_format_it = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
    astc_format_max = GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR;
    for (; astc_format_it <= astc_format_max; astc_format_it++) {
      validators_.compressed_texture_format.AddValue(astc_format_it);
      validators_.texture_internal_format_storage.AddValue(astc_format_it);
    }
  }

  bool have_atc =
      gfx::HasExtension(extensions, "GL_AMD_compressed_ATC_texture") ||
      gfx::HasExtension(extensions, "GL_ATI_texture_compression_atitc");
  if (have_atc) {
    feature_flags_.ext_texture_format_atc = true;

    AddExtensionString("GL_AMD_compressed_ATC_texture");
    validators_.compressed_texture_format.AddValue(GL_ATC_RGB_AMD);
    validators_.compressed_texture_format.AddValue(
        GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD);

    validators_.texture_internal_format_storage.AddValue(GL_ATC_RGB_AMD);
    validators_.texture_internal_format_storage.AddValue(
        GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD);
  }

  // Check if we should enable GL_EXT_texture_filter_anisotropic.
  if (gfx::HasExtension(extensions, "GL_EXT_texture_filter_anisotropic")) {
    ext_texture_filter_anisotropic_available_ = true;
    if (!disallowed_features_.ext_texture_filter_anisotropic)
      EnableEXTTextureFilterAnisotropic();
  }

  // Check if we should support GL_OES_packed_depth_stencil and/or
  // GL_GOOGLE_depth_texture / GL_CHROMIUM_depth_texture.
  //
  // NOTE: GL_OES_depth_texture requires support for depth cubemaps.
  //
  // Therefore we made up GL_GOOGLE_depth_texture / GL_CHROMIUM_depth_texture.
  //
  // GL_GOOGLE_depth_texture is legacy. As we exposed it into NaCl we can't
  // get rid of it.
  //
  bool enable_depth_texture = false;
  GLenum depth_texture_format = GL_NONE;
  if (!workarounds_.disable_depth_texture &&
      (gfx::HasExtension(extensions, "GL_OES_depth_texture") ||
       gfx::HasExtension(extensions, "GL_ANGLE_depth_texture"))) {
    // Note that we don't expose depth_texture extenion on top of ES3 if
    // the depth_texture extension isn't exposed by the ES3 driver.
    // This is because depth textures are filterable under linear mode in
    // ES2 + extension, but not in core ES3.
    enable_depth_texture = true;
    depth_texture_format = GL_DEPTH_COMPONENT;
    feature_flags_.angle_depth_texture =
        gfx::HasExtension(extensions, "GL_ANGLE_depth_texture");
  }

  if (enable_depth_texture) {
    AddExtensionString("GL_CHROMIUM_depth_texture");
    AddExtensionString("GL_GOOGLE_depth_texture");
    validators_.texture_internal_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.texture_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_SHORT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_INT);
    validators_.texture_depth_renderable_internal_format.AddValue(
        GL_DEPTH_COMPONENT);
  }

  GLenum depth_stencil_texture_format = GL_NONE;
  if (gfx::HasExtension(extensions, "GL_EXT_packed_depth_stencil") ||
      gfx::HasExtension(extensions, "GL_OES_packed_depth_stencil") ||
      gl_version_info_->is_es3) {
    AddExtensionString("GL_OES_packed_depth_stencil");
    feature_flags_.packed_depth24_stencil8 = true;
    if (enable_depth_texture) {
      if (gl_version_info_->is_es3) {
        depth_stencil_texture_format = GL_DEPTH24_STENCIL8;
      } else {
        depth_stencil_texture_format = GL_DEPTH_STENCIL;
      }
      validators_.texture_internal_format.AddValue(GL_DEPTH_STENCIL);
      validators_.texture_format.AddValue(GL_DEPTH_STENCIL);
      validators_.pixel_type.AddValue(GL_UNSIGNED_INT_24_8);
      validators_.texture_depth_renderable_internal_format.AddValue(
          GL_DEPTH_STENCIL);
      validators_.texture_stencil_renderable_internal_format.AddValue(
          GL_DEPTH_STENCIL);
    }
    validators_.render_buffer_format.AddValue(GL_DEPTH24_STENCIL8);
    if (context_type_ == CONTEXT_TYPE_WEBGL1) {
      // For glFramebufferRenderbuffer and glFramebufferTexture2D calls with
      // attachment == GL_DEPTH_STENCIL_ATTACHMENT, we always split into two
      // calls, one with attachment == GL_DEPTH_ATTACHMENT, and one with
      // attachment == GL_STENCIL_ATTACHMENT.  So even if the underlying driver
      // is ES2 where GL_DEPTH_STENCIL_ATTACHMENT isn't accepted, it is still
      // OK.
      validators_.attachment.AddValue(GL_DEPTH_STENCIL_ATTACHMENT);
      validators_.attachment_query.AddValue(GL_DEPTH_STENCIL_ATTACHMENT);
    }
  }

  if (gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_OES_vertex_array_object")) {
    feature_flags_.native_vertex_array_object = true;
  }

  // If we're using client_side_arrays we have to emulate
  // vertex array objects since vertex array objects do not work
  // with client side arrays.
  if (workarounds_.use_client_side_arrays_for_stream_buffers) {
    feature_flags_.native_vertex_array_object = false;
  }

  if (gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_OES_element_index_uint")) {
    AddExtensionString("GL_OES_element_index_uint");
    validators_.index_type.AddValue(GL_UNSIGNED_INT);
  }

  // Note (crbug.com/1058744): not implemented for validating command decoder
  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_OES_draw_buffers_indexed")) {
    if (!disallowed_features_.oes_draw_buffers_indexed) {
      EnableOESDrawBuffersIndexed();
    }
  }

  if (gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_OES_fbo_render_mipmap") ||
      gfx::HasExtension(extensions, "GL_EXT_framebuffer_object")) {
    if (!disallowed_features_.oes_fbo_render_mipmap) {
      EnableOESFboRenderMipmap();
    }
  }

  bool has_srgb_framebuffer_support = false;
  // With EXT_sRGB, unsized SRGB_EXT and SRGB_ALPHA_EXT are accepted by the
  // <format> and <internalformat> parameter of TexImage2D. GLES3 adds support
  // for SRGB Textures but the accepted internal formats for TexImage2D are only
  // sized formats GL_SRGB8 and GL_SRGB8_ALPHA8. Also, SRGB_EXT isn't a valid
  // <format> in this case. So, even with GLES3 explicitly check for
  // GL_EXT_sRGB.
  if ((gl_version_info_->is_es3 ||
       gfx::HasExtension(extensions, "GL_OES_rgb8_rgba8")) &&
      gfx::HasExtension(extensions, "GL_EXT_sRGB") &&
      IsWebGL1OrES2Context()) {
    feature_flags_.ext_srgb = true;
    AddExtensionString("GL_EXT_sRGB");
    validators_.texture_internal_format.AddValue(GL_SRGB_EXT);
    validators_.texture_internal_format.AddValue(GL_SRGB_ALPHA_EXT);
    validators_.texture_format.AddValue(GL_SRGB_EXT);
    validators_.texture_format.AddValue(GL_SRGB_ALPHA_EXT);
    validators_.render_buffer_format.AddValue(GL_SRGB8_ALPHA8_EXT);
    validators_.framebuffer_attachment_parameter.AddValue(
        GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT);
    validators_.texture_unsized_internal_format.AddValue(GL_SRGB_EXT);
    validators_.texture_unsized_internal_format.AddValue(GL_SRGB_ALPHA_EXT);
    has_srgb_framebuffer_support = true;
  }
  if (gl_version_info_->is_es3)
    has_srgb_framebuffer_support = true;

  if (has_srgb_framebuffer_support) {
    // GL_FRAMEBUFFER_SRGB_EXT is exposed by the GLES extension
    // GL_EXT_sRGB_write_control (which is not part of the core, even in GLES3).
    if (gfx::HasExtension(extensions, "GL_EXT_sRGB_write_control")) {
      feature_flags_.ext_srgb_write_control = true;

      // Do not expose this extension to WebGL.
      if (!IsWebGLContext()) {
        AddExtensionString("GL_EXT_sRGB_write_control");
        validators_.capability.AddValue(GL_FRAMEBUFFER_SRGB_EXT);
      }
    }
  }

  // The extension GL_EXT_texture_sRGB_decode is the same on desktop and GLES.
  if (gfx::HasExtension(extensions, "GL_EXT_texture_sRGB_decode") &&
      !IsWebGLContext()) {
    AddExtensionString("GL_EXT_texture_sRGB_decode");
    validators_.texture_parameter.AddValue(GL_TEXTURE_SRGB_DECODE_EXT);
  }

  // On mobile, the only extension that supports S3TC+sRGB is NV_sRGB_formats.
  // The draft extension EXT_texture_compression_s3tc_srgb also supports it
  // and is used if available (e.g. if ANGLE exposes it).
  bool have_s3tc_srgb =
      gfx::HasExtension(extensions, "GL_NV_sRGB_formats") ||
      gfx::HasExtension(extensions, "GL_EXT_texture_compression_s3tc_srgb");

  if (have_s3tc_srgb) {
    AddExtensionString("GL_EXT_texture_compression_s3tc_srgb");

    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);

    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
  }

  // Note: Only APPLE_texture_format_BGRA8888 extension allows BGRA8_EXT in
  // ES3's glTexStorage2D, whereas EXT_texture_format_BGRA8888 doesn't provide
  // that compatibility. So if EXT_texture_format_BGRA8888 (but not
  // APPLE_texture_format_BGRA8888) is present on an underlying ES3 context, we
  // have to choose which one of BGRA vs texture storage we expose.
  // When creating ES2 contexts, we prefer support BGRA to texture storage, in
  // order to use BGRA as platform color in the compositor, so we disable
  // texture storage if only EXT_texture_format_BGRA8888 is present.
  // If neither is present, we expose texture storage.
  // When creating ES3 contexts, we do need to expose texture storage, so we
  // disable BGRA if we have to.
  // In WebGL contexts, BRGA is used for hardware overlay and WebGL 2.0 exposes
  // glTexStorage2D. WebGL never uses both BGRA and glTexStorage2D together
  // because WebGL API doesn't expose BGRA format. So allow both.
  bool has_apple_bgra =
      gfx::HasExtension(extensions, "GL_APPLE_texture_format_BGRA8888");
  bool has_ext_bgra =
      gfx::HasExtension(extensions, "GL_EXT_texture_format_BGRA8888");
  bool enable_texture_format_bgra8888 = has_ext_bgra || has_apple_bgra;

  bool has_ext_texture_storage =
      gfx::HasExtension(extensions, "GL_EXT_texture_storage");
  bool has_texture_storage =
      !workarounds_.disable_texture_storage &&
      (has_ext_texture_storage || gl_version_info_->is_es3);

  bool enable_texture_storage = has_texture_storage;

  bool texture_storage_incompatible_with_bgra =
      gl_version_info_->is_es3 && !has_ext_texture_storage && !has_apple_bgra;
  if (texture_storage_incompatible_with_bgra &&
      enable_texture_format_bgra8888 && enable_texture_storage) {
    switch (context_type_) {
      case CONTEXT_TYPE_OPENGLES2:
        enable_texture_storage = false;
        break;
      case CONTEXT_TYPE_OPENGLES3:
      case CONTEXT_TYPE_OPENGLES31_FOR_TESTING:
        enable_texture_format_bgra8888 = false;
        break;
      case CONTEXT_TYPE_WEBGL1:
      case CONTEXT_TYPE_WEBGL2:
      case CONTEXT_TYPE_WEBGPU:
        break;
    }
  }

  if (enable_texture_storage) {
    feature_flags_.ext_texture_storage = true;
    AddExtensionString("GL_EXT_texture_storage");
    validators_.texture_parameter.AddValue(GL_TEXTURE_IMMUTABLE_FORMAT_EXT);
  }

  if (enable_texture_format_bgra8888) {
    feature_flags_.ext_texture_format_bgra8888 = true;
    AddExtensionString("GL_EXT_texture_format_BGRA8888");
    validators_.texture_internal_format.AddValue(GL_BGRA_EXT);
    validators_.texture_format.AddValue(GL_BGRA_EXT);
    validators_.texture_unsized_internal_format.AddValue(GL_BGRA_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_BGRA8_EXT);
    validators_.texture_sized_color_renderable_internal_format.AddValue(
        GL_BGRA8_EXT);
    validators_.texture_sized_texture_filterable_internal_format.AddValue(
        GL_BGRA8_EXT);
    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRA_8888);
    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::BGRX_8888);
  }

#if BUILDFLAG(IS_MAC)
  // TODO(sugoi): Remove this once crbug.com/1276529 is fixed.
  // On Mac OS, DrawingBuffer is using an IOSurface as its backing storage,
  // this allows WebGL-rendered canvases to be composited by the OS rather
  // than Chrome. Currently, this causes an issue on MacOS with SwANGLE when
  // alpha is false, so disable that case for now so that we go through
  // emulation.
  if (gl_version_info_->is_angle_swiftshader) {
    feature_flags_.gpu_memory_buffer_formats.Remove(
        gfx::BufferFormat::RGBX_8888);
    feature_flags_.gpu_memory_buffer_formats.Remove(
        gfx::BufferFormat::BGRX_8888);
  }
#endif

  // On desktop, all devices support BGRA render buffers (note that on desktop
  // BGRA internal formats are converted to RGBA in the API implementation).
  // For ES, there is no extension that exposes BGRA renderbuffers, however
  // Angle does support these.
  bool enable_render_buffer_bgra =
#if BUILDFLAG(IS_IOS)
      // BGRA is not supported on iOS with ANGLE + GL.
      gl_version_info_->is_angle_metal;
#else
      gl_version_info_->is_angle;
#endif  // BUILDFLAG(IS_IOS)

  if (enable_render_buffer_bgra) {
    feature_flags_.ext_render_buffer_format_bgra8888 = true;
    AddExtensionString("GL_CHROMIUM_renderbuffer_format_BGRA8888");
    validators_.render_buffer_format.AddValue(GL_BGRA8_EXT);
  }

  // On desktop, all devices support BGRA readback since OpenGL 2.0, which we
  // require. On ES, support is indicated by the GL_EXT_read_format_bgra
  // extension.
  bool enable_read_format_bgra =
      gfx::HasExtension(extensions, "GL_EXT_read_format_bgra");

  if (enable_read_format_bgra) {
    feature_flags_.ext_read_format_bgra = true;
    AddExtensionString("GL_EXT_read_format_bgra");
    validators_.read_pixel_format.AddValue(GL_BGRA_EXT);
  }

  // glGetInteger64v for timestamps is implemented on the client side in a way
  // that it does not depend on a driver-level implementation of
  // glGetInteger64v. The GPUTimer class which implements timer queries can also
  // fallback to an implementation that does not depend on glGetInteger64v on
  // ES2. Thus we can enable GL_EXT_disjoint_timer_query on ES2 contexts even
  // though it does not support glGetInteger64v due to a specification bug.
  if (gfx::HasExtension(extensions, "GL_EXT_disjoint_timer_query")) {
    feature_flags_.ext_disjoint_timer_query = true;
    AddExtensionString("GL_EXT_disjoint_timer_query");
  }

  if (gfx::HasExtension(extensions, "GL_OES_rgb8_rgba8")) {
    AddExtensionString("GL_OES_rgb8_rgba8");
    validators_.render_buffer_format.AddValue(GL_RGB8_OES);
    validators_.render_buffer_format.AddValue(GL_RGBA8_OES);
  }

  // Check if we should allow GL_OES_texture_npot
  if (!disallowed_features_.npot_support &&
      (gl_version_info_->is_es3 ||
       gfx::HasExtension(extensions, "GL_OES_texture_npot"))) {
    AddExtensionString("GL_OES_texture_npot");
    feature_flags_.npot_ok = true;
  }

  InitializeFloatAndHalfFloatFeatures(extensions);

  // Check for multisample support
  if (!workarounds_.disable_chromium_framebuffer_multisample) {
    bool ext_has_multisample =
        (gfx::HasExtension(extensions, "GL_EXT_framebuffer_multisample") &&
         gfx::HasExtension(extensions, "GL_EXT_framebuffer_blit")) ||
        gl_version_info_->is_es3;
    if (gl_version_info_->is_angle || gl_version_info_->is_swiftshader) {
      ext_has_multisample |=
          gfx::HasExtension(extensions, "GL_ANGLE_framebuffer_multisample");
    }
    if (ext_has_multisample) {
      feature_flags_.chromium_framebuffer_multisample = true;
      validators_.framebuffer_target.AddValue(GL_READ_FRAMEBUFFER_EXT);
      validators_.framebuffer_target.AddValue(GL_DRAW_FRAMEBUFFER_EXT);
      validators_.g_l_state.AddValue(GL_READ_FRAMEBUFFER_BINDING_EXT);
      validators_.g_l_state.AddValue(GL_MAX_SAMPLES_EXT);
      validators_.render_buffer_parameter.AddValue(GL_RENDERBUFFER_SAMPLES_EXT);
      AddExtensionString("GL_CHROMIUM_framebuffer_multisample");
    }
  }

  if (gfx::HasExtension(extensions, "GL_EXT_multisampled_render_to_texture")) {
    feature_flags_.multisampled_render_to_texture = true;
  } else if (gfx::HasExtension(extensions,
                               "GL_IMG_multisampled_render_to_texture")) {
    feature_flags_.multisampled_render_to_texture = true;
    feature_flags_.use_img_for_multisampled_render_to_texture = true;
  }
  if (feature_flags_.multisampled_render_to_texture) {
    validators_.render_buffer_parameter.AddValue(GL_RENDERBUFFER_SAMPLES_EXT);
    validators_.g_l_state.AddValue(GL_MAX_SAMPLES_EXT);
    validators_.framebuffer_attachment_parameter.AddValue(
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT);
    AddExtensionString("GL_EXT_multisampled_render_to_texture");
  }

  if (gfx::HasExtension(extensions, "GL_EXT_multisample_compatibility")) {
    AddExtensionString("GL_EXT_multisample_compatibility");
    feature_flags_.ext_multisample_compatibility = true;
    validators_.capability.AddValue(GL_MULTISAMPLE_EXT);
    validators_.capability.AddValue(GL_SAMPLE_ALPHA_TO_ONE_EXT);
  }

  if (gfx::HasExtension(extensions, "GL_OES_depth24") ||
      gl_version_info_->is_es3) {
    AddExtensionString("GL_OES_depth24");
    feature_flags_.oes_depth24 = true;
    validators_.render_buffer_format.AddValue(GL_DEPTH_COMPONENT24);
  }

  if (gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_OES_standard_derivatives")) {
    AddExtensionString("GL_OES_standard_derivatives");
    feature_flags_.oes_standard_derivatives = true;
    validators_.hint_target.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
    validators_.g_l_state.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
  }

  if (gfx::HasExtension(extensions, "GL_OES_EGL_image_external")) {
    AddExtensionString("GL_OES_EGL_image_external");
    feature_flags_.oes_egl_image_external = true;

    // In many places we check oes_egl_image_external to know whether
    // TEXTURE_EXTERNAL_OES is valid. Drivers with the _essl3 version *should*
    // have both. But to be safe, only enable the _essl3 version if the
    // non-_essl3 version is available.
    if (gfx::HasExtension(extensions, "GL_OES_EGL_image_external_essl3")) {
      AddExtensionString("GL_OES_EGL_image_external_essl3");
      feature_flags_.oes_egl_image_external_essl3 = true;
    }
  }
  if (gfx::HasExtension(extensions, "GL_NV_EGL_stream_consumer_external")) {
    AddExtensionString("GL_NV_EGL_stream_consumer_external");
    feature_flags_.nv_egl_stream_consumer_external = true;
  }

  if (feature_flags_.oes_egl_image_external ||
      feature_flags_.nv_egl_stream_consumer_external) {
    validators_.texture_bind_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    validators_.get_tex_param_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    validators_.texture_parameter.AddValue(GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES);
    validators_.g_l_state.AddValue(GL_TEXTURE_BINDING_EXTERNAL_OES);
  }

  if (gfx::HasExtension(extensions, "GL_EXT_YUV_target")) {
    validators_.texture_fbo_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    feature_flags_.ext_yuv_target = true;
  }

  // ANGLE only exposes this extension when it has native support of the
  // GL_ETC1_RGB8 format.
  if (gfx::HasExtension(extensions, "GL_OES_compressed_ETC1_RGB8_texture")) {
    AddExtensionString("GL_OES_compressed_ETC1_RGB8_texture");
    feature_flags_.oes_compressed_etc1_rgb8_texture = true;
    validators_.compressed_texture_format.AddValue(GL_ETC1_RGB8_OES);
    validators_.texture_internal_format_storage.AddValue(GL_ETC1_RGB8_OES);
  }

  // Expose GL_ANGLE_compressed_texture_etc when ANGLE exposes it directly or
  // running on top of a non-ANGLE ES driver. We assume that this implies native
  // support of these formats.
  if (gfx::HasExtension(extensions, "GL_ANGLE_compressed_texture_etc") ||
      (gl_version_info_->is_es3 && !gl_version_info_->is_angle)) {
    AddExtensionString("GL_ANGLE_compressed_texture_etc");
    validators_.UpdateETCCompressedTextureFormats();
  }

  if (gfx::HasExtension(extensions, "GL_AMD_compressed_ATC_texture")) {
    AddExtensionString("GL_AMD_compressed_ATC_texture");
    validators_.compressed_texture_format.AddValue(GL_ATC_RGB_AMD);
    validators_.compressed_texture_format.AddValue(
        GL_ATC_RGBA_EXPLICIT_ALPHA_AMD);
    validators_.compressed_texture_format.AddValue(
        GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD);

    validators_.texture_internal_format_storage.AddValue(GL_ATC_RGB_AMD);
    validators_.texture_internal_format_storage.AddValue(
        GL_ATC_RGBA_EXPLICIT_ALPHA_AMD);
    validators_.texture_internal_format_storage.AddValue(
        GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD);
  }

  if (gfx::HasExtension(extensions, "GL_IMG_texture_compression_pvrtc")) {
    AddExtensionString("GL_IMG_texture_compression_pvrtc");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG);

    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG);
    validators_.texture_internal_format_storage.AddValue(
        GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG);
  }

  // Ideally we would only expose this extension on Mac OS X, to support
  // IOSurface backed textures. We don't want applications to start using it;
  // they should use ordinary non-power-of-two textures. However, for unit
  // testing purposes we expose it on all supported platforms.
  if (gfx::HasExtension(extensions, "GL_ANGLE_texture_rectangle")) {
    AddExtensionString("GL_ARB_texture_rectangle");
    feature_flags_.arb_texture_rectangle = true;
    // Rectangle textures are used as samplers via glBindTexture, framebuffer
    // textures via glFramebufferTexture2D, and copy destinations via
    // glCopyPixels.
    validators_.texture_bind_target.AddValue(GL_TEXTURE_RECTANGLE_ARB);
    validators_.texture_fbo_target.AddValue(GL_TEXTURE_RECTANGLE_ARB);
    validators_.texture_target.AddValue(GL_TEXTURE_RECTANGLE_ARB);
    validators_.get_tex_param_target.AddValue(GL_TEXTURE_RECTANGLE_ARB);
    validators_.g_l_state.AddValue(GL_TEXTURE_BINDING_RECTANGLE_ARB);
  }

  if (feature_flags_.chromium_image_ycbcr_420v) {
    AddExtensionString("GL_CHROMIUM_ycbcr_420v_image");
    feature_flags_.gpu_memory_buffer_formats.Put(
        gfx::BufferFormat::YUV_420_BIPLANAR);
  }

#if BUILDFLAG(IS_APPLE)
  // Macs can create SharedImages out of AR30 IOSurfaces. iOS based devices seem
  // to handle well also.
  feature_flags_.chromium_image_ar30 = true;
#elif !BUILDFLAG(IS_WIN)
  // TODO(mcasas): connect in Windows, https://crbug.com/803451
  // XB30 support was introduced in GLES 3.0/ OpenGL 3.3, before that it was
  // signalled via a specific extension.
  feature_flags_.chromium_image_ab30 =
      gl_version_info_->IsAtLeastGLES(3, 0) ||
      gfx::HasExtension(extensions, "GL_EXT_texture_type_2_10_10_10_REV");
#endif
  if (feature_flags_.chromium_image_ar30 ||
      feature_flags_.chromium_image_ab30) {
    validators_.texture_internal_format.AddValue(GL_RGB10_A2_EXT);
    validators_.render_buffer_format.AddValue(GL_RGB10_A2_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGB10_A2_EXT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_INT_2_10_10_10_REV);
  }
  if (feature_flags_.chromium_image_ar30) {
    feature_flags_.gpu_memory_buffer_formats.Put(
        gfx::BufferFormat::BGRA_1010102);
  }
  if (feature_flags_.chromium_image_ab30) {
    feature_flags_.gpu_memory_buffer_formats.Put(
        gfx::BufferFormat::RGBA_1010102);
  }

  if (feature_flags_.chromium_image_ycbcr_p010) {
    AddExtensionString("GL_CHROMIUM_ycbcr_p010_image");
    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::P010);
  }

#if BUILDFLAG(IS_MAC)
  feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::RGBA_F16);
  if (base::mac::MacOSMajorVersion() >= 11) {
    feature_flags_.gpu_memory_buffer_formats.Put(
        gfx::BufferFormat::YUVA_420_TRIPLANAR);
  }
#endif  // BUILDFLAG(IS_MAC)

  // TODO(gman): Add support for these extensions.
  //     GL_OES_depth32

  if (gfx::HasExtension(extensions, "GL_ANGLE_texture_usage")) {
    feature_flags_.angle_texture_usage = true;
    AddExtensionString("GL_ANGLE_texture_usage");
    validators_.texture_parameter.AddValue(GL_TEXTURE_USAGE_ANGLE);
  }

  bool have_occlusion_query = gl_version_info_->IsAtLeastGLES(3, 0);
  bool have_ext_occlusion_query_boolean =
      gfx::HasExtension(extensions, "GL_EXT_occlusion_query_boolean");

  if (have_occlusion_query || have_ext_occlusion_query_boolean) {
    if (context_type_ == CONTEXT_TYPE_OPENGLES2) {
      AddExtensionString("GL_EXT_occlusion_query_boolean");
    }
    feature_flags_.occlusion_query_boolean = true;
  }

  EnableANGLEInstancedArrayIfPossible(extensions);

  bool have_es2_draw_buffers_vendor_agnostic =
      gfx::HasExtension(extensions, "GL_EXT_draw_buffers");
  bool can_emulate_es2_draw_buffers_on_es3_nv =
      gl_version_info_->is_es3 &&
      gfx::HasExtension(extensions, "GL_NV_draw_buffers");
  bool is_webgl_compatibility_context =
      gfx::HasExtension(extensions, "GL_ANGLE_webgl_compatibility");
  bool have_es2_draw_buffers =
      (have_es2_draw_buffers_vendor_agnostic ||
       can_emulate_es2_draw_buffers_on_es3_nv) &&
      (context_type_ == CONTEXT_TYPE_OPENGLES2 ||
       (context_type_ == CONTEXT_TYPE_WEBGL1 &&
        IsWebGLDrawBuffersSupported(is_webgl_compatibility_context,
                                    depth_texture_format,
                                    depth_stencil_texture_format)));
  if (have_es2_draw_buffers) {
    AddExtensionString("GL_EXT_draw_buffers");
    feature_flags_.ext_draw_buffers = true;

    // This flag is set to enable emulation of EXT_draw_buffers when we're
    // running on GLES 3.0+, NV_draw_buffers extension is supported and
    // glDrawBuffers from GLES 3.0 core has been bound. It toggles using the
    // NV_draw_buffers extension directive instead of EXT_draw_buffers extension
    // directive in ESSL 100 shaders translated by ANGLE, enabling them to write
    // into multiple gl_FragData values, which is not by default possible in
    // ESSL 100 with core GLES 3.0. For more information, see the
    // NV_draw_buffers specification.
    feature_flags_.nv_draw_buffers = can_emulate_es2_draw_buffers_on_es3_nv &&
                                     !have_es2_draw_buffers_vendor_agnostic;
  }

  if (IsWebGL2OrES3OrHigherContext() || have_es2_draw_buffers) {
    GLint max_color_attachments = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &max_color_attachments);
    for (GLenum i = GL_COLOR_ATTACHMENT1_EXT;
         i < static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + max_color_attachments);
         ++i) {
      validators_.attachment.AddValue(i);
      validators_.attachment_query.AddValue(i);
    }
    static_assert(GL_COLOR_ATTACHMENT0_EXT == GL_COLOR_ATTACHMENT0,
                  "GL_COLOR_ATTACHMENT0_EXT should equal GL_COLOR_ATTACHMENT0");

    validators_.g_l_state.AddValue(GL_MAX_COLOR_ATTACHMENTS_EXT);
    validators_.g_l_state.AddValue(GL_MAX_DRAW_BUFFERS_ARB);
    GLint max_draw_buffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &max_draw_buffers);
    for (GLenum i = GL_DRAW_BUFFER0_ARB;
         i < static_cast<GLenum>(GL_DRAW_BUFFER0_ARB + max_draw_buffers); ++i) {
      validators_.g_l_state.AddValue(i);
    }
  }

  if (gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_EXT_blend_minmax")) {
    AddExtensionString("GL_EXT_blend_minmax");
    validators_.equation.AddValue(GL_MIN_EXT);
    validators_.equation.AddValue(GL_MAX_EXT);
    static_assert(GL_MIN_EXT == GL_MIN && GL_MAX_EXT == GL_MAX,
                  "min & max variations must match");
  }

  // TODO(dshwang): GLES3 supports gl_FragDepth, not gl_FragDepthEXT.
  if (gfx::HasExtension(extensions, "GL_EXT_frag_depth")) {
    AddExtensionString("GL_EXT_frag_depth");
    feature_flags_.ext_frag_depth = true;
  }

  if (gfx::HasExtension(extensions, "GL_EXT_shader_texture_lod")) {
    AddExtensionString("GL_EXT_shader_texture_lod");
    feature_flags_.ext_shader_texture_lod = true;
  }

  bool ui_gl_fence_works = gl::GLFence::IsSupported();

  feature_flags_.map_buffer_range =
      gl_version_info_->is_es3 ||
      gfx::HasExtension(extensions, "GL_EXT_map_buffer_range");

  // We will use either glMapBuffer() or glMapBufferRange() for async readbacks.
  if (has_pixel_buffers && ui_gl_fence_works &&
      !workarounds_.disable_async_readpixels) {
    feature_flags_.use_async_readpixels = true;
  }

  if (gl_version_info_->is_es3) {
    feature_flags_.enable_samplers = true;
    // TODO(dsinclair): Add AddExtensionString("GL_CHROMIUM_sampler_objects")
    // when available.
  }

  if ((gl_version_info_->is_es3 ||
       gfx::HasExtension(extensions, "GL_EXT_discard_framebuffer")) &&
      !workarounds_.disable_discard_framebuffer) {
    // DiscardFramebufferEXT is automatically bound to InvalidateFramebuffer.
    AddExtensionString("GL_EXT_discard_framebuffer");
    feature_flags_.ext_discard_framebuffer = true;
  }

  if (ui_gl_fence_works) {
    AddExtensionString("GL_CHROMIUM_sync_query");
    feature_flags_.chromium_sync_query = true;
  }

  if (!workarounds_.disable_blend_equation_advanced) {
    bool blend_equation_advanced_coherent =
        gfx::HasExtension(extensions,
                          "GL_NV_blend_equation_advanced_coherent") ||
        gfx::HasExtension(extensions,
                          "GL_KHR_blend_equation_advanced_coherent");

    if (blend_equation_advanced_coherent ||
        gfx::HasExtension(extensions, "GL_NV_blend_equation_advanced") ||
        gfx::HasExtension(extensions, "GL_KHR_blend_equation_advanced")) {
      const GLenum equations[] = {
          GL_MULTIPLY_KHR,       GL_SCREEN_KHR,    GL_OVERLAY_KHR,
          GL_DARKEN_KHR,         GL_LIGHTEN_KHR,   GL_COLORDODGE_KHR,
          GL_COLORBURN_KHR,      GL_HARDLIGHT_KHR, GL_SOFTLIGHT_KHR,
          GL_DIFFERENCE_KHR,     GL_EXCLUSION_KHR, GL_HSL_HUE_KHR,
          GL_HSL_SATURATION_KHR, GL_HSL_COLOR_KHR, GL_HSL_LUMINOSITY_KHR};

      for (GLenum equation : equations)
        validators_.equation.AddValue(equation);
      if (blend_equation_advanced_coherent)
        AddExtensionString("GL_KHR_blend_equation_advanced_coherent");

      AddExtensionString("GL_KHR_blend_equation_advanced");
      feature_flags_.blend_equation_advanced = true;
      feature_flags_.blend_equation_advanced_coherent =
          blend_equation_advanced_coherent;
    }
  }

  if ((gl_version_info_->is_es3 ||
       gfx::HasExtension(extensions, "GL_EXT_texture_rg")) &&
      IsGL_REDSupportedOnFBOs()) {
    feature_flags_.ext_texture_rg = true;
    AddExtensionString("GL_EXT_texture_rg");

    validators_.texture_format.AddValue(GL_RED_EXT);
    validators_.texture_format.AddValue(GL_RG_EXT);

    validators_.texture_internal_format.AddValue(GL_RED_EXT);
    validators_.texture_internal_format.AddValue(GL_R8_EXT);
    validators_.texture_internal_format.AddValue(GL_RG_EXT);
    validators_.texture_internal_format.AddValue(GL_RG8_EXT);

    validators_.read_pixel_format.AddValue(GL_RED_EXT);
    validators_.read_pixel_format.AddValue(GL_RG_EXT);

    validators_.render_buffer_format.AddValue(GL_R8_EXT);
    validators_.render_buffer_format.AddValue(GL_RG8_EXT);

    validators_.texture_unsized_internal_format.AddValue(GL_RED_EXT);
    validators_.texture_unsized_internal_format.AddValue(GL_RG_EXT);

    validators_.texture_internal_format_storage.AddValue(GL_R8_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RG8_EXT);

    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::R_8);
    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::RG_88);
  }

  const bool is_texture_norm16_supported_for_webgl2_or_es3 =
      IsWebGL2OrES3OrHigherContext() &&
      gfx::HasExtension(extensions, "GL_EXT_texture_norm16");
  const bool is_texture_norm16_supported_for_angle_es2 =
      is_passthrough_cmd_decoder_ && context_type_ == CONTEXT_TYPE_OPENGLES2 &&
      gfx::HasExtension(extensions, "GL_EXT_texture_norm16");
  if (is_texture_norm16_supported_for_webgl2_or_es3 ||
      is_texture_norm16_supported_for_angle_es2) {
    AddExtensionString("GL_EXT_texture_norm16");
    feature_flags_.ext_texture_norm16 = true;

    validators_.pixel_type.AddValue(GL_UNSIGNED_SHORT);
    validators_.pixel_type.AddValue(GL_SHORT);

    validators_.texture_format.AddValue(GL_RED_EXT);
    validators_.texture_format.AddValue(GL_RG_EXT);

    validators_.texture_internal_format.AddValue(GL_R16_EXT);
    validators_.texture_internal_format.AddValue(GL_RG16_EXT);
    validators_.texture_internal_format.AddValue(GL_RGB16_EXT);
    validators_.texture_internal_format.AddValue(GL_RGBA16_EXT);
    validators_.texture_internal_format.AddValue(GL_R16_SNORM_EXT);
    validators_.texture_internal_format.AddValue(GL_RG16_SNORM_EXT);
    validators_.texture_internal_format.AddValue(GL_RGB16_SNORM_EXT);
    validators_.texture_internal_format.AddValue(GL_RGBA16_SNORM_EXT);
    validators_.texture_internal_format.AddValue(GL_RED_EXT);

    validators_.read_pixel_format.AddValue(GL_R16_EXT);
    validators_.read_pixel_format.AddValue(GL_RG16_EXT);
    validators_.read_pixel_format.AddValue(GL_RGBA16_EXT);

    validators_.render_buffer_format.AddValue(GL_R16_EXT);
    validators_.render_buffer_format.AddValue(GL_RG16_EXT);
    validators_.render_buffer_format.AddValue(GL_RGBA16_EXT);

    validators_.texture_unsized_internal_format.AddValue(GL_RED_EXT);
    validators_.texture_unsized_internal_format.AddValue(GL_RG_EXT);

    validators_.texture_internal_format_storage.AddValue(GL_R16_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RG16_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGB16_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGBA16_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_R16_SNORM_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RG16_SNORM_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGB16_SNORM_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGBA16_SNORM_EXT);

    validators_.texture_sized_color_renderable_internal_format.AddValue(
        GL_R16_EXT);
    validators_.texture_sized_color_renderable_internal_format.AddValue(
        GL_RG16_EXT);
    validators_.texture_sized_color_renderable_internal_format.AddValue(
        GL_RGBA16_EXT);

    // TODO(shrekshao): gpu_memory_buffer_formats is not used by WebGL
    // So didn't expose all buffer formats here.
    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::R_16);
    feature_flags_.gpu_memory_buffer_formats.Put(gfx::BufferFormat::RG_1616);
  }

  if (enable_es3 && gfx::HasExtension(extensions, "GL_EXT_window_rectangles")) {
    AddExtensionString("GL_EXT_window_rectangles");
    feature_flags_.ext_window_rectangles = true;
    validators_.g_l_state.AddValue(GL_WINDOW_RECTANGLE_MODE_EXT);
    validators_.g_l_state.AddValue(GL_MAX_WINDOW_RECTANGLES_EXT);
    validators_.g_l_state.AddValue(GL_NUM_WINDOW_RECTANGLES_EXT);
    validators_.indexed_g_l_state.AddValue(GL_WINDOW_RECTANGLE_EXT);
  }

  if (!disable_shader_translator_ && gl_version_info_->IsAtLeastGLES(3, 0) &&
      gfx::HasExtension(extensions, "GL_EXT_blend_func_extended")) {
    // Note: to simplify the code, we do not expose EXT_blend_func_extended
    // unless the service context supports ES 3.0. This means the theoretical ES
    // 2.0 implementation with EXT_blend_func_extended is not sufficient.
    feature_flags_.ext_blend_func_extended = true;
    AddExtensionString("GL_EXT_blend_func_extended");

    // NOTE: SRC_ALPHA_SATURATE is valid for ES2 src blend factor.
    // SRC_ALPHA_SATURATE is valid for ES3 src and dst blend factor.
    validators_.dst_blend_factor.AddValue(GL_SRC_ALPHA_SATURATE_EXT);

    validators_.src_blend_factor.AddValue(GL_SRC1_ALPHA_EXT);
    validators_.dst_blend_factor.AddValue(GL_SRC1_ALPHA_EXT);
    validators_.src_blend_factor.AddValue(GL_SRC1_COLOR_EXT);
    validators_.dst_blend_factor.AddValue(GL_SRC1_COLOR_EXT);
    validators_.src_blend_factor.AddValue(GL_ONE_MINUS_SRC1_COLOR_EXT);
    validators_.dst_blend_factor.AddValue(GL_ONE_MINUS_SRC1_COLOR_EXT);
    validators_.src_blend_factor.AddValue(GL_ONE_MINUS_SRC1_ALPHA_EXT);
    validators_.dst_blend_factor.AddValue(GL_ONE_MINUS_SRC1_ALPHA_EXT);
    validators_.g_l_state.AddValue(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT);
  }

  feature_flags_.angle_robust_client_memory =
      gfx::HasExtension(extensions, "GL_ANGLE_robust_client_memory");

  feature_flags_.khr_debug = gl_version_info_->IsAtLeastGLES(3, 2) ||
                             gfx::HasExtension(extensions, "GL_KHR_debug");

  feature_flags_.chromium_gpu_fence = gl::GLFence::IsGpuFenceSupported();
  if (feature_flags_.chromium_gpu_fence)
    AddExtensionString("GL_CHROMIUM_gpu_fence");

  feature_flags_.chromium_bind_generates_resource =
      gfx::HasExtension(extensions, "GL_CHROMIUM_bind_generates_resource");
  feature_flags_.angle_webgl_compatibility = is_webgl_compatibility_context;
  feature_flags_.chromium_copy_texture =
      gfx::HasExtension(extensions, "GL_CHROMIUM_copy_texture");
  feature_flags_.chromium_copy_compressed_texture =
      gfx::HasExtension(extensions, "GL_CHROMIUM_copy_compressed_texture");
  feature_flags_.angle_client_arrays =
      gfx::HasExtension(extensions, "GL_ANGLE_client_arrays");
  feature_flags_.angle_request_extension =
      gfx::HasExtension(extensions, "GL_ANGLE_request_extension");
  feature_flags_.ext_pixel_buffer_object =
      gfx::HasExtension(extensions, "GL_NV_pixel_buffer_object");
  feature_flags_.ext_unpack_subimage =
      gfx::HasExtension(extensions, "GL_EXT_unpack_subimage");
  feature_flags_.oes_rgb8_rgba8 =
      gfx::HasExtension(extensions, "GL_OES_rgb8_rgba8");
  feature_flags_.angle_robust_resource_initialization =
      gfx::HasExtension(extensions, "GL_ANGLE_robust_resource_initialization");
  feature_flags_.nv_fence = gfx::HasExtension(extensions, "GL_NV_fence");

  if (gfx::HasExtension(extensions, "GL_EXT_debug_marker")) {
    feature_flags_.ext_debug_marker = true;
    AddExtensionString("GL_EXT_debug_marker");
  }

  // On D3D, there is only one ref, mask, and writemask setting which applies
  // to both FRONT and BACK faces. This restriction is always applied to WebGL.
  // https://crbug.com/806557
  // https://github.com/KhronosGroup/WebGL/pull/2583
  feature_flags_.separate_stencil_ref_mask_writemask =
      !(gl_version_info_->is_d3d) && !IsWebGLContext();

  if (gfx::HasExtension(extensions, "GL_MESA_framebuffer_flip_y")) {
    feature_flags_.mesa_framebuffer_flip_y = true;
    validators_.framebuffer_parameter.AddValue(GL_FRAMEBUFFER_FLIP_Y_MESA);
    AddExtensionString("GL_MESA_framebuffer_flip_y");
  }

  // Only supporting OVR_multiview in passthrough mode - not implemented in
  // validating command decoder. The extension is only available in ANGLE and in
  // that case Chromium should be using passthrough by default.
  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_OVR_multiview2")) {
    AddExtensionString("GL_OVR_multiview2");
    feature_flags_.ovr_multiview2 = true;
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_KHR_parallel_shader_compile")) {
    AddExtensionString("GL_KHR_parallel_shader_compile");
    feature_flags_.khr_parallel_shader_compile = true;
    validators_.g_l_state.AddValue(GL_MAX_SHADER_COMPILER_THREADS_KHR);
    // TODO(jie.a.chen@intel.com): Make the query as cheap as possible.
    // https://crbug.com/881152
    validators_.shader_parameter.AddValue(GL_COMPLETION_STATUS_KHR);
    validators_.program_parameter.AddValue(GL_COMPLETION_STATUS_KHR);

    AddExtensionString("GL_CHROMIUM_completion_query");
    feature_flags_.chromium_completion_query = true;
  }

  if (gfx::HasExtension(extensions, "GL_KHR_robust_buffer_access_behavior")) {
    AddExtensionString("GL_KHR_robust_buffer_access_behavior");
    feature_flags_.khr_robust_buffer_access_behavior = true;
  }

  EnableWEBGLMultiDrawIfPossible(extensions);

#if BUILDFLAG(IS_MAC)
  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_ANGLE_base_vertex_base_instance")) {
#else
  if ((!is_passthrough_cmd_decoder_ &&
       ((gl_version_info_->IsAtLeastGLES(3, 2) &&
         gfx::HasExtension(extensions, "GL_EXT_base_instance")))) ||
      gfx::HasExtension(extensions, "GL_ANGLE_base_vertex_base_instance")) {
#endif
    // TODO(shrekshao): change condition to the following after workaround for
    // Mac AMD and non-native base instance support are implemented, or when
    // angle is universally used.
    //
    // if ((!is_passthrough_cmd_decoder_ &&
    //      ((gl_version_info_->IsAtLeastGLES(3, 2) ||
    //        gfx::HasExtension(extensions,
    //          "GL_OES_draw_elements_base_vertex_base_instance") ||
    //        gfx::HasExtension(extensions,
    //          "GL_EXT_draw_elements_base_vertex_base_instance")) ||
    //       (gl_version_info_->is_desktop_core_profile &&
    //        gl_version_info_->IsAtLeastGL(3, 2)))) ||
    //     gfx::HasExtension(extensions, "GL_ANGLE_base_vertex_base_instance"))
    //     {
    feature_flags_.webgl_draw_instanced_base_vertex_base_instance = true;
    AddExtensionString("GL_WEBGL_draw_instanced_base_vertex_base_instance");
    if (feature_flags_.webgl_multi_draw) {
      feature_flags_.webgl_multi_draw_instanced_base_vertex_base_instance =
          true;
      AddExtensionString(
          "GL_WEBGL_multi_draw_instanced_base_vertex_base_instance");
    }
  }

  if (gfx::HasExtension(extensions, "GL_NV_internalformat_sample_query")) {
    feature_flags_.nv_internalformat_sample_query = true;
  }

  if (gfx::HasExtension(extensions,
                        "GL_AMD_framebuffer_multisample_advanced")) {
    feature_flags_.amd_framebuffer_multisample_advanced = true;
    AddExtensionString("GL_AMD_framebuffer_multisample_advanced");
  }

  if (gfx::HasExtension(extensions, "GL_ANGLE_shader_pixel_local_storage")) {
    feature_flags_.angle_shader_pixel_local_storage = true;
    AddExtensionString("GL_ANGLE_shader_pixel_local_storage");
  }

  if (gfx::HasExtension(extensions,
                        "GL_ANGLE_shader_pixel_local_storage_coherent")) {
    AddExtensionString("GL_ANGLE_shader_pixel_local_storage_coherent");
  }

  if (gfx::HasExtension(extensions, "GL_ANGLE_rgbx_internal_format")) {
    feature_flags_.angle_rgbx_internal_format = true;
    AddExtensionString("GL_ANGLE_rgbx_internal_format");
    validators_.texture_internal_format_storage.AddValue(GL_RGBX8_ANGLE);
  }

  if (gfx::HasExtension(extensions, "GL_ANGLE_provoking_vertex")) {
    feature_flags_.angle_provoking_vertex = true;
    AddExtensionString("GL_ANGLE_provoking_vertex");
  }

  if (gfx::HasExtension(extensions, "GL_ANGLE_clip_cull_distance")) {
    feature_flags_.angle_clip_cull_distance = true;
    AddExtensionString("GL_ANGLE_clip_cull_distance");
  }

  if (gfx::HasExtension(extensions, "GL_ANGLE_polygon_mode")) {
    feature_flags_.angle_polygon_mode = true;
    AddExtensionString("GL_ANGLE_polygon_mode");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_ANGLE_stencil_texturing")) {
    AddExtensionString("GL_ANGLE_stencil_texturing");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_blend_func_extended")) {
    feature_flags_.ext_blend_func_extended = true;
    AddExtensionString("GL_EXT_blend_func_extended");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_clip_control")) {
    feature_flags_.ext_clip_control = true;
    AddExtensionString("GL_EXT_clip_control");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_conservative_depth")) {
    AddExtensionString("GL_EXT_conservative_depth");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_depth_clamp")) {
    AddExtensionString("GL_EXT_depth_clamp");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_polygon_offset_clamp")) {
    feature_flags_.ext_polygon_offset_clamp = true;
    AddExtensionString("GL_EXT_polygon_offset_clamp");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_render_snorm")) {
    AddExtensionString("GL_EXT_render_snorm");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_EXT_texture_mirror_clamp_to_edge")) {
    AddExtensionString("GL_EXT_texture_mirror_clamp_to_edge");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions,
                        "GL_NV_shader_noperspective_interpolation")) {
    AddExtensionString("GL_NV_shader_noperspective_interpolation");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_OES_sample_variables")) {
    AddExtensionString("GL_OES_sample_variables");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions,
                        "GL_OES_shader_multisample_interpolation")) {
    AddExtensionString("GL_OES_shader_multisample_interpolation");
  }

  if (is_passthrough_cmd_decoder_ &&
      gfx::HasExtension(extensions, "GL_QCOM_render_shared_exponent")) {
    AddExtensionString("GL_QCOM_render_shared_exponent");
  }
}

void FeatureInfo::InitializeFloatAndHalfFloatFeatures(
    const gfx::ExtensionSet& extensions) {
  // Check if we should allow GL_OES_texture_float, GL_OES_texture_half_float,
  // GL_OES_texture_float_linear, GL_OES_texture_half_float_linear
  bool enable_texture_float = false;
  bool enable_texture_float_linear = false;
  bool enable_texture_half_float = false;
  bool enable_texture_half_float_linear = false;
  bool enable_ext_color_buffer_float = false;
  bool enable_ext_color_buffer_half_float = false;

  bool may_enable_chromium_color_buffer_float = false;

  bool enable_es3 = IsWebGL2OrES3OrHigherContext();

  // These extensions allow a variety of floating point formats to be
  // rendered to via framebuffer objects.
  if (gfx::HasExtension(extensions, "GL_EXT_color_buffer_float"))
    enable_ext_color_buffer_float = true;
  if (gfx::HasExtension(extensions, "GL_EXT_color_buffer_half_float")) {
    // TODO(zmo): even if the underlying driver reports the extension, WebGL
    // version of the extension requires RGBA16F to be color-renderable,
    // whereas the OpenGL ES extension only requires one of the format to be
    // color-renderable. So we still need to do some verification before
    // exposing the extension.
    enable_ext_color_buffer_half_float = true;
  }

  // GLES3 adds support for Float type by default but it doesn't support all
  // formats as GL_OES_texture_float(i.e.LUMINANCE_ALPHA,LUMINANCE and Alpha)
  if (gfx::HasExtension(extensions, "GL_OES_texture_float")) {
    enable_texture_float = true;
    if (enable_ext_color_buffer_float) {
      may_enable_chromium_color_buffer_float = true;
    }
  }

  if (gfx::HasExtension(extensions, "GL_OES_texture_float_linear")) {
    enable_texture_float_linear = true;
  }

  // TODO(dshwang): GLES3 supports half float by default but GL_HALF_FLOAT_OES
  // isn't equal to GL_HALF_FLOAT.
  if (gfx::HasExtension(extensions, "GL_OES_texture_half_float")) {
    enable_texture_half_float = true;
  }

  if (gfx::HasExtension(extensions, "GL_OES_texture_half_float_linear")) {
    enable_texture_half_float_linear = true;
  }

  if (enable_texture_float) {
    validators_.pixel_type.AddValue(GL_FLOAT);
    validators_.read_pixel_type.AddValue(GL_FLOAT);
    AddExtensionString("GL_OES_texture_float");
  }

  if (enable_texture_float_linear) {
    oes_texture_float_linear_available_ = true;
    if (!disallowed_features_.oes_texture_float_linear)
      EnableOESTextureFloatLinear();
  }

  if (enable_texture_half_float) {
    oes_texture_float_available_ = true;
    validators_.pixel_type.AddValue(GL_HALF_FLOAT_OES);
    validators_.read_pixel_type.AddValue(GL_HALF_FLOAT_OES);
    AddExtensionString("GL_OES_texture_half_float");
  }

  if (enable_texture_half_float_linear) {
    oes_texture_half_float_linear_available_ = true;
    if (!disallowed_features_.oes_texture_half_float_linear)
      EnableOESTextureHalfFloatLinear();
  }

  bool had_native_chromium_color_buffer_float_ext = false;
  if (gfx::HasExtension(extensions, "GL_CHROMIUM_color_buffer_float_rgb")) {
    had_native_chromium_color_buffer_float_ext = true;
    feature_flags_.chromium_color_buffer_float_rgb = true;
    if (!disallowed_features_.chromium_color_buffer_float_rgb) {
      EnableCHROMIUMColorBufferFloatRGB();
    }
  }

  if (gfx::HasExtension(extensions, "GL_CHROMIUM_color_buffer_float_rgba")) {
    had_native_chromium_color_buffer_float_ext = true;
    feature_flags_.chromium_color_buffer_float_rgba = true;
    if (!disallowed_features_.chromium_color_buffer_float_rgba) {
      EnableCHROMIUMColorBufferFloatRGBA();
    }
  }

  // Floating-point format blending is core of ES 3.2.
  if (gl_version_info_->IsAtLeastGLES(3, 2) ||
      gfx::HasExtension(extensions, "GL_EXT_float_blend")) {
    if (!disallowed_features_.ext_float_blend) {
      EnableEXTFloatBlend();
    }
  }

  if (may_enable_chromium_color_buffer_float &&
      !had_native_chromium_color_buffer_float_ext) {
    if (workarounds_.force_enable_color_buffer_float ||
        workarounds_.force_enable_color_buffer_float_except_rgb32f) {
      if (enable_es3)
        enable_ext_color_buffer_float = true;
      feature_flags_.chromium_color_buffer_float_rgba = true;
      if (!disallowed_features_.chromium_color_buffer_float_rgba)
        EnableCHROMIUMColorBufferFloatRGBA();
      if (!workarounds_.force_enable_color_buffer_float_except_rgb32f) {
        feature_flags_.chromium_color_buffer_float_rgb = true;
        if (!disallowed_features_.chromium_color_buffer_float_rgb)
          EnableCHROMIUMColorBufferFloatRGB();
      }
    } else {
      static_assert(
          GL_RGBA32F_ARB == GL_RGBA32F && GL_RGBA32F_EXT == GL_RGBA32F &&
              GL_RGB32F_ARB == GL_RGB32F && GL_RGB32F_EXT == GL_RGB32F,
          "sized float internal format variations must match");
      // We don't check extension support beyond ARB_texture_float on desktop
      // GL, and format support varies between GL configurations. For example,
      // spec prior to OpenGL 3.0 mandates framebuffer support only for one
      // implementation-chosen format, and ES3.0 EXT_color_buffer_float does not
      // support rendering to RGB32F. Check for framebuffer completeness with
      // formats that the extensions expose, and only enable an extension when a
      // framebuffer created with its texture format is reported as complete.
      GLint fb_binding = 0;
      GLint tex_binding = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fb_binding);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex_binding);

      GLuint tex_id = 0;
      GLuint fb_id = 0;
      GLsizei width = 16;

      glGenTextures(1, &tex_id);
      glGenFramebuffersEXT(1, &fb_id);
      glBindTexture(GL_TEXTURE_2D, tex_id);
      // Nearest filter needed for framebuffer completeness on some drivers.
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, width, 0, GL_RGBA,
                   GL_FLOAT, nullptr);
      glBindFramebufferEXT(GL_FRAMEBUFFER, fb_id);
      glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, tex_id, 0);
      GLenum status_rgba = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, width, 0, GL_RGB,
                   GL_FLOAT, nullptr);
      GLenum status_rgb = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);

      // For desktop systems, check to see if we support rendering to the full
      // range of formats supported by EXT_color_buffer_float
      if (status_rgba == GL_FRAMEBUFFER_COMPLETE && enable_es3) {
        bool full_float_support = true;
        const GLenum kInternalFormats[] = {
            GL_R16F, GL_RG16F, GL_RGBA16F, GL_R32F, GL_RG32F, GL_R11F_G11F_B10F,
        };
        const GLenum kFormats[] = {
            GL_RED, GL_RG, GL_RGBA, GL_RED, GL_RG, GL_RGB,
        };
        DCHECK_EQ(std::size(kInternalFormats), std::size(kFormats));
        for (size_t i = 0; i < std::size(kFormats); ++i) {
          glTexImage2D(GL_TEXTURE_2D, 0, kInternalFormats[i], width, width, 0,
                       kFormats[i], GL_FLOAT, nullptr);
          bool supported = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) ==
                           GL_FRAMEBUFFER_COMPLETE;
          full_float_support &= supported;
        }
        enable_ext_color_buffer_float = full_float_support;
      }
      // Likewise for EXT_color_buffer_half_float on ES2 contexts. On desktop,
      // require at least GL 3.0, to ensure that all formats are defined.
      if (IsWebGL1OrES2Context() && !enable_ext_color_buffer_half_float &&
          gl_version_info_->IsAtLeastGLES(3, 0)) {
        // EXT_color_buffer_half_float requires at least one of the formats is
        // supported to be color-renderable. WebGL's extension requires RGBA16F
        // to be the supported effective format. Here we only expose the
        // extension if RGBA16F is color-renderable.
        GLenum internal_format = GL_RGBA16F;
        GLenum format = GL_RGBA;
        GLenum data_type = GL_HALF_FLOAT;
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, width, 0, format,
                     data_type, nullptr);
        bool supported = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER) ==
                         GL_FRAMEBUFFER_COMPLETE;
        enable_ext_color_buffer_half_float = supported;
      }

      glDeleteFramebuffersEXT(1, &fb_id);
      glDeleteTextures(1, &tex_id);

      glBindFramebufferEXT(GL_FRAMEBUFFER, static_cast<GLuint>(fb_binding));
      glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex_binding));

      DCHECK_EQ(glGetError(), static_cast<GLuint>(GL_NO_ERROR));

      if (status_rgba == GL_FRAMEBUFFER_COMPLETE) {
        feature_flags_.chromium_color_buffer_float_rgba = true;
        if (!disallowed_features_.chromium_color_buffer_float_rgba)
          EnableCHROMIUMColorBufferFloatRGBA();
      }
      if (status_rgb == GL_FRAMEBUFFER_COMPLETE) {
        feature_flags_.chromium_color_buffer_float_rgb = true;
        if (!disallowed_features_.chromium_color_buffer_float_rgb)
          EnableCHROMIUMColorBufferFloatRGB();
      }
    }
  }

  // Enable the GL_EXT_color_buffer_float extension for WebGL 2.0
  if (enable_ext_color_buffer_float && enable_es3) {
    ext_color_buffer_float_available_ = true;
    if (!disallowed_features_.ext_color_buffer_float)
      EnableEXTColorBufferFloat();
  }

  // Enable GL_EXT_color_buffer_half_float if we have found the capability.
  if (enable_ext_color_buffer_half_float) {
    ext_color_buffer_half_float_available_ = true;
    if (!disallowed_features_.ext_color_buffer_half_float)
      EnableEXTColorBufferHalfFloat();
  }

  if (enable_texture_float) {
    validators_.texture_internal_format_storage.AddValue(GL_RGBA32F_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGB32F_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_ALPHA32F_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_LUMINANCE32F_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_LUMINANCE_ALPHA32F_EXT);
  }

  if (enable_texture_half_float) {
    validators_.texture_internal_format_storage.AddValue(GL_RGBA16F_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_RGB16F_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_ALPHA16F_EXT);
    validators_.texture_internal_format_storage.AddValue(GL_LUMINANCE16F_EXT);
    validators_.texture_internal_format_storage.AddValue(
        GL_LUMINANCE_ALPHA16F_EXT);
  }
}

bool FeatureInfo::IsES3Capable() const {
  if (workarounds_.disable_texture_storage)
    return false;
  if (gl_version_info_)
    return gl_version_info_->IsAtLeastGLES(3, 0);
  return false;
}

void FeatureInfo::EnableES3Validators() {
  DCHECK(IsES3Capable());
  validators_.UpdateValuesES3();

  GLint max_color_attachments = 0;
  glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max_color_attachments);
  const int kTotalColorAttachmentEnums = 16;
  const GLenum kColorAttachments[] = {
    GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2,
    GL_COLOR_ATTACHMENT3,
    GL_COLOR_ATTACHMENT4,
    GL_COLOR_ATTACHMENT5,
    GL_COLOR_ATTACHMENT6,
    GL_COLOR_ATTACHMENT7,
    GL_COLOR_ATTACHMENT8,
    GL_COLOR_ATTACHMENT9,
    GL_COLOR_ATTACHMENT10,
    GL_COLOR_ATTACHMENT11,
    GL_COLOR_ATTACHMENT12,
    GL_COLOR_ATTACHMENT13,
    GL_COLOR_ATTACHMENT14,
    GL_COLOR_ATTACHMENT15,
  };
  if (max_color_attachments < kTotalColorAttachmentEnums) {
    validators_.attachment.RemoveValues(
        kColorAttachments + max_color_attachments,
        kTotalColorAttachmentEnums - max_color_attachments);
    validators_.attachment_query.RemoveValues(
        kColorAttachments + max_color_attachments,
        kTotalColorAttachmentEnums - max_color_attachments);
    validators_.read_buffer.RemoveValues(
        kColorAttachments + max_color_attachments,
        kTotalColorAttachmentEnums - max_color_attachments);
  }

  GLint max_draw_buffers = 0;
  glGetIntegerv(GL_MAX_DRAW_BUFFERS, &max_draw_buffers);
  const int kTotalDrawBufferEnums = 16;
  const GLenum kDrawBuffers[] = {
    GL_DRAW_BUFFER0,
    GL_DRAW_BUFFER1,
    GL_DRAW_BUFFER2,
    GL_DRAW_BUFFER3,
    GL_DRAW_BUFFER4,
    GL_DRAW_BUFFER5,
    GL_DRAW_BUFFER6,
    GL_DRAW_BUFFER7,
    GL_DRAW_BUFFER8,
    GL_DRAW_BUFFER9,
    GL_DRAW_BUFFER10,
    GL_DRAW_BUFFER11,
    GL_DRAW_BUFFER12,
    GL_DRAW_BUFFER13,
    GL_DRAW_BUFFER14,
    GL_DRAW_BUFFER15,
  };
  if (max_draw_buffers < kTotalDrawBufferEnums) {
    validators_.g_l_state.RemoveValues(
        kDrawBuffers + max_draw_buffers,
        kTotalDrawBufferEnums - max_draw_buffers);
  }

  if (feature_flags_.ext_texture_format_bgra8888) {
    validators_.texture_internal_format.AddValue(GL_BGRA8_EXT);
    validators_.texture_sized_color_renderable_internal_format.AddValue(
        GL_BGRA8_EXT);
    validators_.texture_sized_texture_filterable_internal_format.AddValue(
        GL_BGRA8_EXT);
  }

  if (!IsWebGLContext()) {
    validators_.texture_parameter.AddValue(GL_TEXTURE_SWIZZLE_R);
    validators_.texture_parameter.AddValue(GL_TEXTURE_SWIZZLE_G);
    validators_.texture_parameter.AddValue(GL_TEXTURE_SWIZZLE_B);
    validators_.texture_parameter.AddValue(GL_TEXTURE_SWIZZLE_A);
  }
}

bool FeatureInfo::IsWebGLContext() const {
  return IsWebGLContextType(context_type_);
}

bool FeatureInfo::IsWebGL1OrES2Context() const {
  return IsWebGL1OrES2ContextType(context_type_);
}

bool FeatureInfo::IsWebGL2OrES3Context() const {
  return IsWebGL2OrES3ContextType(context_type_);
}

bool FeatureInfo::IsWebGL2OrES3OrHigherContext() const {
  return IsWebGL2OrES3OrHigherContextType(context_type_);
}

bool FeatureInfo::IsES31ForTestingContext() const {
  return IsES31ForTestingContextType(context_type_);
}

void FeatureInfo::AddExtensionString(std::string_view extension) {
  extensions_.insert(extension);
}

FeatureInfo::~FeatureInfo() = default;

}  // namespace gles2
}  // namespace gpu
