// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/context_group.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/program_manager.h"
#include "gpu/command_buffer/service/renderbuffer_manager.h"
#include "gpu/command_buffer/service/sampler_manager.h"
#include "gpu/command_buffer/service/service_discardable_manager.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/progress_reporter.h"

namespace gpu {
namespace gles2 {

namespace {

void GetIntegerv(GLenum pname, uint32_t* var) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *var = value;
}

}  // namespace anonymous

DisallowedFeatures AdjustDisallowedFeatures(
    ContextType context_type, const DisallowedFeatures& disallowed_features) {
  DisallowedFeatures adjusted_disallowed_features = disallowed_features;
  if (context_type == CONTEXT_TYPE_WEBGL1) {
    adjusted_disallowed_features.npot_support = true;
  }
  if (context_type == CONTEXT_TYPE_WEBGL1 ||
      context_type == CONTEXT_TYPE_WEBGL2) {
    adjusted_disallowed_features.chromium_color_buffer_float_rgba = true;
    adjusted_disallowed_features.chromium_color_buffer_float_rgb = true;
    adjusted_disallowed_features.ext_color_buffer_float = true;
    adjusted_disallowed_features.oes_texture_float_linear = true;
    adjusted_disallowed_features.ext_color_buffer_half_float = true;
    adjusted_disallowed_features.oes_texture_half_float_linear = true;
    adjusted_disallowed_features.ext_texture_filter_anisotropic = true;
    adjusted_disallowed_features.ext_float_blend = true;
    adjusted_disallowed_features.oes_fbo_render_mipmap = true;
    adjusted_disallowed_features.oes_draw_buffers_indexed = true;
  }
  return adjusted_disallowed_features;
}

ContextGroup::ContextGroup(
    const GpuPreferences& gpu_preferences,
    bool supports_passthrough_command_decoders,
    std::unique_ptr<MemoryTracker> memory_tracker,
    ShaderTranslatorCache* shader_translator_cache,
    FramebufferCompletenessCache* framebuffer_completeness_cache,
    const scoped_refptr<FeatureInfo>& feature_info,
    bool bind_generates_resource,
    gl::ProgressReporter* progress_reporter,
    const GpuFeatureInfo& gpu_feature_info,
    ServiceDiscardableManager* discardable_manager,
    PassthroughDiscardableManager* passthrough_discardable_manager,
    SharedImageManager* shared_image_manager)
    : gpu_preferences_(gpu_preferences),
      memory_tracker_(std::move(memory_tracker)),
      shader_translator_cache_(shader_translator_cache),
#if BUILDFLAG(IS_MAC)
      // Framebuffer completeness is not cacheable on OS X because of dynamic
      // graphics switching.
      // http://crbug.com/180876
      // TODO(tobiasjs): determine whether GPU switching is possible
      // programmatically, rather than just hardcoding this behaviour
      // for OS X.
      framebuffer_completeness_cache_(nullptr),
#else
      framebuffer_completeness_cache_(framebuffer_completeness_cache),
#endif
      enforce_gl_minimums_(gpu_preferences_.enforce_gl_minimums),
      bind_generates_resource_(bind_generates_resource),
      max_vertex_attribs_(0u),
      max_texture_units_(0u),
      max_texture_image_units_(0u),
      max_vertex_texture_image_units_(0u),
      max_fragment_uniform_vectors_(0u),
      max_varying_vectors_(0u),
      max_vertex_uniform_vectors_(0u),
      max_color_attachments_(1u),
      max_draw_buffers_(1u),
      max_dual_source_draw_buffers_(0u),
      max_vertex_output_components_(0u),
      max_fragment_input_components_(0u),
      min_program_texel_offset_(0),
      max_program_texel_offset_(0),
      max_transform_feedback_separate_attribs_(0u),
      max_uniform_buffer_bindings_(0u),
      uniform_buffer_offset_alignment_(1u),
      program_cache_(nullptr),
      feature_info_(feature_info),
      use_passthrough_cmd_decoder_(false),
      passthrough_resources_(new PassthroughResources),
      passthrough_discardable_manager_(passthrough_discardable_manager),
      progress_reporter_(progress_reporter),
      gpu_feature_info_(gpu_feature_info),
      discardable_manager_(discardable_manager),
      shared_image_representation_factory_(
          std::make_unique<SharedImageRepresentationFactory>(
              shared_image_manager,
              memory_tracker_.get())),
      shared_image_manager_(shared_image_manager) {
  DCHECK(discardable_manager);
  DCHECK(feature_info_);
  use_passthrough_cmd_decoder_ = supports_passthrough_command_decoders &&
                                 gpu_preferences_.use_passthrough_cmd_decoder;
}

gpu::ContextResult ContextGroup::Initialize(
    DecoderContext* decoder,
    ContextType context_type,
    const DisallowedFeatures& disallowed_features) {
  switch (context_type) {
    case CONTEXT_TYPE_WEBGL1:
      if (kGpuFeatureStatusBlocklisted ==
          gpu_feature_info_.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL]) {
        LOG(ERROR) << "ContextResult::kFatalFailure: WebGL1 blocklisted";
        return gpu::ContextResult::kFatalFailure;
      }
      break;
    case CONTEXT_TYPE_WEBGL2:
      if (kGpuFeatureStatusBlocklisted ==
          gpu_feature_info_
              .status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL2]) {
        LOG(ERROR) << "ContextResult::kFatalFailure: WebGL2 blocklisted";
        return gpu::ContextResult::kFatalFailure;
      }
      break;
    default:
      break;
  }
  if (HaveContexts()) {
    if (context_type != feature_info_->context_type()) {
      LOG(ERROR) << "ContextResult::kFatalFailure: the type of "
                    "the context does not fit with the group.";
      return gpu::ContextResult::kFatalFailure;
    }
    // If we've already initialized the group just add the context.
    decoders_.push_back(decoder->AsWeakPtr());
    return gpu::ContextResult::kSuccess;
  }

  DisallowedFeatures adjusted_disallowed_features =
      AdjustDisallowedFeatures(context_type, disallowed_features);

  feature_info_->Initialize(context_type, use_passthrough_cmd_decoder_,
                            adjusted_disallowed_features);

  // Fail early if ES3 is requested and driver does not support it.
  if ((context_type == CONTEXT_TYPE_WEBGL2 ||
       context_type == CONTEXT_TYPE_OPENGLES3) &&
      !feature_info_->IsES3Capable()) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
               << "ES3 is blocklisted/disabled/unsupported by driver.";
    return gpu::ContextResult::kFatalFailure;
  }

  const GLint kMinRenderbufferSize = 512;  // GL says 1 pixel!
  GLint max_renderbuffer_size = 0;
  if (!QueryGLFeature(
      GL_MAX_RENDERBUFFER_SIZE, kMinRenderbufferSize,
      &max_renderbuffer_size)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "maximum renderbuffer size too small ("
               << max_renderbuffer_size << ", should be "
               << kMinRenderbufferSize << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }
  GLint max_samples = 0;
  if (feature_info_->feature_flags().chromium_framebuffer_multisample ||
      feature_info_->feature_flags().multisampled_render_to_texture) {
    if (feature_info_->feature_flags()
            .use_img_for_multisampled_render_to_texture) {
      glGetIntegerv(GL_MAX_SAMPLES_IMG, &max_samples);
    } else {
      glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
    }
  }

  if (context_type == CONTEXT_TYPE_OPENGLES3 ||
      context_type == CONTEXT_TYPE_WEBGL2 ||
      feature_info_->feature_flags().ext_draw_buffers) {
    GetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &max_color_attachments_);
    if (max_color_attachments_ < 1)
      max_color_attachments_ = 1;
    if (max_color_attachments_ > 16)
      max_color_attachments_ = 16;
    GetIntegerv(GL_MAX_DRAW_BUFFERS_ARB, &max_draw_buffers_);
    if (max_draw_buffers_ < 1)
      max_draw_buffers_ = 1;
    if (max_draw_buffers_ > 16)
      max_draw_buffers_ = 16;
  }
  if (feature_info_->feature_flags().ext_blend_func_extended) {
    GetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT,
                &max_dual_source_draw_buffers_);
    DCHECK(max_dual_source_draw_buffers_ >= 1);
  }

  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 0)) {
    const GLint kMinTransformFeedbackSeparateAttribs = 4;
    if (!QueryGLFeatureU(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                         kMinTransformFeedbackSeparateAttribs,
                         &max_transform_feedback_separate_attribs_)) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "maximum transform feedback separate attribs is too small ("
                 << max_transform_feedback_separate_attribs_ << ", should be "
                 << kMinTransformFeedbackSeparateAttribs << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }

    const GLint kMinUniformBufferBindings = 24;
    if (!QueryGLFeatureU(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                         kMinUniformBufferBindings,
                         &max_uniform_buffer_bindings_)) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "maximum uniform buffer bindings is too small ("
                 << max_uniform_buffer_bindings_ << ", should be "
                 << kMinUniformBufferBindings << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }

    // TODO(zmo): Should we check max UNIFORM_BUFFER_OFFSET_ALIGNMENT is 256?
    GetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                &uniform_buffer_offset_alignment_);
  }

  // Managers are not used by the passthrough command decoder. Save memory by
  // not allocating them.
  if (!use_passthrough_cmd_decoder_) {
    buffer_manager_ = std::make_unique<BufferManager>(memory_tracker_.get(),
                                                      feature_info_.get());
    renderbuffer_manager_ = std::make_unique<RenderbufferManager>(
        memory_tracker_.get(), max_renderbuffer_size, max_samples,
        feature_info_.get());
    shader_manager_ = std::make_unique<ShaderManager>(progress_reporter_);
    sampler_manager_ = std::make_unique<SamplerManager>(feature_info_.get());
  }

  // Lookup GL things we need to know.
  const GLint kGLES2RequiredMinimumVertexAttribs = 8u;
  if (!QueryGLFeatureU(
      GL_MAX_VERTEX_ATTRIBS, kGLES2RequiredMinimumVertexAttribs,
      &max_vertex_attribs_)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "too few vertex attributes supported (" << max_vertex_attribs_
               << ", should be " << kGLES2RequiredMinimumVertexAttribs << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }

  const GLuint kGLES2RequiredMinimumTextureUnits = 8u;
  if (!QueryGLFeatureU(
      GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, kGLES2RequiredMinimumTextureUnits,
      &max_texture_units_)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "too few texture units supported (" << max_texture_units_
               << ", should be " << kGLES2RequiredMinimumTextureUnits << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }

  GLint max_texture_size = 0;
  GLint max_cube_map_texture_size = 0;
  GLint max_rectangle_texture_size = 0;
  GLint max_3d_texture_size = 0;
  GLint max_array_texture_layers = 0;

  const GLint kMinTextureSize = 2048;  // GL actually says 64!?!?
  const GLint kMinCubeMapSize = 256;  // GL actually says 16!?!?
  const GLint kMinRectangleTextureSize = 64;
  const GLint kMin3DTextureSize = 256;
  const GLint kMinArrayTextureLayers = 256;

  if (!QueryGLFeature(GL_MAX_TEXTURE_SIZE, kMinTextureSize,
                      &max_texture_size)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "maximum 2d texture size is too small (" << max_texture_size
               << ", should be " << kMinTextureSize << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }
  if (!QueryGLFeature(GL_MAX_CUBE_MAP_TEXTURE_SIZE, kMinCubeMapSize,
                      &max_cube_map_texture_size)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "maximum cube texture size is too small ("
               << max_cube_map_texture_size << ", should be " << kMinCubeMapSize
               << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 0) &&
      !QueryGLFeature(GL_MAX_3D_TEXTURE_SIZE, kMin3DTextureSize,
                      &max_3d_texture_size)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "maximum 3d texture size is too small ("
               << max_3d_texture_size << ", should be " << kMin3DTextureSize
               << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 0) &&
      !QueryGLFeature(GL_MAX_ARRAY_TEXTURE_LAYERS, kMinArrayTextureLayers,
                      &max_array_texture_layers)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "maximum array texture layers is too small ("
               << max_array_texture_layers << ", should be "
               << kMinArrayTextureLayers << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }
  if (feature_info_->feature_flags().arb_texture_rectangle &&
      !QueryGLFeature(GL_MAX_RECTANGLE_TEXTURE_SIZE_ARB,
                      kMinRectangleTextureSize, &max_rectangle_texture_size)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "maximum rectangle texture size is too small ("
               << max_rectangle_texture_size << ", should be "
               << kMinRectangleTextureSize << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }

  if (feature_info_->workarounds().webgl_or_caps_max_texture_size &&
      feature_info_->IsWebGLContext()) {
    max_texture_size =
        std::min(max_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
    max_rectangle_texture_size =
        std::min(max_rectangle_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
    max_cube_map_texture_size =
        std::min(max_cube_map_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
  }

  if (feature_info_->workarounds().max_3d_array_texture_size) {
    max_3d_texture_size =
        std::min(max_3d_texture_size,
                 feature_info_->workarounds().max_3d_array_texture_size);
    max_array_texture_layers =
        std::min(max_array_texture_layers,
                 feature_info_->workarounds().max_3d_array_texture_size);
  }

  // Managers are not used by the passthrough command decoder. Save memory by
  // not allocating them.
  if (!use_passthrough_cmd_decoder_) {
    texture_manager_ = std::make_unique<TextureManager>(
        memory_tracker_.get(), feature_info_.get(), max_texture_size,
        max_cube_map_texture_size, max_rectangle_texture_size,
        max_3d_texture_size, max_array_texture_layers, bind_generates_resource_,
        progress_reporter_, discardable_manager_);
  }

  const GLint kMinTextureImageUnits = 8;
  const GLint kMinVertexTextureImageUnits = 0;
  if (!QueryGLFeatureU(GL_MAX_TEXTURE_IMAGE_UNITS, kMinTextureImageUnits,
                       &max_texture_image_units_)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "too few texture image units supported ("
               << max_texture_image_units_ << ", should be "
               << kMinTextureImageUnits << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }
  if (!QueryGLFeatureU(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                       kMinVertexTextureImageUnits,
                       &max_vertex_texture_image_units_)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "too few vertex texture image units supported ("
               << max_vertex_texture_image_units_ << ", should be "
               << kMinTextureImageUnits << ").";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }

  GetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &max_fragment_uniform_vectors_);
  GetIntegerv(GL_MAX_VARYING_VECTORS, &max_varying_vectors_);
  GetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &max_vertex_uniform_vectors_);

  const GLint kMinFragmentUniformVectors = 16;
  const GLint kMinVaryingVectors = 8;
  const GLint kMinVertexUniformVectors = 128;
  if (!CheckGLFeatureU(
      kMinFragmentUniformVectors, &max_fragment_uniform_vectors_) ||
      !CheckGLFeatureU(kMinVaryingVectors, &max_varying_vectors_) ||
      !CheckGLFeatureU(
      kMinVertexUniformVectors, &max_vertex_uniform_vectors_)) {
    bool was_lost = decoder->CheckResetStatus();
    LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                            : "ContextResult::kFatalFailure: ")
               << "too few uniforms or varyings supported.";
    return was_lost ? gpu::ContextResult::kTransientFailure
                    : gpu::ContextResult::kFatalFailure;
  }

  if (context_type != CONTEXT_TYPE_WEBGL1 &&
      context_type != CONTEXT_TYPE_OPENGLES2) {
    const GLuint kMinVertexOutputComponents = 64;
    const GLuint kMinFragmentInputComponents = 60;
    const GLint kMin_MaxProgramTexelOffset = 7;
    const GLint kMax_MinProgramTexelOffset = -8;

    if (!QueryGLFeatureU(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                         kMinVertexOutputComponents,
                         &max_vertex_output_components_)) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "maximum vertex output components is too small ("
                 << max_vertex_output_components_ << ", should be "
                 << kMinVertexOutputComponents << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }
    if (!QueryGLFeatureU(GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                         kMinFragmentInputComponents,
                         &max_fragment_input_components_)) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "maximum fragment input components is too small ("
                 << max_fragment_input_components_ << ", should be "
                 << kMinFragmentInputComponents << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }
    if (!QueryGLFeature(GL_MAX_PROGRAM_TEXEL_OFFSET, kMin_MaxProgramTexelOffset,
                        &max_program_texel_offset_)) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "maximum program texel offset is too small ("
                 << max_program_texel_offset_ << ", should be "
                 << kMin_MaxProgramTexelOffset << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }
    glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &min_program_texel_offset_);
    if (enforce_gl_minimums_) {
      min_program_texel_offset_ =
          std::max(min_program_texel_offset_, kMax_MinProgramTexelOffset);
    }
    if (min_program_texel_offset_ > kMax_MinProgramTexelOffset) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "minimum program texel offset is too big ("
                 << min_program_texel_offset_ << ", should be "
                 << kMax_MinProgramTexelOffset << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }

    const GLint kES3MinCubeMapSize = 2048;
    if (max_cube_map_texture_size < kES3MinCubeMapSize) {
      bool was_lost = decoder->CheckResetStatus();
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "maximum cube texture size is too small ("
                 << max_cube_map_texture_size << ", should be "
                 << kES3MinCubeMapSize << ").";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }
  }

  // Managers are not used by the passthrough command decoder. Save memory by
  // not allocating them.
  if (!use_passthrough_cmd_decoder_) {
    program_manager_ = std::make_unique<ProgramManager>(
        program_cache_, max_varying_vectors_, max_draw_buffers_,
        max_dual_source_draw_buffers_, max_vertex_attribs_, gpu_preferences_,
        feature_info_.get(), progress_reporter_);

    texture_manager_->Initialize();
  }

  decoders_.push_back(decoder->AsWeakPtr());
  return gpu::ContextResult::kSuccess;
}

