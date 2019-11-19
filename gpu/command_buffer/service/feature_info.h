// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
#define GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/extension_set.h"

namespace base {
class CommandLine;
}

namespace gl {
struct GLVersionInfo;
}

namespace gpu {
namespace gles2 {

// FeatureInfo records the features that are available for a ContextGroup.
class GPU_GLES2_EXPORT FeatureInfo : public base::RefCounted<FeatureInfo> {
 public:
  struct FeatureFlags {
    FeatureFlags();

    GpuMemoryBufferFormatSet gpu_memory_buffer_formats = {
        gfx::BufferFormat::BGR_565,   gfx::BufferFormat::RGBA_4444,
        gfx::BufferFormat::RGBA_8888, gfx::BufferFormat::RGBX_8888,
        gfx::BufferFormat::YVU_420,
    };
    // Use glBlitFramebuffer() and glRenderbufferStorageMultisample() with
    // GL_EXT_framebuffer_multisample-style semantics (as opposed to
    // GL_EXT_multisampled_render_to_texture semantics).
    bool chromium_framebuffer_multisample = false;
    bool chromium_sync_query = false;
    bool multisampled_render_to_texture = false;
    // Use the IMG GLenum values rather than EXT.
    bool use_img_for_multisampled_render_to_texture = false;
    bool chromium_screen_space_antialiasing = false;
    bool use_chromium_screen_space_antialiasing_via_shaders = false;
    bool oes_standard_derivatives = false;
    bool oes_egl_image_external = false;
    bool nv_egl_stream_consumer_external = false;
    bool oes_depth24 = false;
    bool oes_compressed_etc1_rgb8_texture = false;
    bool packed_depth24_stencil8 = false;
    bool npot_ok = false;
    bool enable_texture_filter_anisotropic = false;
    bool enable_texture_float_linear = false;
    bool enable_texture_half_float_linear = false;
    bool enable_color_buffer_float = false;
    bool enable_color_buffer_half_float = false;
    bool angle_translated_shader_source = false;
    bool angle_pack_reverse_row_order = false;
    bool arb_texture_rectangle = false;
    bool angle_instanced_arrays = false;
    bool occlusion_query = false;
    bool occlusion_query_boolean = false;
    bool use_arb_occlusion_query2_for_occlusion_query_boolean = false;
    bool use_arb_occlusion_query_for_occlusion_query_boolean = false;
    bool native_vertex_array_object = false;
    bool ext_texture_format_astc = false;
    bool ext_texture_format_atc = false;
    bool ext_texture_format_bgra8888 = false;
    bool ext_texture_format_dxt1 = false;
    bool ext_texture_format_dxt5 = false;
    bool enable_shader_name_hashing = false;
    bool enable_samplers = false;
    bool ext_draw_buffers = false;
    bool nv_draw_buffers = false;
    bool ext_frag_depth = false;
    bool ext_shader_texture_lod = false;
    bool use_async_readpixels = false;
    bool map_buffer_range = false;
    bool ext_discard_framebuffer = false;
    bool angle_depth_texture = false;
    bool is_swiftshader_for_webgl = false;
    bool is_swiftshader = false;
    bool chromium_texture_filtering_hint = false;
    bool angle_texture_usage = false;
    bool ext_texture_storage = false;
    bool chromium_path_rendering = false;
    bool chromium_raster_transport = false;
    bool chromium_framebuffer_mixed_samples = false;
    bool blend_equation_advanced = false;
    bool blend_equation_advanced_coherent = false;
    bool ext_texture_rg = false;
    bool ext_texture_norm16 = false;
    bool chromium_image_ycbcr_420v = false;
    bool chromium_image_ycbcr_422 = false;
    bool chromium_image_xr30 = false;
    bool chromium_image_xb30 = false;
    bool chromium_image_ycbcr_p010 = false;
    bool emulate_primitive_restart_fixed_index = false;
    bool ext_render_buffer_format_bgra8888 = false;
    bool ext_multisample_compatibility = false;
    bool ext_blend_func_extended = false;
    bool ext_read_format_bgra = false;
    bool desktop_srgb_support = false;
    bool arb_es3_compatibility = false;
    bool chromium_color_buffer_float_rgb = false;
    bool chromium_color_buffer_float_rgba = false;
    bool angle_robust_client_memory = false;
    bool khr_debug = false;
    bool chromium_bind_generates_resource = false;
    bool angle_webgl_compatibility = false;
    bool ext_srgb_write_control = false;
    bool ext_srgb = false;
    bool chromium_copy_texture = false;
    bool chromium_copy_compressed_texture = false;
    bool ext_disjoint_timer_query = false;
    bool angle_client_arrays = false;
    bool angle_request_extension = false;
    bool ext_debug_marker = false;
    bool ext_pixel_buffer_object = false;
    bool ext_unpack_subimage = false;
    bool oes_rgb8_rgba8 = false;
    bool angle_robust_resource_initialization = false;
    bool nv_fence = false;
    bool chromium_texture_storage_image = false;
    bool ext_window_rectangles = false;
    bool chromium_gpu_fence = false;
    bool unpremultiply_and_dither_copy = false;
    bool separate_stencil_ref_mask_writemask = false;
    bool mesa_framebuffer_flip_y = false;
    bool ovr_multiview2 = false;
    bool khr_parallel_shader_compile = false;
    bool android_surface_control = false;
    bool khr_robust_buffer_access_behavior = false;
    bool webgl_multi_draw = false;
    bool nv_internalformat_sample_query = false;
    bool amd_framebuffer_multisample_advanced = false;
    bool ext_float_blend = false;
    bool chromium_completion_query = false;
    bool oes_fbo_render_mipmap = false;
    bool webgl_draw_instanced_base_vertex_base_instance = false;
    bool webgl_multi_draw_instanced_base_vertex_base_instance = false;
  };

  FeatureInfo();

  // Constructor with workarounds taken from the current process's CommandLine
  FeatureInfo(const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds,
              const GpuFeatureInfo& gpu_feature_info);

  // Initializes the feature information. Needs a current GL context.
  void Initialize(ContextType context_type,
                  bool is_passthrough_cmd_decoder,
                  const DisallowedFeatures& disallowed_features,
                  bool force_reinitialize = false);

  // Helper that defaults to no disallowed features and a GLES2 context.
  void InitializeForTesting();
  // Helper that defaults to no disallowed Features.
  void InitializeForTesting(ContextType context_type);
  // Helper that defaults to a GLES2 context.
  void InitializeForTesting(const DisallowedFeatures& disallowed_features);

  const Validators* validators() const {
    return &validators_;
  }

  ContextType context_type() const { return context_type_; }

  const gfx::ExtensionSet& extensions() const { return extensions_; }

  const FeatureFlags& feature_flags() const {
    return feature_flags_;
  }

  const GpuDriverBugWorkarounds& workarounds() const { return workarounds_; }

  const DisallowedFeatures& disallowed_features() const {
    return disallowed_features_;
  }

  const gl::GLVersionInfo& gl_version_info() const {
    DCHECK(gl_version_info_.get());
    return *(gl_version_info_.get());
  }

  bool IsES3Capable() const;
  void EnableES3Validators();

  bool disable_shader_translator() const { return disable_shader_translator_; }

  bool IsWebGLContext() const;
  bool IsWebGL1OrES2Context() const;
  bool IsWebGL2OrES3Context() const;
  bool IsWebGL2OrES3OrHigherContext() const;
  bool IsWebGL2ComputeContext() const;

  void EnableCHROMIUMTextureStorageImage();
  void EnableCHROMIUMColorBufferFloatRGBA();
  void EnableCHROMIUMColorBufferFloatRGB();
  void EnableEXTFloatBlend();
  void EnableEXTColorBufferFloat();
  void EnableEXTColorBufferHalfFloat();
  void EnableEXTTextureFilterAnisotropic();
  void EnableOESFboRenderMipmap();
  void EnableOESTextureFloatLinear();
  void EnableOESTextureHalfFloatLinear();

  bool ext_color_buffer_float_available() const {
    return ext_color_buffer_float_available_;
  }

  bool ext_color_buffer_half_float_available() const {
    return ext_color_buffer_half_float_available_;
  }

  bool oes_texture_float_linear_available() const {
    return oes_texture_float_linear_available_;
  }

  bool oes_texture_half_float_linear_available() const {
    return oes_texture_half_float_linear_available_;
  }

  bool is_passthrough_cmd_decoder() const {
    return is_passthrough_cmd_decoder_;
  }

 private:
  friend class base::RefCounted<FeatureInfo>;
  friend class BufferManagerClientSideArraysTest;

  ~FeatureInfo();

  void AddExtensionString(const base::StringPiece& s);
  void InitializeBasicState(const base::CommandLine* command_line);
  void InitializeFeatures();
  void InitializeFloatAndHalfFloatFeatures(const gfx::ExtensionSet& extensions);

  bool initialized_ = false;

  Validators validators_;

  DisallowedFeatures disallowed_features_;

  ContextType context_type_ = CONTEXT_TYPE_OPENGLES2;
  bool is_passthrough_cmd_decoder_ = false;

  // The set of extensions returned by glGetString(GL_EXTENSIONS);
  gfx::ExtensionSet extensions_;

  // Flags for some features
  FeatureFlags feature_flags_;

  // Flags for Workarounds.
  GpuDriverBugWorkarounds workarounds_;

  bool ext_color_buffer_float_available_ = false;
  bool ext_color_buffer_half_float_available_ = false;
  bool ext_texture_filter_anisotropic_available_ = false;
  bool oes_texture_float_linear_available_ = false;
  bool oes_texture_half_float_linear_available_ = false;

  bool disable_shader_translator_;
  std::unique_ptr<gl::GLVersionInfo> gl_version_info_;

  DISALLOW_COPY_AND_ASSIGN(FeatureInfo);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