namespace {

bool IsNull(const base::WeakPtr<DecoderContext>& decoder) {
  return !decoder;
}

template <typename T>
class WeakPtrEquals {
 public:
  explicit WeakPtrEquals(T* t) : t_(t) {}

  bool operator()(const base::WeakPtr<T>& t) {
    return t.get() == t_;
  }

 private:
  const raw_ptr<T> t_;
};

}  // namespace anonymous

bool ContextGroup::HaveContexts() {
  std::erase_if(decoders_, IsNull);
  return !decoders_.empty();
}

void ContextGroup::ReportProgress() {
  if (progress_reporter_)
    progress_reporter_->ReportProgress();
}

void ContextGroup::Destroy(DecoderContext* decoder, bool have_context) {
  std::erase_if(decoders_, WeakPtrEquals<DecoderContext>(decoder));

  // If we still have contexts do nothing.
  if (HaveContexts()) {
    return;
  }

  if (buffer_manager_ != nullptr) {
    if (!have_context) {
      buffer_manager_->MarkContextLost();
    }
    buffer_manager_->Destroy();
    buffer_manager_.reset();
    ReportProgress();
  }

  if (renderbuffer_manager_ != nullptr) {
    renderbuffer_manager_->Destroy(have_context);
    renderbuffer_manager_.reset();
    ReportProgress();
  }

  if (texture_manager_ != nullptr) {
    if (!have_context)
      texture_manager_->MarkContextLost();
    texture_manager_->Destroy();
    texture_manager_.reset();
    ReportProgress();
  }

  if (program_manager_ != nullptr) {
    program_manager_->Destroy(have_context);
    program_manager_.reset();
    ReportProgress();
  }

  if (shader_manager_ != nullptr) {
    shader_manager_->Destroy(have_context);
    shader_manager_.reset();
    ReportProgress();
  }

  if (sampler_manager_ != nullptr) {
    sampler_manager_->Destroy(have_context);
    sampler_manager_.reset();
    ReportProgress();
  }

  if (passthrough_discardable_manager_) {
    passthrough_discardable_manager_->DeleteContextGroup(this, have_context);
  }

  if (passthrough_resources_) {
    gl::GLApi* api = have_context ? gl::g_current_gl_context : nullptr;
    passthrough_resources_->Destroy(api, progress_reporter_);
    passthrough_resources_.reset();
    ReportProgress();
  }
}

uint32_t ContextGroup::GetMemRepresented() const {
  uint32_t total = 0;
  if (buffer_manager_.get())
    total += buffer_manager_->mem_represented();
  if (renderbuffer_manager_.get())
    total += renderbuffer_manager_->mem_represented();
  if (texture_manager_.get())
    total += texture_manager_->mem_represented();
  return total;
}

void ContextGroup::LoseContexts(error::ContextLostReason reason) {
  for (size_t ii = 0; ii < decoders_.size(); ++ii) {
    if (decoders_[ii].get()) {
      decoders_[ii]->MarkContextLost(reason);
    }
  }
  if (buffer_manager_ != nullptr) {
    buffer_manager_->MarkContextLost();
  }
}

ContextGroup::~ContextGroup() {
  CHECK(!HaveContexts());
}

bool ContextGroup::CheckGLFeature(GLint min_required, GLint* v) {
  GLint value = *v;
  if (enforce_gl_minimums_) {
    value = std::min(min_required, value);
  }
  *v = value;
  return value >= min_required;
}

bool ContextGroup::CheckGLFeatureU(GLint min_required, uint32_t* v) {
  GLint value = *v;
  if (enforce_gl_minimums_) {
    value = std::min(min_required, value);
  }
  *v = value;
  return value >= min_required;
}

bool ContextGroup::QueryGLFeature(
    GLenum pname, GLint min_required, GLint* v) {
  GLint value = 0;
  glGetIntegerv(pname, &value);
  *v = value;
  return CheckGLFeature(min_required, v);
}

bool ContextGroup::QueryGLFeatureU(GLenum pname,
                                   GLint min_required,
                                   uint32_t* v) {
  uint32_t value = 0;
  GetIntegerv(pname, &value);
  bool result = CheckGLFeatureU(min_required, &value);
  *v = value;
  return result;
}

bool ContextGroup::GetBufferServiceId(
    GLuint client_id, GLuint* service_id) const {
  Buffer* buffer = buffer_manager_->GetBuffer(client_id);
  if (!buffer)
    return false;
  *service_id = buffer->service_id();
  return true;
}

}  // namespace gles2
}  // namespace gpu
