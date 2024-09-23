// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gles2_external_framebuffer.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/multi_draw_manager.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/progress_reporter.h"
#include "ui/gl/scoped_make_current.h"

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#endif  // BUILDFLAG(IS_WIN)

namespace gpu {
namespace gles2 {

namespace {
GLenum GetterForTextureTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_EXTERNAL_OES:
      return GL_TEXTURE_BINDING_EXTERNAL_OES;
    case GL_TEXTURE_RECTANGLE_ARB:
      return GL_TEXTURE_BINDING_RECTANGLE_ARB;
    default:
      // Other targets not currently used.
      NOTIMPLEMENTED();
      return GL_TEXTURE_2D;
  }
}

class ScopedFramebufferBindingReset {
 public:
  ScopedFramebufferBindingReset(gl::GLApi* api,
                                bool supports_separate_fbo_bindings)
      : api_(api),
        supports_separate_fbo_bindings_(supports_separate_fbo_bindings),
        draw_framebuffer_(0),
        read_framebuffer_(0) {
    if (supports_separate_fbo_bindings_) {
      api_->glGetIntegervFn(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
      api_->glGetIntegervFn(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
    } else {
      api_->glGetIntegervFn(GL_FRAMEBUFFER_BINDING, &draw_framebuffer_);
    }
  }

  ~ScopedFramebufferBindingReset() {
    if (supports_separate_fbo_bindings_) {
      api_->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER, draw_framebuffer_);
      api_->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, read_framebuffer_);
    } else {
      api_->glBindFramebufferEXTFn(GL_FRAMEBUFFER, draw_framebuffer_);
    }
  }

 private:
  raw_ptr<gl::GLApi> api_;
  bool supports_separate_fbo_bindings_;
  GLint draw_framebuffer_;
  GLint read_framebuffer_;
};

class ScopedTextureBindingReset {
 public:
  // |texture_target| only supports GL_TEXTURE_2D, GL_TEXTURE_EXTERNAL_OES, and
  // GL_TEXTURE_RECTANGLE_ARB.
  ScopedTextureBindingReset(gl::GLApi* api, GLenum texture_target)
      : api_(api), texture_target_(texture_target), texture_(0) {
    api_->glGetIntegervFn(GetterForTextureTarget(texture_target_), &texture_);
  }

  ~ScopedTextureBindingReset() {
    api_->glBindTextureFn(texture_target_, texture_);
  }

 private:
  raw_ptr<gl::GLApi> api_;
  GLenum texture_target_;
  GLint texture_;
};

class ScopedClearColorReset {
 public:
  explicit ScopedClearColorReset(gl::GLApi* api) : api_(api) {
    api_->glGetFloatvFn(GL_COLOR_CLEAR_VALUE, clear_color_);
  }
  ~ScopedClearColorReset() {
    api_->glClearColorFn(clear_color_[0], clear_color_[1], clear_color_[2],
                         clear_color_[3]);
  }

 private:
  raw_ptr<gl::GLApi> api_;
  GLfloat clear_color_[4];
};

// Reset the color mask for buffer zero only.
class ScopedColorMaskZeroReset {
 public:
  explicit ScopedColorMaskZeroReset(gl::GLApi* api,
                                    bool oes_draw_buffers_indexed)
      : api_(api), oes_draw_buffers_indexed_(oes_draw_buffers_indexed) {
    if (oes_draw_buffers_indexed_) {
      GLsizei length = 0;
      api_->glGetBooleani_vRobustANGLEFn(
          GL_COLOR_WRITEMASK, 0, sizeof(color_mask_), &length, color_mask_);
    } else {
      api_->glGetBooleanvFn(GL_COLOR_WRITEMASK, color_mask_);
    }
  }
  ~ScopedColorMaskZeroReset() {
    if (oes_draw_buffers_indexed_) {
      api_->glColorMaskiOESFn(0, color_mask_[0], color_mask_[1], color_mask_[2],
                              color_mask_[3]);
    } else {
      api_->glColorMaskFn(color_mask_[0], color_mask_[1], color_mask_[2],
                          color_mask_[3]);
    }
  }

 private:
  raw_ptr<gl::GLApi> api_;
  const bool oes_draw_buffers_indexed_;
  // The color mask, or the color mask of buffer zero, if
  // OES_draw_buffers_indexed is enabled.
  GLboolean color_mask_[4];
};

class ScopedScissorTestReset {
 public:
  explicit ScopedScissorTestReset(gl::GLApi* api) : api_(api) {
    api_->glGetBooleanvFn(GL_SCISSOR_TEST, &scissor_test_);
  }
  ~ScopedScissorTestReset() {
    if (scissor_test_)
      api_->glEnableFn(GL_SCISSOR_TEST);
    else
      api_->glDisableFn(GL_SCISSOR_TEST);
  }

 private:
  raw_ptr<gl::GLApi> api_;
  GLboolean scissor_test_;
};

template <typename ClientType, typename ServiceType, typename DeleteFunction>
void DeleteServiceObjects(ClientServiceMap<ClientType, ServiceType>* id_map,
                          bool have_context,
                          DeleteFunction delete_function) {
  if (have_context) {
    id_map->ForEach(delete_function);
  }

  id_map->Clear();
}

template <typename ClientType, typename ServiceType, typename ResultType>
bool GetClientID(const ClientServiceMap<ClientType, ServiceType>* map,
                 ResultType service_id,
                 ResultType* result) {
  ClientType client_id = 0;
  if (!map->GetClientID(static_cast<ServiceType>(service_id), &client_id)) {
    return false;
  }
  *result = static_cast<ResultType>(client_id);
  return true;
}

void RequestExtensions(gl::GLApi* api,
                       const gfx::ExtensionSet& requestable_extensions,
                       const char* const* extensions_to_request,
                       size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (gfx::HasExtension(requestable_extensions, extensions_to_request[i])) {
      // Request the intersection of the two sets
      api->glRequestExtensionANGLEFn(extensions_to_request[i]);
    }
  }
}

void APIENTRY PassthroughGLDebugMessageCallback(GLenum source,
                                                GLenum type,
                                                GLuint id,
                                                GLenum severity,
                                                GLsizei length,
                                                const GLchar* message,
                                                const GLvoid* user_param) {
  DCHECK(user_param != nullptr);
  GLES2DecoderPassthroughImpl* command_decoder =
      static_cast<GLES2DecoderPassthroughImpl*>(const_cast<void*>(user_param));
  command_decoder->OnDebugMessage(source, type, id, severity, length, message);
  LogGLDebugMessage(source, type, id, severity, length, message,
                    command_decoder->GetLogger());
}

void RunCallbacks(std::vector<base::OnceClosure> callbacks) {
  for (base::OnceClosure& callback : callbacks) {
    std::move(callback).Run();
  }
}

// Converts texture targets to texture binding types.  Does not validate the
// input.
GLenum TextureTargetToTextureType(GLenum texture_target) {
  switch (texture_target) {
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return GL_TEXTURE_CUBE_MAP;

    default:
      return texture_target;
  }
}

void UpdateBoundTexturePassthroughSize(gl::GLApi* api,
                                       TexturePassthrough* texture) {
  GLint texture_memory_size = 0;
  api->glGetTexParameterivFn(texture->target(), GL_MEMORY_SIZE_ANGLE,
                             &texture_memory_size);

  texture->SetEstimatedSize(texture_memory_size);
}

void ReturnProgramInfoData(DecoderClient* client,
                           const std::vector<uint8_t>& info,
                           GLES2ReturnDataType type,
                           uint32_t program) {
  // Limit the data size in order not to block the IO threads too long.
  // https://docs.google.com/document/d/1qEfU0lAkeZ8lU06qtxv7ENGxRxExxztXu1LhIDNGqtU/edit?disco=AAAACksORlU
  constexpr static size_t kMaxDataSize = 8 * 1024 * 1024;
  if (info.size() > kMaxDataSize)
    return;

  std::vector<uint8_t> return_data;
  return_data.resize(sizeof(cmds::GLES2ReturnProgramInfo) + info.size());
  auto* return_program_info =
      reinterpret_cast<cmds::GLES2ReturnProgramInfo*>(return_data.data());
  return_program_info->return_data_header.return_data_type = type;
  return_program_info->program_client_id = program;
  memcpy(return_program_info->deserialized_buffer, info.data(), info.size());
  client->HandleReturnData(
      base::span<uint8_t>(return_data.data(), return_data.size()));
}

}  // anonymous namespace

GLES2DecoderPassthroughImpl::ScopedPixelLocalStorageInterrupt::
    ScopedPixelLocalStorageInterrupt(const GLES2DecoderPassthroughImpl* impl)
    : impl_(impl) {
  if (impl_->has_activated_pixel_local_storage_) {
    impl_->api()->glFramebufferPixelLocalStorageInterruptANGLEFn();
  }
}

GLES2DecoderPassthroughImpl::ScopedPixelLocalStorageInterrupt::
    ~ScopedPixelLocalStorageInterrupt() {
  if (impl_->has_activated_pixel_local_storage_) {
    impl_->api()->glFramebufferPixelLocalStorageRestoreANGLEFn();
  }
}

PassthroughResources::PassthroughResources() : texture_object_map(nullptr) {}
PassthroughResources::~PassthroughResources() = default;

void PassthroughResources::SuspendSharedImageAccessIfNeeded() {
  for (auto& [texture_id, shared_image_data] : texture_shared_image_map) {
    shared_image_data.SuspendAccessIfNeeded();
  }
}

bool PassthroughResources::ResumeSharedImageAccessIfNeeded(gl::GLApi* api) {
  bool success = true;
  for (auto& [texture_id, shared_image_data] : texture_shared_image_map) {
    if (!shared_image_data.ResumeAccessIfNeeded(api)) {
      success = false;
    }
  }
  return success;
}

void PassthroughResources::Destroy(gl::GLApi* api,
                                   gl::ProgressReporter* progress_reporter) {
  bool have_context = !!api;
  // Only delete textures that are not referenced by a TexturePassthrough
  // object, they handle their own deletion once all references are lost
  DeleteServiceObjects(
      &texture_id_map, have_context,
      [this, api, progress_reporter](GLuint client_id, GLuint texture) {
        if (!texture_object_map.HasClientID(client_id)) {
          api->glDeleteTexturesFn(1, &texture);
          if (progress_reporter) {
            progress_reporter->ReportProgress();
          }
        }
      });
  DeleteServiceObjects(
      &buffer_id_map, have_context,
      [api, progress_reporter](GLuint client_id, GLuint buffer) {
        api->glDeleteBuffersARBFn(1, &buffer);
        if (progress_reporter) {
          progress_reporter->ReportProgress();
        }
      });
  DeleteServiceObjects(
      &renderbuffer_id_map, have_context,
      [api, progress_reporter](GLuint client_id, GLuint renderbuffer) {
        api->glDeleteRenderbuffersEXTFn(1, &renderbuffer);
        if (progress_reporter) {
          progress_reporter->ReportProgress();
        }
      });
  DeleteServiceObjects(&sampler_id_map, have_context,
                       [api](GLuint client_id, GLuint sampler) {
                         api->glDeleteSamplersFn(1, &sampler);
                       });
  DeleteServiceObjects(
      &program_id_map, have_context,
      [api, progress_reporter](GLuint client_id, GLuint program) {
        api->glDeleteProgramFn(program);
        if (progress_reporter) {
          progress_reporter->ReportProgress();
        }
      });
  DeleteServiceObjects(&shader_id_map, have_context,
                       [api](GLuint client_id, GLuint shader) {
                         api->glDeleteShaderFn(shader);
                       });
  DeleteServiceObjects(&sync_id_map, have_context,
                       [api](GLuint client_id, uintptr_t sync) {
                         api->glDeleteSyncFn(reinterpret_cast<GLsync>(sync));
                       });

  if (!have_context) {
    texture_object_map.ForEach(
        [](GLuint client_id, scoped_refptr<TexturePassthrough> texture) {
          texture->MarkContextLost();
        });
    for (auto& pair : texture_shared_image_map) {
      pair.second.representation()->OnContextLost();
    }
  }
  texture_object_map.Clear();
  texture_shared_image_map.clear();
}

PassthroughResources::SharedImageData::SharedImageData() = default;
PassthroughResources::SharedImageData::SharedImageData(
    const GLES2DecoderPassthroughImpl* impl,
    std::unique_ptr<GLTexturePassthroughImageRepresentation> representation)
    : representation_(std::move(representation)) {
  DCHECK(representation_);

  // Note, that ideally we could defer clear till BeginAccess, but there is no
  // enforcement that will require clients to call Begin/End access.
  EnsureClear(impl);
}
PassthroughResources::SharedImageData::SharedImageData(
    SharedImageData&& other) = default;
PassthroughResources::SharedImageData::~SharedImageData() = default;

PassthroughResources::SharedImageData&
PassthroughResources::SharedImageData::operator=(SharedImageData&& other) {
  scoped_access_ = std::move(other.scoped_access_);
  access_mode_ = std::move(other.access_mode_);
  other.access_mode_.reset();
  representation_ = std::move(other.representation_);
  return *this;
}

void PassthroughResources::SharedImageData::EnsureClear(
    const GLES2DecoderPassthroughImpl* impl) {
  // To avoid unnessary overhead we don't enable robust initialization on shared
  // gl context where all shared images are created, so we clear image here if
  // necessary.
  if (!representation_->IsCleared()) {
    // Allow uncleared access as we're going to clear the image.
    auto scoped_access = representation_->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);

    if (!scoped_access)
      return;

    auto texture = representation_->GetTexturePassthrough();
    const bool use_oes_draw_buffers_indexed =
        impl->features().oes_draw_buffers_indexed;

    // Back up all state we are about to change.
    gl::GLApi* api = impl->api();
    GLES2DecoderPassthroughImpl::ScopedPixelLocalStorageInterrupt
        scoped_pls_interrupt(impl);
    ScopedFramebufferBindingReset fbo_reset(
        api, false /* supports_seperate_fbo_bindings */);
    ScopedTextureBindingReset texture_reset(api, texture->target());
    ScopedClearColorReset clear_color_reset(api);
    ScopedColorMaskZeroReset color_mask_reset(api,
                                              use_oes_draw_buffers_indexed);
    ScopedScissorTestReset scissor_test_reset(api);

    // Generate a new framebuffer and bind the shared image's uncleared texture
    // to it.
    GLuint fbo = 0;
    api->glGenFramebuffersEXTFn(1, &fbo);
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);
    api->glBindTextureFn(texture->target(), texture->service_id());
    api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     texture->target(), texture->service_id(),
                                     0);
    // Clear the bound framebuffer.
    api->glClearColorFn(0, 0, 0, 0);
    if (use_oes_draw_buffers_indexed)
      api->glColorMaskiOESFn(0, true, true, true, true);
    else
      api->glColorMaskFn(true, true, true, true);
    api->glDisableFn(GL_SCISSOR_TEST);
    api->glClearFn(GL_COLOR_BUFFER_BIT);

    // Delete the generated framebuffer.
    api->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     texture->target(), 0, 0);
    api->glDeleteFramebuffersEXTFn(1, &fbo);

    // Mark the shared image as cleared.
    representation_->SetCleared();
  }
}

bool PassthroughResources::SharedImageData::BeginAccess(GLenum mode,
                                                        gl::GLApi* api) {
  DCHECK(!is_being_accessed());
  // The image should have been cleared already when we created the texture if
  // necessary.
  scoped_access_ = representation_->BeginScopedAccess(
      mode, SharedImageRepresentation::AllowUnclearedAccess::kNo);
  if (scoped_access_) {
    access_mode_.emplace(mode);
    return true;
  }
  return false;
}

void PassthroughResources::SharedImageData::EndAccess() {
  DCHECK(is_being_accessed());
  scoped_access_.reset();
  access_mode_.reset();
}

bool PassthroughResources::SharedImageData::ResumeAccessIfNeeded(
    gl::GLApi* api) {
  // Do not resume access if BeginAccess was never called or if a scoped access
  // is already present.
  if (!is_being_accessed() || scoped_access_) {
    return true;
  }
  scoped_access_ = representation_->BeginScopedAccess(
      access_mode_.value(),
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  return !!scoped_access_;
}

void PassthroughResources::SharedImageData::SuspendAccessIfNeeded() {
  // Suspend access if shared image is being accessed and doesn't support
  // concurrent read access on other clients or devices.
  if (is_being_accessed() &&
      representation_->NeedsSuspendAccessForDXGIKeyedMutex()) {
    scoped_access_.reset();
  }
}

GLES2DecoderPassthroughImpl::PendingQuery::PendingQuery() = default;
GLES2DecoderPassthroughImpl::PendingQuery::~PendingQuery() {
  // Run all callbacks when a query is destroyed even if it did not complete.
  // This avoids leaks due to outstandsing callbacks.
  RunCallbacks(std::move(callbacks));
}

GLES2DecoderPassthroughImpl::PendingQuery::PendingQuery(PendingQuery&&) =
    default;
GLES2DecoderPassthroughImpl::PendingQuery&
GLES2DecoderPassthroughImpl::PendingQuery::operator=(PendingQuery&&) = default;

GLES2DecoderPassthroughImpl::ActiveQuery::ActiveQuery() = default;
GLES2DecoderPassthroughImpl::ActiveQuery::~ActiveQuery() = default;
GLES2DecoderPassthroughImpl::ActiveQuery::ActiveQuery(ActiveQuery&&) = default;
GLES2DecoderPassthroughImpl::ActiveQuery&
GLES2DecoderPassthroughImpl::ActiveQuery::operator=(ActiveQuery&&) = default;

GLES2DecoderPassthroughImpl::BoundTexture::BoundTexture() = default;
GLES2DecoderPassthroughImpl::BoundTexture::~BoundTexture() = default;
GLES2DecoderPassthroughImpl::BoundTexture::BoundTexture(const BoundTexture&) =
    default;
GLES2DecoderPassthroughImpl::BoundTexture::BoundTexture(BoundTexture&&) =
    default;
GLES2DecoderPassthroughImpl::BoundTexture&
GLES2DecoderPassthroughImpl::BoundTexture::operator=(const BoundTexture&) =
    default;
GLES2DecoderPassthroughImpl::BoundTexture&
GLES2DecoderPassthroughImpl::BoundTexture::operator=(BoundTexture&&) = default;

GLES2DecoderPassthroughImpl::PendingReadPixels::PendingReadPixels() = default;
GLES2DecoderPassthroughImpl::PendingReadPixels::~PendingReadPixels() = default;
GLES2DecoderPassthroughImpl::PendingReadPixels::PendingReadPixels(
    PendingReadPixels&&) = default;
GLES2DecoderPassthroughImpl::PendingReadPixels&
GLES2DecoderPassthroughImpl::PendingReadPixels::operator=(PendingReadPixels&&) =
    default;

GLES2DecoderPassthroughImpl::BufferShadowUpdate::BufferShadowUpdate() = default;
GLES2DecoderPassthroughImpl::BufferShadowUpdate::~BufferShadowUpdate() =
    default;
GLES2DecoderPassthroughImpl::BufferShadowUpdate::BufferShadowUpdate(
    BufferShadowUpdate&&) = default;
GLES2DecoderPassthroughImpl::BufferShadowUpdate&
GLES2DecoderPassthroughImpl::BufferShadowUpdate::operator=(
    BufferShadowUpdate&&) = default;

GLES2DecoderPassthroughImpl::EmulatedDefaultFramebuffer::
    EmulatedDefaultFramebuffer(const GLES2DecoderPassthroughImpl* impl)
    : impl_(impl) {}

GLES2DecoderPassthroughImpl::EmulatedDefaultFramebuffer::
    ~EmulatedDefaultFramebuffer() = default;

bool GLES2DecoderPassthroughImpl::EmulatedDefaultFramebuffer::Initialize(
    const gfx::Size& size) {
  DCHECK(!size.IsEmpty());

  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(impl_);
  ScopedFramebufferBindingReset scoped_fbo_reset(
      api(), impl_->supports_separate_fbo_bindings_);
  ScopedTextureBindingReset scoped_texture_reset(api(), GL_TEXTURE_2D);

  api()->glGenFramebuffersEXTFn(1, &framebuffer_service_id);
  api()->glBindFramebufferEXTFn(GL_FRAMEBUFFER, framebuffer_service_id);

  const auto format = impl_->emulated_default_framebuffer_format_;

  GLuint color_buffer_texture = 0;
  api()->glGenTexturesFn(1, &color_buffer_texture);
  api()->glBindTextureFn(GL_TEXTURE_2D, color_buffer_texture);
  api()->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api()->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api()->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api()->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  api()->glTexImage2DFn(GL_TEXTURE_2D, 0, format, size.width(), size.height(),
                        0, format, GL_UNSIGNED_BYTE, nullptr);

  texture = new TexturePassthrough(color_buffer_texture, GL_TEXTURE_2D);
  UpdateBoundTexturePassthroughSize(api(), texture.get());

  api()->glFramebufferTexture2DEXTFn(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, texture->service_id(), 0);

  // Check that the framebuffer is complete
  if (api()->glCheckFramebufferStatusEXTFn(GL_FRAMEBUFFER) !=
      GL_FRAMEBUFFER_COMPLETE) {
    LOG(ERROR)
        << "GLES2DecoderPassthroughImpl::ResizeOffscreenFramebuffer failed "
        << "because the resulting framebuffer was not complete.";
    return false;
  }

  return true;
}

void GLES2DecoderPassthroughImpl::EmulatedDefaultFramebuffer::Destroy(
    bool have_context) {
  if (have_context) {
    api()->glDeleteFramebuffersEXTFn(1, &framebuffer_service_id);
    framebuffer_service_id = 0;
  } else {
    texture->MarkContextLost();
  }
  texture = nullptr;
}

GLES2DecoderPassthroughImpl::GLES2DecoderPassthroughImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    Outputter* outputter,
    ContextGroup* group)
    : GLES2Decoder(client, command_buffer_service, outputter),
      commands_to_process_(0),
      debug_marker_manager_(),
      logger_(&debug_marker_manager_,
              base::BindRepeating(&DecoderClient::OnConsoleMessage,
                                  base::Unretained(client),
                                  0),
              group->gpu_preferences().disable_gl_error_limit),
      surface_(),
      context_(),
      offscreen_(false),
      group_(group),
      feature_info_(new FeatureInfo(group->feature_info()->workarounds(),
                                    group->gpu_feature_info())),
      emulated_back_buffer_(nullptr),
      bound_draw_framebuffer_(0),
      bound_read_framebuffer_(0),
      gpu_decoder_category_(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.decoder"))),
      gpu_trace_level_(2),
      gpu_trace_commands_(false),
      gpu_debug_commands_(false),
      context_lost_(false),
      reset_by_robustness_extension_(false),
      lose_context_when_out_of_memory_(false) {
  DCHECK(client);
  DCHECK(group);
}

GLES2DecoderPassthroughImpl::~GLES2DecoderPassthroughImpl() = default;

GLES2Decoder::Error GLES2DecoderPassthroughImpl::DoCommands(
    unsigned int num_commands,
    const volatile void* buffer,
    int num_entries,
    int* entries_processed) {
  if (gpu_debug_commands_) {
    return DoCommandsImpl<true>(num_commands, buffer, num_entries,
                                entries_processed);
  } else {
    return DoCommandsImpl<false>(num_commands, buffer, num_entries,
                                 entries_processed);
  }
}

template <bool DebugImpl>
GLES2Decoder::Error GLES2DecoderPassthroughImpl::DoCommandsImpl(
    unsigned int num_commands,
    const volatile void* buffer,
    int num_entries,
    int* entries_processed) {
  commands_to_process_ = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  unsigned int command = 0;

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process_--) {
    const unsigned int size = cmd_data->value_header.size;
    command = cmd_data->value_header.command;

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    // size can't overflow because it is 21 bits.
    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    if (DebugImpl && log_commands()) {
      LOG(ERROR) << "[" << logger_.GetLogPrefix() << "]"
                 << "cmd: " << GetCommandName(command);
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstGLES2Command;
    if (command_index < std::size(command_info)) {
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        bool doing_gpu_trace = false;
        if (DebugImpl && gpu_trace_commands_) {
          if (CMD_FLAG_GET_TRACE_LEVEL(info.cmd_flags) <= gpu_trace_level_) {
            doing_gpu_trace = true;
            gpu_tracer_->Begin(TRACE_DISABLED_BY_DEFAULT("gpu.decoder"),
                               GetCommandName(command), kTraceDecoder);
          }
        }

        if (DebugImpl) {
          VerifyServiceTextureObjectsExist();
        }

        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        if (info.cmd_handler) {
          result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
        } else {
          result = error::kUnknownCommand;
        }

        if (DebugImpl && doing_gpu_trace) {
          gpu_tracer_->End(kTraceDecoder);
        }
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError && context_lost_) {
      result = error::kLostContext;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  if (entries_processed)
    *entries_processed = process_pos;

#if BUILDFLAG(IS_MAC)
  // Aggressively call glFlush on macOS. This is the only fix that has been
  // found so far to avoid crashes on Intel drivers. The workaround
  // isn't needed for WebGL contexts, though.
  // https://crbug.com/863817
  if (!feature_info_->IsWebGLContext())
    context_->FlushForDriverCrashWorkaround();
#endif

  return result;
}

void GLES2DecoderPassthroughImpl::ExitCommandProcessingEarly() {
  commands_to_process_ = 0;
}

base::WeakPtr<DecoderContext> GLES2DecoderPassthroughImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

gpu::ContextResult GLES2DecoderPassthroughImpl::Initialize(
    const scoped_refptr<gl::GLSurface>& surface,
    const scoped_refptr<gl::GLContext>& context,
    bool offscreen,
    const DisallowedFeatures& disallowed_features,
    const ContextCreationAttribs& attrib_helper) {
  TRACE_EVENT0("gpu", "GLES2DecoderPassthroughImpl::Initialize");
  DCHECK(context->IsCurrent(surface.get()));
  api_ = gl::g_current_gl_context;
  // Take ownership of the context and surface. The surface can be replaced
  // with SetSurface.
  context_ = context;
  surface_ = surface;
  offscreen_ = offscreen;

  // For WebGL contexts, log GL errors so they appear in devtools. Otherwise
  // only enable debug logging if requested.
  bool log_non_errors =
      group_->gpu_preferences().enable_gpu_driver_debug_logging;
  InitializeGLDebugLogging(log_non_errors, PassthroughGLDebugMessageCallback,
                           this);

  // Create GPU Tracer for timing values.
  gpu_tracer_ = std::make_unique<GPUTracer>(this);

  gpu_fence_manager_ = std::make_unique<GpuFenceManager>();

  multi_draw_manager_ = std::make_unique<MultiDrawManager>(
      MultiDrawManager::IndexStorageType::Pointer);

  auto result =
      group_->Initialize(this, attrib_helper.context_type, disallowed_features);
  if (result != gpu::ContextResult::kSuccess) {
    // Must not destroy ContextGroup if it is not initialized.
    group_ = nullptr;
    Destroy(true);
    return result;
  }

  // Extensions that are enabled via emulation on the client side or needed for
  // basic command buffer functionality.  Make sure they are always enabled.
  if (IsWebGLContextType(attrib_helper.context_type)) {
    // Grab the extensions that are requestable
    gfx::ExtensionSet requestable_extensions(
        gl::GetRequestableGLExtensionsFromCurrentContext());

    static constexpr const char* kRequiredFunctionalityExtensions[] = {
      "GL_ANGLE_framebuffer_blit",
#if BUILDFLAG(IS_FUCHSIA)
      "GL_ANGLE_memory_object_fuchsia",
#endif
      "GL_ANGLE_memory_size",
      "GL_ANGLE_native_id",
#if BUILDFLAG(IS_FUCHSIA)
      "GL_ANGLE_semaphore_fuchsia",
#endif
      "GL_ANGLE_texture_storage_external",
      "GL_ANGLE_texture_usage",
      "GL_CHROMIUM_bind_uniform_location",
      "GL_CHROMIUM_sync_query",
      "GL_EXT_debug_marker",
      "GL_EXT_memory_object",
      "GL_EXT_memory_object_fd",
      "GL_EXT_semaphore",
      "GL_EXT_semaphore_fd",
      "GL_KHR_debug",
      "GL_NV_fence",
      "GL_OES_EGL_image",
      "GL_OES_EGL_image_external",
      "GL_OES_EGL_image_external_essl3",
#if BUILDFLAG(IS_APPLE)
      "GL_ANGLE_texture_rectangle",
#endif
      "GL_ANGLE_vulkan_image",
    };
    RequestExtensions(api(), requestable_extensions,
                      kRequiredFunctionalityExtensions,
                      std::size(kRequiredFunctionalityExtensions));

    if (request_optional_extensions_) {
      static constexpr const char* kOptionalFunctionalityExtensions[] = {
          "GL_ANGLE_depth_texture",
          "GL_ANGLE_framebuffer_multisample",
          "GL_ANGLE_get_tex_level_parameter",
          "GL_ANGLE_instanced_arrays",
          "GL_ANGLE_memory_object_flags",
          "GL_ANGLE_pack_reverse_row_order",
          "GL_ANGLE_translated_shader_source",
          "GL_CHROMIUM_path_rendering",
          "GL_EXT_blend_minmax",
          "GL_EXT_discard_framebuffer",
          "GL_EXT_disjoint_timer_query",
          "GL_EXT_multisampled_render_to_texture",
          "GL_EXT_occlusion_query_boolean",
          "GL_EXT_sRGB",
          "GL_EXT_sRGB_write_control",
          "GL_EXT_texture_format_BGRA8888",
          "GL_EXT_texture_norm16",
          "GL_EXT_texture_rg",
          "GL_EXT_texture_sRGB_decode",
          "GL_EXT_texture_storage",
          "GL_EXT_unpack_subimage",
          "GL_KHR_parallel_shader_compile",
          "GL_KHR_robust_buffer_access_behavior",
#if BUILDFLAG(IS_CHROMEOS)
          // Required for Webgl to display in overlay on ChromeOS devices.
          // TODO(crbug.com/40244202): Consider for other platforms.
          "GL_MESA_framebuffer_flip_y",
#endif
          "GL_NV_pack_subimage",
          "GL_OES_depth32",
          "GL_OES_packed_depth_stencil",
          "GL_OES_rgb8_rgba8",
          "GL_OES_vertex_array_object",
          "NV_EGL_stream_consumer_external",
      };
      RequestExtensions(api(), requestable_extensions,
                        kOptionalFunctionalityExtensions,
                        std::size(kOptionalFunctionalityExtensions));
    }

    context->ReinitializeDynamicBindings();
  }

  // Each context initializes its own feature info because some extensions may
  // be enabled dynamically.  Don't disallow any features, leave it up to ANGLE
  // to dynamically enable extensions.
  InitializeFeatureInfo(attrib_helper.context_type, DisallowedFeatures(),
                        false);

  // Check for required extensions
  // TODO(geofflang): verify
  // feature_info_->feature_flags().angle_robust_resource_initialization and
  // api()->glIsEnabledFn(GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE)

#define FAIL_INIT_IF_NOT(feature, message)                       \
  if (!(feature)) {                                              \
    Destroy(true);                                               \
    LOG(ERROR) << "ContextResult::kFatalFailure: " << (message); \
    return gpu::ContextResult::kFatalFailure;                    \
  }

  FAIL_INIT_IF_NOT(feature_info_->feature_flags().angle_robust_client_memory,
                   "missing GL_ANGLE_robust_client_memory");
  FAIL_INIT_IF_NOT(
      feature_info_->feature_flags().chromium_bind_generates_resource,
      "missing GL_CHROMIUM_bind_generates_resource");
  FAIL_INIT_IF_NOT(feature_info_->feature_flags().chromium_copy_texture,
                   "missing GL_CHROMIUM_copy_texture");
  FAIL_INIT_IF_NOT(feature_info_->feature_flags().angle_client_arrays,
                   "missing GL_ANGLE_client_arrays");
  FAIL_INIT_IF_NOT(api()->glIsEnabledFn(GL_CLIENT_ARRAYS_ANGLE) == GL_FALSE,
                   "GL_ANGLE_client_arrays shouldn't be enabled");
  FAIL_INIT_IF_NOT(feature_info_->feature_flags().angle_webgl_compatibility ==
                       IsWebGLContextType(attrib_helper.context_type),
                   "missing GL_ANGLE_webgl_compatibility");
  FAIL_INIT_IF_NOT(feature_info_->feature_flags().angle_request_extension,
                   "missing GL_ANGLE_request_extension");
  FAIL_INIT_IF_NOT(feature_info_->feature_flags().khr_debug,
                   "missing GL_KHR_debug");
  FAIL_INIT_IF_NOT(!attrib_helper.fail_if_major_perf_caveat ||
                       !feature_info_->feature_flags().is_swiftshader_for_webgl,
                   "fail_if_major_perf_caveat + swiftshader");
  FAIL_INIT_IF_NOT(!attrib_helper.enable_oop_rasterization,
                   "oop rasterization not supported");
  FAIL_INIT_IF_NOT(!IsES31ForTestingContextType(attrib_helper.context_type) ||
                       feature_info_->gl_version_info().IsAtLeastGLES(3, 1),
                   "ES 3.1 context type requires an ES 3.1 ANGLE context");

#undef FAIL_INIT_IF_NOT

  bind_generates_resource_ = group_->bind_generates_resource();

  resources_ = group_->passthrough_resources();

  // Query information about the texture units
  GLint num_texture_units = 0;
  api()->glGetIntegervFn(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                         &num_texture_units);
  if (num_texture_units > static_cast<GLint>(kMaxTextureUnits)) {
    Destroy(true);
    LOG(ERROR) << "kMaxTextureUnits (" << kMaxTextureUnits
               << ") must be at least GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS ("
               << num_texture_units << ").";
    return gpu::ContextResult::kFatalFailure;
  }

  active_texture_unit_ = 0;

  // Initialize the tracked buffer bindings
  bound_buffers_[GL_ARRAY_BUFFER] = 0;
  bound_buffers_[GL_ELEMENT_ARRAY_BUFFER] = 0;
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 0) ||
      feature_info_->feature_flags().ext_pixel_buffer_object) {
    bound_buffers_[GL_PIXEL_PACK_BUFFER] = 0;
    bound_buffers_[GL_PIXEL_UNPACK_BUFFER] = 0;
  }
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 0)) {
    bound_buffers_[GL_COPY_READ_BUFFER] = 0;
    bound_buffers_[GL_COPY_WRITE_BUFFER] = 0;
    bound_buffers_[GL_TRANSFORM_FEEDBACK_BUFFER] = 0;
    bound_buffers_[GL_UNIFORM_BUFFER] = 0;
  }
  if (feature_info_->gl_version_info().IsAtLeastGLES(3, 1)) {
    bound_buffers_[GL_ATOMIC_COUNTER_BUFFER] = 0;
    bound_buffers_[GL_SHADER_STORAGE_BUFFER] = 0;
    bound_buffers_[GL_DRAW_INDIRECT_BUFFER] = 0;
    bound_buffers_[GL_DISPATCH_INDIRECT_BUFFER] = 0;
  }
  bound_element_array_buffer_dirty_ = false;

  lose_context_when_out_of_memory_ =
      attrib_helper.lose_context_when_out_of_memory;

  GLint max_2d_texture_size = 0;
  api()->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_2d_texture_size);
  api()->glGetIntegervFn(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer_size_);
  max_offscreen_framebuffer_size_ =
      std::min(max_2d_texture_size, max_renderbuffer_size_);

  if (offscreen_) {
    emulated_default_framebuffer_format_ = GL_RGB;

    CheckErrorCallbackState();
    emulated_back_buffer_ = std::make_unique<EmulatedDefaultFramebuffer>(this);
    // Some ChromeOS platforms (particularly MediaTek devices), there are driver
    // limitations on the minimum size of a buffer. Thus, we set the initial
    // size to 64x64 here instead of 1x1.
    gfx::Size initial_size(64, 64);
    if (!emulated_back_buffer_->Initialize(initial_size)) {
      bool was_lost = CheckResetStatus();
      Destroy(true);
      LOG(ERROR) << (was_lost ? "ContextResult::kTransientFailure: "
                              : "ContextResult::kFatalFailure: ")
                 << "Resize of emulated back buffer failed";
      return was_lost ? gpu::ContextResult::kTransientFailure
                      : gpu::ContextResult::kFatalFailure;
    }

    if (CheckErrorCallbackState()) {
      Destroy(true);
      // Errors are considered fatal, including OOM.
      LOG(ERROR)
          << "ContextResult::kFatalFailure: "
             "Creation of the offscreen framebuffer failed because errors were "
             "generated.";
      return gpu::ContextResult::kFatalFailure;
    }

    framebuffer_id_map_.SetIDMapping(
        0, emulated_back_buffer_->framebuffer_service_id);

    // Bind the emulated default framebuffer and initialize the viewport
    api()->glBindFramebufferEXTFn(
        GL_FRAMEBUFFER, emulated_back_buffer_->framebuffer_service_id);
    api()->glViewportFn(0, 0, initial_size.width(), initial_size.height());
  }

#if BUILDFLAG(IS_MAC)
  // On mac we need the ANGLE_texture_rectangle extension to support IOSurface
  // backbuffers, but we don't want it exposed to WebGL user shaders. This
  // disables support for it in the shader compiler. We then enable it
  // temporarily when necessary; see
  // ScopedEnableTextureRectangleInShaderCompiler.
  if (feature_info_->IsWebGLContext())
    api()->glDisableFn(GL_TEXTURE_RECTANGLE_ANGLE);
#endif

  // Register this object as a GPU switching observer.
  if (feature_info_->IsWebGLContext()) {
    ui::GpuSwitchingManager::GetInstance()->AddObserver(this);
  }

  // Deprecation warning for SwiftShader WebGL fallback
  if (feature_info_->IsWebGLContext() &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kSwiftShader &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableUnsafeSwiftShader)) {
    constexpr const char* kSwiftShaderFallbackDeprcationMessage =
        "Automatic fallback to software WebGL has been deprecated. Please use "
        "the --enable-unsafe-swiftshader flag to opt in to lower security "
        "guarantees for trusted content.";
    logger_.LogMessage(__FILE__, __LINE__,
                       kSwiftShaderFallbackDeprcationMessage);
  }

  set_initialized();
  return gpu::ContextResult::kSuccess;
}

void GLES2DecoderPassthroughImpl::Destroy(bool have_context) {
  if (have_context) {
    FlushErrors();
  }

  // Destroy all pending read pixels operations
  for (PendingReadPixels& pending_read_pixels : pending_read_pixels_) {
    if (have_context) {
      api()->glDeleteBuffersARBFn(1, &pending_read_pixels.buffer_service_id);
    } else {
      pending_read_pixels.fence->Invalidate();
    }
  }
  pending_read_pixels_.clear();

  for (auto& bound_texture_type : bound_textures_) {
    for (auto& bound_texture : bound_texture_type) {
      if (!have_context && bound_texture.texture) {
        bound_texture.texture->MarkContextLost();
      }
      bound_texture.texture = nullptr;
    }
  }

  for (PendingQuery& pending_query : pending_queries_) {
    if (!have_context) {
      if (pending_query.commands_completed_fence) {
        pending_query.commands_completed_fence->Invalidate();
      }
      if (pending_query.buffer_shadow_update_fence) {
        pending_query.buffer_shadow_update_fence->Invalidate();
      }
    }
  }
  pending_queries_.clear();

  for (PendingReadPixels& pending_read_pixels : pending_read_pixels_) {
    if (!have_context) {
      if (pending_read_pixels.fence) {
        pending_read_pixels.fence->Invalidate();
      }
    }
  }
  pending_read_pixels_.clear();

  DeleteServiceObjects(&framebuffer_id_map_, have_context,
                       [this](GLuint client_id, GLuint framebuffer) {
                         api()->glDeleteFramebuffersEXTFn(1, &framebuffer);
                       });
  DeleteServiceObjects(&transform_feedback_id_map_, have_context,
                       [this](GLuint client_id, GLuint transform_feedback) {
                         api()->glDeleteTransformFeedbacksFn(
                             1, &transform_feedback);
                       });
  DeleteServiceObjects(
      &query_id_map_, have_context, [this](GLuint client_id, GLuint query) {
        // glDeleteQueries is not loaded unless GL_EXT_occlusion_query_boolean
        // is present. All queries must be emulated so they don't need to be
        // deleted.
        if (feature_info_->feature_flags().occlusion_query_boolean) {
          api()->glDeleteQueriesFn(1, &query);
        }
      });
  DeleteServiceObjects(&vertex_array_id_map_, have_context,
                       [this](GLuint client_id, GLuint vertex_array) {
                         api()->glDeleteVertexArraysOESFn(1, &vertex_array);
                       });

  // Destroy the emulated backbuffer
  if (emulated_back_buffer_) {
    emulated_back_buffer_->Destroy(have_context);
    emulated_back_buffer_.reset();
  }

  if (external_default_framebuffer_) {
    external_default_framebuffer_->Destroy(have_context);
    external_default_framebuffer_.reset();
  }

  if (gpu_fence_manager_.get()) {
    gpu_fence_manager_->Destroy(have_context);
    gpu_fence_manager_.reset();
  }

  // Destroy the GPU Tracer which may own some in process GPU Timings.
  if (gpu_tracer_) {
    gpu_tracer_->Destroy(have_context);
    gpu_tracer_.reset();
  }

  if (multi_draw_manager_.get()) {
    multi_draw_manager_.reset();
  }

  if (!have_context) {
    for (auto& fence : deschedule_until_finished_fences_) {
      fence->Invalidate();
    }
  }
  deschedule_until_finished_fences_.clear();

  // Unregister this object as a GPU switching observer.
  if (feature_info_->IsWebGLContext()) {
    ui::GpuSwitchingManager::GetInstance()->RemoveObserver(this);
  }

  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (group_) {
    // `resources_` depends on `group_`. It must be cleared before we set
    // `group_` to nullptr.
    resources_ = nullptr;
    group_->Destroy(this, have_context);
    group_ = nullptr;
  }

  if (have_context) {
    api()->glDebugMessageCallbackFn(nullptr, nullptr);
  }

  if (context_.get()) {
    context_->ReleaseCurrent(nullptr);
    //`api_` might depend on `context_`. It must be cleared before we set
    //`context_` to nullptr.
    api_ = nullptr;
    context_ = nullptr;
  }
}

void GLES2DecoderPassthroughImpl::SetSurface(
    const scoped_refptr<gl::GLSurface>& surface) {
  DCHECK(context_->IsCurrent(nullptr));
  DCHECK(surface_.get());
  surface_ = surface;
}

void GLES2DecoderPassthroughImpl::ReleaseSurface() {
  if (!context_.get())
    return;
  if (WasContextLost()) {
    DLOG(ERROR) << "  GLES2DecoderImpl: Trying to release lost context.";
    return;
  }
  context_->ReleaseCurrent(surface_.get());
  surface_ = nullptr;
}

void GLES2DecoderPassthroughImpl::SetDefaultFramebufferSharedImage(
    const Mailbox& mailbox,
    int samples,
    bool preserve,
    bool needs_depth,
    bool needs_stencil) {
  if (!offscreen_)
    return;

  if (!external_default_framebuffer_) {
    external_default_framebuffer_ = std::make_unique<GLES2ExternalFramebuffer>(
        /*passthrough=*/true, *group_->feature_info(),
        group_->shared_image_representation_factory());
  }

  if (!external_default_framebuffer_->AttachSharedImage(
          mailbox, samples, preserve, needs_depth, needs_stencil)) {
    return;
  }

  GLuint default_framebuffer_id;
  if (external_default_framebuffer_->IsSharedImageAttached()) {
    default_framebuffer_id = external_default_framebuffer_->GetFramebufferId();
  } else {
    default_framebuffer_id = emulated_back_buffer_->framebuffer_service_id;
  }

  framebuffer_id_map_.RemoveClientID(0);
  framebuffer_id_map_.SetIDMapping(0, default_framebuffer_id);

  // Note, there is member variable `supports_separate_fbo_bindings_` that is
  // used across this class, but it's never initialized with the real value
  // (defaults to false) which is likely a bug. To avoid any code changes
  // outside of the feature flag we don't use it here.
  const bool supports_separate_fbo_bindings =
      feature_info_->feature_flags().chromium_framebuffer_multisample ||
      feature_info_->IsWebGL2OrES3Context();

  if (supports_separate_fbo_bindings) {
    if (bound_draw_framebuffer_ == 0) {
      api()->glBindFramebufferEXTFn(GL_DRAW_FRAMEBUFFER,
                                    default_framebuffer_id);
    }
    if (bound_read_framebuffer_ == 0) {
      api()->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER,
                                    default_framebuffer_id);
    }
  } else {
    DCHECK_EQ(bound_draw_framebuffer_, bound_read_framebuffer_);
    if (bound_draw_framebuffer_ == 0) {
      api()->glBindFramebufferEXTFn(GL_FRAMEBUFFER, default_framebuffer_id);
    }
  }
}

bool GLES2DecoderPassthroughImpl::MakeCurrent() {
  if (!context_.get())
    return false;

  if (WasContextLost()) {
    LOG(ERROR) << "  GLES2DecoderPassthroughImpl: Trying to make lost context "
                  "current.";
    return false;
  }

  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR)
        << "  GLES2DecoderPassthroughImpl: Context lost during MakeCurrent.";
    MarkContextLost(error::kMakeCurrentFailed);
    group_->LoseContexts(error::kUnknown);
    return false;
  }
  DCHECK_EQ(api(), gl::g_current_gl_context);

  if (CheckResetStatus()) {
    LOG(ERROR) << "  GLES2DecoderPassthroughImpl: Context reset detected after "
                  "MakeCurrent.";
    group_->LoseContexts(error::kUnknown);
    return false;
  }

  ProcessReadPixels(false);
  ProcessQueries(false);

  return true;
}

gpu::gles2::GLES2Util* GLES2DecoderPassthroughImpl::GetGLES2Util() {
  return nullptr;
}

gl::GLContext* GLES2DecoderPassthroughImpl::GetGLContext() {
  return context_.get();
}

gl::GLSurface* GLES2DecoderPassthroughImpl::GetGLSurface() {
  return surface_.get();
}

gpu::gles2::ContextGroup* GLES2DecoderPassthroughImpl::GetContextGroup() {
  return group_.get();
}

const FeatureInfo* GLES2DecoderPassthroughImpl::GetFeatureInfo() const {
  return group_->feature_info();
}

gpu::Capabilities GLES2DecoderPassthroughImpl::GetCapabilities() {
  DCHECK(initialized());
  Capabilities caps;

  PopulateNumericCapabilities(&caps, feature_info_.get());

  caps.egl_image_external =
      feature_info_->feature_flags().oes_egl_image_external;
  caps.egl_image_external_essl3 =
      feature_info_->feature_flags().oes_egl_image_external_essl3;
  caps.texture_format_bgra8888 =
      feature_info_->feature_flags().ext_texture_format_bgra8888;

  caps.texture_format_etc1_npot =
      feature_info_->feature_flags().oes_compressed_etc1_rgb8_texture &&
      !feature_info_->workarounds().etc1_power_of_two_only;
  // Vulkan currently doesn't support single-component cross-thread shared
  // images.
  caps.disable_one_component_textures =
      group_->shared_image_manager() &&
      group_->shared_image_manager()->display_context_on_another_thread() &&
      (feature_info_->workarounds().avoid_one_component_egl_images ||
       features::IsUsingVulkan());
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;
  caps.texture_rg = feature_info_->feature_flags().ext_texture_rg;
  caps.texture_norm16 = feature_info_->feature_flags().ext_texture_norm16;
  caps.texture_half_float_linear =
      feature_info_->feature_flags().enable_texture_half_float_linear;
  caps.image_ycbcr_420v =
      feature_info_->feature_flags().chromium_image_ycbcr_420v;
  caps.image_ar30 = feature_info_->feature_flags().chromium_image_ar30;
  caps.image_ab30 = feature_info_->feature_flags().chromium_image_ab30;
  caps.image_ycbcr_p010 =
      feature_info_->feature_flags().chromium_image_ycbcr_p010;
  if (feature_info_->workarounds().webgl_or_caps_max_texture_size) {
    caps.max_texture_size =
        std::min(caps.max_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
  }
  caps.max_copy_texture_chromium_size =
      feature_info_->workarounds().max_copy_texture_chromium_size;
  caps.render_buffer_format_bgra8888 =
      feature_info_->feature_flags().ext_render_buffer_format_bgra8888;
  caps.gpu_rasterization = false;
  caps.msaa_is_slow = MSAAIsSlow(feature_info_->workarounds());
  caps.avoid_stencil_buffers =
      feature_info_->workarounds().avoid_stencil_buffers;
  caps.supports_rgb_to_yuv_conversion = true;
  // Technically, YUV readback is handled on the client side, but enable it here
  // so that clients can use this to detect support.
  caps.supports_yuv_readback = true;
  caps.chromium_gpu_fence = feature_info_->feature_flags().chromium_gpu_fence;
  caps.mesa_framebuffer_flip_y =
      feature_info_->feature_flags().mesa_framebuffer_flip_y;

  caps.gpu_memory_buffer_formats =
      feature_info_->feature_flags().gpu_memory_buffer_formats;
  caps.angle_rgbx_internal_format =
      feature_info_->feature_flags().angle_rgbx_internal_format;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  PopulateDRMCapabilities(&caps, feature_info_.get());
#endif

  return caps;
}

gpu::GLCapabilities GLES2DecoderPassthroughImpl::GetGLCapabilities() {
  CHECK(initialized());
  GLCapabilities caps;

  PopulateGLCapabilities(&caps, feature_info_.get());
  CHECK_EQ(caps.bind_generates_resource_chromium != GL_FALSE,
           group_->bind_generates_resource());
  caps.occlusion_query_boolean =
      feature_info_->feature_flags().occlusion_query_boolean;
  caps.timer_queries = feature_info_->feature_flags().ext_disjoint_timer_query;

  if (feature_info_->workarounds().webgl_or_caps_max_texture_size) {
    caps.max_texture_size =
        std::min(caps.max_texture_size,
                 feature_info_->workarounds().webgl_or_caps_max_texture_size);
  }
  caps.sync_query = feature_info_->feature_flags().chromium_sync_query;

  return caps;
}

void GLES2DecoderPassthroughImpl::RestoreState(const ContextState* prev_state) {
}

void GLES2DecoderPassthroughImpl::RestoreActiveTexture() const {}

void GLES2DecoderPassthroughImpl::RestoreAllTextureUnitAndSamplerBindings(
    const ContextState* prev_state) const {}

void GLES2DecoderPassthroughImpl::RestoreActiveTextureUnitBinding(
    unsigned int target) const {}

void GLES2DecoderPassthroughImpl::RestoreBufferBinding(unsigned int target) {}

void GLES2DecoderPassthroughImpl::RestoreBufferBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreFramebufferBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreRenderbufferBindings() {}

void GLES2DecoderPassthroughImpl::RestoreGlobalState() const {}

void GLES2DecoderPassthroughImpl::RestoreProgramBindings() const {}

void GLES2DecoderPassthroughImpl::RestoreTextureState(unsigned service_id) {}

void GLES2DecoderPassthroughImpl::RestoreTextureUnitBindings(
    unsigned unit) const {}

void GLES2DecoderPassthroughImpl::RestoreVertexAttribArray(unsigned index) {}

void GLES2DecoderPassthroughImpl::RestoreAllExternalTextureBindingsIfNeeded() {}

void GLES2DecoderPassthroughImpl::RestoreDeviceWindowRectangles() const {}

void GLES2DecoderPassthroughImpl::ClearAllAttributes() const {}

void GLES2DecoderPassthroughImpl::RestoreAllAttributes() const {}

void GLES2DecoderPassthroughImpl::SetIgnoreCachedStateForTest(bool ignore) {}

void GLES2DecoderPassthroughImpl::SetForceShaderNameHashingForTest(bool force) {
}

gpu::QueryManager* GLES2DecoderPassthroughImpl::GetQueryManager() {
  return nullptr;
}

void GLES2DecoderPassthroughImpl::SetQueryCallback(unsigned int query_client_id,
                                                   base::OnceClosure callback) {
  GLuint service_id = query_id_map_.GetServiceIDOrInvalid(query_client_id);
  for (auto& pending_query : pending_queries_) {
    if (pending_query.service_id == service_id) {
      pending_query.callbacks.push_back(std::move(callback));
      return;
    }
  }

  VLOG(1) << "GLES2DecoderPassthroughImpl::SetQueryCallback: No pending query "
             "with ID "
          << query_client_id << ". Running the callback immediately.";
  std::move(callback).Run();
}

void GLES2DecoderPassthroughImpl::CancelAllQueries() {
  // clear all pending queries.
  pending_queries_.clear();
  query_id_map_.Clear();
}
gpu::gles2::GpuFenceManager* GLES2DecoderPassthroughImpl::GetGpuFenceManager() {
  return gpu_fence_manager_.get();
}

gpu::gles2::FramebufferManager*
GLES2DecoderPassthroughImpl::GetFramebufferManager() {
  return nullptr;
}

gpu::gles2::TransformFeedbackManager*
GLES2DecoderPassthroughImpl::GetTransformFeedbackManager() {
  return nullptr;
}

gpu::gles2::VertexArrayManager*
GLES2DecoderPassthroughImpl::GetVertexArrayManager() {
  return nullptr;
}

bool GLES2DecoderPassthroughImpl::HasPendingQueries() const {
  return !pending_queries_.empty();
}

void GLES2DecoderPassthroughImpl::ProcessPendingQueries(bool did_finish) {
  // TODO(geofflang): If this returned an error, store it somewhere.
  ProcessQueries(did_finish);
}

bool GLES2DecoderPassthroughImpl::HasMoreIdleWork() const {
  return gpu_tracer_->HasTracesToProcess() || !pending_read_pixels_.empty();
}

void GLES2DecoderPassthroughImpl::PerformIdleWork() {
  gpu_tracer_->ProcessTraces();
  ProcessReadPixels(false);
}

bool GLES2DecoderPassthroughImpl::HasPollingWork() const {
  return deschedule_until_finished_fences_.size() >= 2;
}

void GLES2DecoderPassthroughImpl::PerformPollingWork() {
  ProcessDescheduleUntilFinished();
}

bool GLES2DecoderPassthroughImpl::GetServiceTextureId(
    uint32_t client_texture_id,
    uint32_t* service_texture_id) {
  return resources_->texture_id_map.GetServiceID(client_texture_id,
                                                 service_texture_id);
}

TextureBase* GLES2DecoderPassthroughImpl::GetTextureBase(uint32_t client_id) {
  scoped_refptr<TexturePassthrough> texture;
  if (resources_->texture_object_map.GetServiceID(client_id, &texture)) {
    return texture.get();
  } else {
    return nullptr;
  }
}

bool GLES2DecoderPassthroughImpl::ClearLevel(Texture* texture,
                                             unsigned target,
                                             int level,
                                             unsigned format,
                                             unsigned type,
                                             int xoffset,
                                             int yoffset,
                                             int width,
                                             int height) {
  return true;
}

bool GLES2DecoderPassthroughImpl::ClearCompressedTextureLevel(Texture* texture,
                                                              unsigned target,
                                                              int level,
                                                              unsigned format,
                                                              int width,
                                                              int height) {
  return true;
}

bool GLES2DecoderPassthroughImpl::ClearCompressedTextureLevel3D(
    Texture* texture,
    unsigned target,
    int level,
    unsigned format,
    int width,
    int height,
    int depth) {
  return true;
}

bool GLES2DecoderPassthroughImpl::IsCompressedTextureFormat(unsigned format) {
  return false;
}

bool GLES2DecoderPassthroughImpl::ClearLevel3D(Texture* texture,
                                               unsigned target,
                                               int level,
                                               unsigned format,
                                               unsigned type,
                                               int width,
                                               int height,
                                               int depth) {
  return true;
}

gpu::gles2::ErrorState* GLES2DecoderPassthroughImpl::GetErrorState() {
  return nullptr;
}

void GLES2DecoderPassthroughImpl::WaitForReadPixels(
    base::OnceClosure callback) {}

bool GLES2DecoderPassthroughImpl::WasContextLost() const {
  return context_lost_;
}

bool GLES2DecoderPassthroughImpl::WasContextLostByRobustnessExtension() const {
  return WasContextLost() && reset_by_robustness_extension_;
}

void GLES2DecoderPassthroughImpl::MarkContextLost(
    error::ContextLostReason reason) {
  // Only lose the context once.
  if (WasContextLost()) {
    return;
  }

  // Don't make GL calls in here, the context might not be current.
  command_buffer_service()->SetContextLostReason(reason);
  context_lost_ = true;
}

gpu::gles2::Logger* GLES2DecoderPassthroughImpl::GetLogger() {
  return &logger_;
}

void GLES2DecoderPassthroughImpl::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  // Send OnGpuSwitched notification to renderer process via decoder client.
  client()->OnGpuSwitched(active_gpu_heuristic);
}

void GLES2DecoderPassthroughImpl::BeginDecoding() {
  gpu_tracer_->BeginDecoding();
  gpu_trace_commands_ = gpu_tracer_->IsTracing() && *gpu_decoder_category_;
  gpu_debug_commands_ = log_commands() || debug() || gpu_trace_commands_;

#if BUILDFLAG(IS_WIN)
  if (!resources_->ResumeSharedImageAccessIfNeeded(api())) {
    LOG(ERROR) << "  GLES2DecoderPassthroughImpl: Failed to resume shared "
                  "image access.";
    group_->LoseContexts(error::kUnknown);
  }
#endif

  auto it = active_queries_.find(GL_COMMANDS_ISSUED_CHROMIUM);
  if (it != active_queries_.end()) {
    DCHECK_EQ(it->second.command_processing_start_time, base::TimeTicks());
    it->second.command_processing_start_time = base::TimeTicks::Now();
  }

  if (has_activated_pixel_local_storage_) {
    api()->glFramebufferPixelLocalStorageRestoreANGLEFn();
  }
}

void GLES2DecoderPassthroughImpl::EndDecoding() {
  if (has_activated_pixel_local_storage_) {
    api()->glFramebufferPixelLocalStorageInterruptANGLEFn();
  }

#if BUILDFLAG(IS_WIN)
  resources_->SuspendSharedImageAccessIfNeeded();
#endif

  gpu_tracer_->EndDecoding();

  auto it = active_queries_.find(GL_COMMANDS_ISSUED_CHROMIUM);
  if (it != active_queries_.end()) {
    DCHECK_NE(it->second.command_processing_start_time, base::TimeTicks());
    it->second.active_time +=
        (base::TimeTicks::Now() - it->second.command_processing_start_time);
    it->second.command_processing_start_time = base::TimeTicks();
  }
}

const gpu::gles2::ContextState* GLES2DecoderPassthroughImpl::GetContextState() {
  return nullptr;
}

scoped_refptr<ShaderTranslatorInterface>
GLES2DecoderPassthroughImpl::GetTranslator(GLenum type) {
  return nullptr;
}

void GLES2DecoderPassthroughImpl::OnDebugMessage(GLenum source,
                                                 GLenum type,
                                                 GLuint id,
                                                 GLenum severity,
                                                 GLsizei length,
                                                 const GLchar* message) {
  if (type == GL_DEBUG_TYPE_ERROR && source == GL_DEBUG_SOURCE_API) {
    had_error_callback_ = true;
  }
}

void GLES2DecoderPassthroughImpl::SetCopyTextureResourceManagerForTest(
    CopyTextureCHROMIUMResourceManager* copy_texture_resource_manager) {
  NOTIMPLEMENTED();
}

void GLES2DecoderPassthroughImpl::SetCopyTexImageBlitterForTest(
    CopyTexImageResourceManager* copy_tex_image_blit) {
  NOTIMPLEMENTED();
}

const char* GLES2DecoderPassthroughImpl::GetCommandName(
    unsigned int command_id) const {
  if (command_id >= kFirstGLES2Command && command_id < kNumCommands) {
    return gles2::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

void GLES2DecoderPassthroughImpl::SetOptionalExtensionsRequestedForTesting(
    bool request_extensions) {
  request_optional_extensions_ = request_extensions;
}

void GLES2DecoderPassthroughImpl::InitializeFeatureInfo(
    ContextType context_type,
    const DisallowedFeatures& disallowed_features,
    bool force_reinitialize) {
  feature_info_->Initialize(context_type, true /* is_passthrough_cmd_decoder */,
                            disallowed_features, force_reinitialize);
}

template <typename T>
error::Error GLES2DecoderPassthroughImpl::PatchGetNumericResults(GLenum pname,
                                                                 GLsizei length,
                                                                 T* params) {
  // Likely a gl error if no parameters were returned
  if (length < 1) {
    return error::kNoError;
  }

  switch (pname) {
    case GL_NUM_EXTENSIONS:
      // Currently handled on the client side.
      params[0] = 0;
      break;

    case GL_TEXTURE_BINDING_2D:
    case GL_TEXTURE_BINDING_CUBE_MAP:
    case GL_TEXTURE_BINDING_2D_ARRAY:
    case GL_TEXTURE_BINDING_3D:
      if (*params != 0 &&
          !GetClientID(&resources_->texture_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_ARRAY_BUFFER_BINDING:
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
    case GL_PIXEL_PACK_BUFFER_BINDING:
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
    case GL_COPY_READ_BUFFER_BINDING:
    case GL_COPY_WRITE_BUFFER_BINDING:
    case GL_UNIFORM_BUFFER_BINDING:
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
    case GL_DRAW_INDIRECT_BUFFER_BINDING:
      if (*params != 0 &&
          !GetClientID(&resources_->buffer_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_RENDERBUFFER_BINDING:
      if (*params != 0 &&
          !GetClientID(&resources_->renderbuffer_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_SAMPLER_BINDING:
      if (*params != 0 &&
          !GetClientID(&resources_->sampler_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_ACTIVE_PROGRAM:
      if (*params != 0 &&
          !GetClientID(&resources_->program_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_FRAMEBUFFER_BINDING:
    case GL_READ_FRAMEBUFFER_BINDING:
      if (*params != 0 && !GetClientID(&framebuffer_id_map_, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_TRANSFORM_FEEDBACK_BINDING:
      if (*params != 0 &&
          !GetClientID(&transform_feedback_id_map_, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_VERTEX_ARRAY_BINDING:
      if (*params != 0 &&
          !GetClientID(&vertex_array_id_map_, *params, params)) {
        return error::kInvalidArguments;
      }
      break;

    case GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE:
      // Impose an upper bound on the number ANGLE_shader_pixel_local_storage
      // planes so we can stack-allocate load/store ops.
      *params = std::min<T>(*params, kPassthroughMaxPLSPlanes);
      break;

    default:
      break;
  }

  return error::kNoError;
}

// Instantiate templated functions
#define INSTANTIATE_PATCH_NUMERIC_RESULTS(type)                              \
  template error::Error GLES2DecoderPassthroughImpl::PatchGetNumericResults( \
      GLenum, GLsizei, type*)
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLint);
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLint64);
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLfloat);
INSTANTIATE_PATCH_NUMERIC_RESULTS(GLboolean);
#undef INSTANTIATE_PATCH_NUMERIC_RESULTS

template <typename T>
error::Error GLES2DecoderPassthroughImpl::PatchGetBufferResults(GLenum target,
                                                                GLenum pname,
                                                                GLsizei bufsize,
                                                                GLsizei* length,
                                                                T* params) {
  if (pname != GL_BUFFER_ACCESS_FLAGS) {
    return error::kNoError;
  }

  // If there was no error, the buffer target should exist
  DCHECK(bound_buffers_.find(target) != bound_buffers_.end());
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    LazilyUpdateCurrentlyBoundElementArrayBuffer();
  }
  GLuint current_client_buffer = bound_buffers_[target];

  auto mapped_buffer_info_iter =
      resources_->mapped_buffer_map.find(current_client_buffer);
  if (mapped_buffer_info_iter == resources_->mapped_buffer_map.end()) {
    // Buffer is not mapped, nothing to do
    return error::kNoError;
  }

  // Buffer is mapped, patch the result with the original access flags
  DCHECK_GE(bufsize, 1);
  DCHECK_EQ(*length, 1);
  params[0] = mapped_buffer_info_iter->second.original_access;
  return error::kNoError;
}

template error::Error GLES2DecoderPassthroughImpl::PatchGetBufferResults(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint64* params);
template error::Error GLES2DecoderPassthroughImpl::PatchGetBufferResults(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params);

error::Error GLES2DecoderPassthroughImpl::
    PatchGetFramebufferPixelLocalStorageParameterivANGLE(GLint plane,
                                                         GLenum pname,
                                                         GLsizei length,
                                                         GLint* params) {
  // Likely a gl error if no parameters were returned
  if (length < 1) {
    return error::kNoError;
  }

  switch (pname) {
    case GL_PIXEL_LOCAL_TEXTURE_NAME_ANGLE:
      if (*params != 0 &&
          !GetClientID(&resources_->texture_id_map, *params, params)) {
        return error::kInvalidArguments;
      }
      break;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::PatchGetFramebufferAttachmentParameter(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei length,
    GLint* params) {
  // Likely a gl error if no parameters were returned
  if (length < 1) {
    return error::kNoError;
  }

  switch (pname) {
    // If the attached object name was requested, it needs to be converted back
    // to a client id.
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: {
      GLint object_type = GL_NONE;
      api()->glGetFramebufferAttachmentParameterivEXTFn(
          target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
          &object_type);

      switch (object_type) {
        case GL_TEXTURE:
          if (!GetClientID(&resources_->texture_id_map, *params, params)) {
            return error::kInvalidArguments;
          }
          break;

        case GL_RENDERBUFFER:
          if (!GetClientID(&resources_->renderbuffer_id_map, *params, params)) {
            return error::kInvalidArguments;
          }
          break;

        case GL_NONE:
          // Default framebuffer, don't transform the result
          break;

        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    } break;

    // If the framebuffer is an emulated default framebuffer, all attachment
    // object types are GL_FRAMEBUFFER_DEFAULT
    case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
      if (IsEmulatedFramebufferBound(target)) {
        *params = GL_FRAMEBUFFER_DEFAULT;
      }
      break;

    default:
      break;
  }

  return error::kNoError;
}

// static
std::unique_ptr<GLES2DecoderPassthroughImpl::LazySharedContextState>
GLES2DecoderPassthroughImpl::LazySharedContextState::Create(
    GLES2DecoderPassthroughImpl* impl) {
  auto context =
      std::make_unique<GLES2DecoderPassthroughImpl::LazySharedContextState>(
          impl);
  if (!context->Initialize()) {
    return nullptr;
  }
  return context;
}

GLES2DecoderPassthroughImpl::LazySharedContextState::LazySharedContextState(
    GLES2DecoderPassthroughImpl* impl)
    : impl_(impl) {}

GLES2DecoderPassthroughImpl::LazySharedContextState::~LazySharedContextState() {
  if (shared_context_state_) {
    ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(impl_);
    ui::ScopedMakeCurrent smc(shared_context_state_->context(),
                              shared_context_state_->surface());
    shared_context_state_.reset();
  }
}

bool GLES2DecoderPassthroughImpl::LazySharedContextState::Initialize() {
  auto gl_surface = gl::init::CreateOffscreenGLSurface(
      impl_->context_->GetGLDisplayEGL(), gfx::Size());
  if (!gl_surface) {
    impl_->InsertError(
        GL_INVALID_OPERATION,
        "ContextResult::kFatalFailure: Failed to create GL Surface "
        "for SharedContextState");
    return false;
  }

  gl::GLContextAttribs attribs;
  attribs.bind_generates_resource = false;
  attribs.global_texture_share_group = true;
  attribs.global_semaphore_share_group = true;
  attribs.robust_resource_initialization = true;
  attribs.robust_buffer_access = true;
  attribs.allow_client_arrays = false;
  auto gl_context = gl::init::CreateGLContext(impl_->context_->share_group(),
                                              gl_surface.get(), attribs);
  if (!gl_context) {
    LOG(ERROR) << "Failed to create GLES3 context, fallback to GLES2.";
    attribs.client_major_es_version = 2;
    attribs.client_minor_es_version = 0;
    gl_context = gl::init::CreateGLContext(impl_->context_->share_group(),
                                           gl_surface.get(), attribs);
  }
  if (!gl_context) {
    impl_->InsertError(
        GL_INVALID_OPERATION,
        "ContextResult::kFatalFailure: Failed to create GL Context "
        "for SharedContextState");
    return false;
  }

  // Make current context using `gl_context` and `gl_surface`
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(impl_);
  ui::ScopedMakeCurrent smc(gl_context.get(), gl_surface.get());

  ContextGroup* group = impl_->GetContextGroup();
  const GpuPreferences& gpu_preferences = group->gpu_preferences();
  const GpuDriverBugWorkarounds& workarounds =
      group->feature_info()->workarounds();

  // TODO(crbug.com/40064510): Add copying shared image to GL Texture support
  // within Graphite.
  shared_context_state_ = base::MakeRefCounted<SharedContextState>(
      impl_->context_->share_group(), std::move(gl_surface),
      std::move(gl_context),
      /*use_virtualized_gl_contexts=*/false, base::DoNothing(),
      GrContextType::kGL);
  auto feature_info = base::MakeRefCounted<gles2::FeatureInfo>(
      workarounds, group->gpu_feature_info());
  if (!shared_context_state_->InitializeGL(gpu_preferences, feature_info)) {
    impl_->InsertError(GL_INVALID_OPERATION,
                       "ContextResult::kFatalFailure: Failed to Initialize GL "
                       "for SharedContextState");
    return false;
  }
  if (!shared_context_state_->InitializeSkia(gpu_preferences, workarounds,
                                             /*cache=*/nullptr)) {
    impl_->InsertError(GL_INVALID_OPERATION,
                       "ContextResult::kFatalFailure: Failed to Initialize "
                       "Skia for SharedContextState");
    return false;
  }
  return true;
}

void GLES2DecoderPassthroughImpl::InsertError(GLenum error,
                                              const std::string& message) {
  errors_.insert(error);
  LogGLDebugMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, error,
                    GL_DEBUG_SEVERITY_HIGH, message.length(), message.c_str(),
                    GetLogger());
}

GLenum GLES2DecoderPassthroughImpl::PopError() {
  GLenum error = GL_NO_ERROR;
  if (!errors_.empty()) {
    error = *errors_.begin();
    errors_.erase(errors_.begin());
  }
  return error;
}

bool GLES2DecoderPassthroughImpl::FlushErrors() {
  bool had_error = false;
  GLenum error = glGetError();
  while (error != GL_NO_ERROR) {
    errors_.insert(error);
    had_error = true;

    // Check for context loss on out-of-memory errors
    if (error == GL_OUT_OF_MEMORY && !WasContextLost() &&
        lose_context_when_out_of_memory_) {
      error::ContextLostReason other = error::kOutOfMemory;
      if (CheckResetStatus()) {
        other = error::kUnknown;
      } else {
        // Need to lose current context before broadcasting!
        MarkContextLost(error::kOutOfMemory);
      }
      group_->LoseContexts(other);
      break;
    }

    error = glGetError();
  }
  return had_error;
}

bool GLES2DecoderPassthroughImpl::IsIgnoredCap(GLenum cap) const {
  switch (cap) {
    case GL_DEBUG_OUTPUT:
      return true;

    default:
      return false;
  }
}

bool GLES2DecoderPassthroughImpl::CheckResetStatus() {
  DCHECK(!WasContextLost());
  DCHECK(context_->IsCurrent(nullptr));

  // If the reason for the call was a GL error, we can try to determine the
  // reset status more accurately.
  GLenum driver_status = context_->CheckStickyGraphicsResetStatus();
  if (driver_status == GL_NO_ERROR) {
    return false;
  }

  switch (driver_status) {
    case GL_GUILTY_CONTEXT_RESET_ARB:
      MarkContextLost(error::kGuilty);
      break;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      MarkContextLost(error::kInnocent);
      break;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      MarkContextLost(error::kUnknown);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
  reset_by_robustness_extension_ = true;
  return true;
}

bool GLES2DecoderPassthroughImpl::IsEmulatedQueryTarget(GLenum target) const {
  switch (target) {
    case GL_COMMANDS_COMPLETED_CHROMIUM:
    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    case GL_GET_ERROR_QUERY_CHROMIUM:
    case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
      return true;

    default:
      return false;
  }
}

bool GLES2DecoderPassthroughImpl::OnlyHasPendingProgramCompletionQueries() {
  return base::ranges::all_of(pending_queries_, [](const auto& query) {
    return query.target == GL_PROGRAM_COMPLETION_QUERY_CHROMIUM;
  });
}

error::Error GLES2DecoderPassthroughImpl::ProcessQueries(bool did_finish) {
  bool program_completion_query_deferred = false;
  while (!pending_queries_.empty()) {
    PendingQuery& query = pending_queries_.front();
    GLuint result_available = GL_FALSE;
    GLuint64 result = 0;
    switch (query.target) {
      case GL_COMMANDS_COMPLETED_CHROMIUM:
        DCHECK(query.commands_completed_fence != nullptr);
        // Note: |did_finish| guarantees that the GPU has passed the fence but
        // we cannot assume that GLFence::HasCompleted() will return true yet as
        // that's not guaranteed by all GLFence implementations.
        result_available =
            did_finish || query.commands_completed_fence->HasCompleted();
        result = result_available;
        break;

      case GL_COMMANDS_ISSUED_CHROMIUM:
        result_available = GL_TRUE;
        result = query.commands_issued_time.InMicroseconds();
        break;

      case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
        result_available = GL_TRUE;
        DCHECK_GT(
            query.commands_issued_timestamp.since_origin().InMicroseconds(), 0);
        result =
            query.commands_issued_timestamp.since_origin().InMicroseconds();
        break;

      case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
        // Initialize the result to being available.  Will be marked as
        // unavailable if any pending read pixels operations reference this
        // query.
        result_available = GL_TRUE;
        result = GL_TRUE;
        for (const PendingReadPixels& pending_read_pixels :
             pending_read_pixels_) {
          if (pending_read_pixels.waiting_async_pack_queries.count(
                  query.service_id) > 0) {
            // Async read pixel processing happens before query processing. If
            // there was a finish then there should be no pending read pixels.
            DCHECK(!did_finish);
            result_available = GL_FALSE;
            result = GL_FALSE;
            break;
          }
        }
        break;

      case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
        DCHECK(query.buffer_shadow_update_fence);
        if (did_finish || query.buffer_shadow_update_fence->HasCompleted()) {
          ReadBackBuffersIntoShadowCopies(query.buffer_shadow_updates);
          result_available = GL_TRUE;
          result = 0;
        }
        break;

      case GL_GET_ERROR_QUERY_CHROMIUM:
        result_available = GL_TRUE;
        FlushErrors();
        result = PopError();
        break;

      case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
        GLint status;
        if (!api()->glIsProgramFn(query.program_service_id)) {
          status = GL_TRUE;
        } else {
          api()->glGetProgramivFn(query.program_service_id,
                                  GL_COMPLETION_STATUS_KHR, &status);
        }
        result_available = (status == GL_TRUE);
        if (!result_available) {
          // Move the query to the end of queue, so that other queries may have
          // chance to be processed.
          auto temp = std::move(query);
          pending_queries_.pop_front();
          pending_queries_.emplace_back(std::move(temp));
          if (did_finish && !OnlyHasPendingProgramCompletionQueries()) {
            continue;
          } else {
            program_completion_query_deferred = true;
          }
          result = 0;
        } else {
          GLint link_status = 0;
          api()->glGetProgramivFn(query.program_service_id, GL_LINK_STATUS,
                                  &link_status);
          result = link_status;

          // Send back all program information as early as possible to be cached
          // at the client side.
          GLuint program_client_id = 0u;
          GetClientID(&resources_->program_id_map, query.program_service_id,
                      &program_client_id);

          // TODO(jie.a.chen@intel.com): Merge the all return data into 1 IPC
          // message if it becomes a concern.
          std::vector<uint8_t> program_info;
          error::Error error =
              DoGetProgramInfoCHROMIUM(program_client_id, &program_info);
          if (error == error::kNoError) {
            ReturnProgramInfoData(client(), program_info,
                                  GLES2ReturnDataType::kES2ProgramInfo,
                                  program_client_id);
          }

          if (feature_info_->IsWebGL2OrES3OrHigherContext()) {
            std::vector<uint8_t> uniform_blocks;
            error =
                DoGetUniformBlocksCHROMIUM(program_client_id, &uniform_blocks);
            if (error == error::kNoError) {
              ReturnProgramInfoData(client(), uniform_blocks,
                                    GLES2ReturnDataType::kES3UniformBlocks,
                                    program_client_id);
            }

            std::vector<uint8_t> transform_feedback_varyings;
            error = DoGetTransformFeedbackVaryingsCHROMIUM(
                program_client_id, &transform_feedback_varyings);
            if (error == error::kNoError) {
              ReturnProgramInfoData(
                  client(), transform_feedback_varyings,
                  GLES2ReturnDataType::kES3TransformFeedbackVaryings,
                  program_client_id);
            }

            std::vector<uint8_t> uniforms;
            error = DoGetUniformsES3CHROMIUM(program_client_id, &uniforms);
            if (error == error::kNoError) {
              ReturnProgramInfoData(client(), uniforms,
                                    GLES2ReturnDataType::kES3Uniforms,
                                    program_client_id);
            }
          }
        }
        break;

      default:
        DCHECK(!IsEmulatedQueryTarget(query.target));
        if (did_finish) {
          result_available = GL_TRUE;
        } else {
          api()->glGetQueryObjectuivFn(
              query.service_id, GL_QUERY_RESULT_AVAILABLE, &result_available);
        }
        if (result_available == GL_TRUE) {
          if (feature_info_->feature_flags().ext_disjoint_timer_query) {
            api()->glGetQueryObjectui64vFn(query.service_id, GL_QUERY_RESULT,
                                           &result);
          } else {
            GLuint temp_result = 0;
            api()->glGetQueryObjectuivFn(query.service_id, GL_QUERY_RESULT,
                                         &temp_result);
            result = temp_result;
          }
        }
        break;
    }

    if (!result_available) {
      break;
    }

    // Mark the query as complete
    query.sync->result = result;
    base::subtle::Release_Store(&query.sync->process_count, query.submit_count);
    pending_queries_.pop_front();
  }

  // If api()->glFinishFn() has been called, all of our queries should be
  // completed.
  DCHECK(!did_finish || pending_queries_.empty() ||
         program_completion_query_deferred);
  return error::kNoError;
}

void GLES2DecoderPassthroughImpl::RemovePendingQuery(GLuint service_id) {
  auto pending_iter = base::ranges::find(pending_queries_, service_id,
                                         &PendingQuery::service_id);
  if (pending_iter != pending_queries_.end()) {
    QuerySync* sync = pending_iter->sync;
    sync->result = 0;
    base::subtle::Release_Store(&sync->process_count,
                                pending_iter->submit_count);

    pending_queries_.erase(pending_iter);
  }
}

void GLES2DecoderPassthroughImpl::ReadBackBuffersIntoShadowCopies(
    const BufferShadowUpdateMap& updates) {
  if (updates.empty()) {
    return;
  }

  GLint old_binding = 0;
  api()->glGetIntegervFn(GL_ARRAY_BUFFER_BINDING, &old_binding);
  for (const auto& u : updates) {
    GLuint client_id = u.first;
    GLuint service_id = 0;
    if (!resources_->buffer_id_map.GetServiceID(client_id, &service_id)) {
      // Buffer no longer exists, this shadow update should have been removed by
      // DoDeleteBuffers
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    const auto& update = u.second;

    void* shadow = update.shm->GetDataAddress(update.shm_offset, update.size);
    DCHECK(shadow);

    api()->glBindBufferFn(GL_ARRAY_BUFFER, service_id);
    GLint already_mapped = GL_TRUE;
    api()->glGetBufferParameterivFn(GL_ARRAY_BUFFER, GL_BUFFER_MAPPED,
                                    &already_mapped);
    if (already_mapped) {
      // The buffer is already mapped by the client. It's okay that the shadow
      // copy will be out-of-date, because the client will never read it:
      // * Client issues READBACK_SHADOW_COPIES_UPDATED_CHROMIUM query
      // * Client maps buffer
      // * Client receives signal that the query completed
      // * Client unmaps buffer - invalidating the shadow copy
      // * Client maps buffer to read back - hits the round-trip path
      continue;
    }

    void* mapped = api()->glMapBufferRangeFn(GL_ARRAY_BUFFER, 0, update.size,
                                             GL_MAP_READ_BIT);
    if (!mapped) {
      DLOG(ERROR) << "glMapBufferRange unexpectedly returned NULL";
      MarkContextLost(error::kOutOfMemory);
      group_->LoseContexts(error::kUnknown);
      return;
    }
    memcpy(shadow, mapped, update.size);
    bool unmap_ok = api()->glUnmapBufferFn(GL_ARRAY_BUFFER);
    if (unmap_ok == GL_FALSE) {
      DLOG(ERROR) << "glUnmapBuffer unexpectedly returned GL_FALSE";
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
      return;
    }
  }

  // Restore original GL_ARRAY_BUFFER binding
  api()->glBindBufferFn(GL_ARRAY_BUFFER, old_binding);
}

error::Error GLES2DecoderPassthroughImpl::ProcessReadPixels(bool did_finish) {
  while (!pending_read_pixels_.empty()) {
    const PendingReadPixels& pending_read_pixels = pending_read_pixels_.front();
    if (did_finish || pending_read_pixels.fence->HasCompleted()) {
      using Result = cmds::ReadPixels::Result;
      Result* result = nullptr;
      if (pending_read_pixels.result_shm_id != 0) {
        result = GetSharedMemoryAs<Result*>(
            pending_read_pixels.result_shm_id,
            pending_read_pixels.result_shm_offset, sizeof(*result));
        if (!result) {
          api()->glDeleteBuffersARBFn(1,
                                      &pending_read_pixels.buffer_service_id);
          pending_read_pixels_.pop_front();
          break;
        }
      }

      void* pixels =
          GetSharedMemoryAs<void*>(pending_read_pixels.pixels_shm_id,
                                   pending_read_pixels.pixels_shm_offset,
                                   pending_read_pixels.pixels_size);
      if (!pixels) {
        api()->glDeleteBuffersARBFn(1, &pending_read_pixels.buffer_service_id);
        pending_read_pixels_.pop_front();
        break;
      }

      api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB,
                            pending_read_pixels.buffer_service_id);
      void* data = nullptr;
      if (feature_info_->feature_flags().map_buffer_range) {
        data = api()->glMapBufferRangeFn(GL_PIXEL_PACK_BUFFER_ARB, 0,
                                         pending_read_pixels.pixels_size,
                                         GL_MAP_READ_BIT);
      } else {
        data = api()->glMapBufferFn(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY);
      }
      if (!data) {
        InsertError(GL_OUT_OF_MEMORY, "Failed to map pixel pack buffer.");
        pending_read_pixels_.pop_front();
        break;
      }

      memcpy(pixels, data, pending_read_pixels.pixels_size);
      api()->glUnmapBufferFn(GL_PIXEL_PACK_BUFFER_ARB);
      api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB,
                            resources_->buffer_id_map.GetServiceIDOrInvalid(
                                bound_buffers_[GL_PIXEL_PACK_BUFFER_ARB]));
      api()->glDeleteBuffersARBFn(1, &pending_read_pixels.buffer_service_id);

      if (result != nullptr) {
        result->success = 1;
      }

      pending_read_pixels_.pop_front();
    }
  }

  // If api()->glFinishFn() has been called, all of our fences should be
  // completed.
  DCHECK(!did_finish || pending_read_pixels_.empty());
  return error::kNoError;
}

void GLES2DecoderPassthroughImpl::ProcessDescheduleUntilFinished() {
  if (deschedule_until_finished_fences_.size() < 2) {
    return;
  }
  DCHECK_EQ(2u, deschedule_until_finished_fences_.size());

  if (!deschedule_until_finished_fences_[0]->HasCompleted()) {
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "cc", "GLES2DecoderPassthroughImpl::DescheduleUntilFinished",
      TRACE_ID_LOCAL(this));
  deschedule_until_finished_fences_.pop_front();
  client()->OnRescheduleAfterFinished();
}

void GLES2DecoderPassthroughImpl::UpdateTextureBinding(
    GLenum target,
    GLuint client_id,
    TexturePassthrough* texture) {
  GLuint texture_service_id = texture ? texture->service_id() : 0;
  size_t cur_texture_unit = active_texture_unit_;
  auto& target_bound_textures =
      bound_textures_[static_cast<size_t>(GLenumToTextureTarget(target))];
  for (size_t bound_texture_index = 0;
       bound_texture_index < target_bound_textures.size();
       bound_texture_index++) {
    if (target_bound_textures[bound_texture_index].client_id == client_id) {
      // Update the active texture unit if needed
      if (bound_texture_index != cur_texture_unit) {
        api()->glActiveTextureFn(
            static_cast<GLenum>(GL_TEXTURE0 + bound_texture_index));
        cur_texture_unit = bound_texture_index;
      }

      // Update the texture binding
      api()->glBindTextureFn(target, texture_service_id);
      target_bound_textures[bound_texture_index].texture = texture;
    }
  }

  // Reset the active texture unit if it was changed
  if (cur_texture_unit != active_texture_unit_) {
    api()->glActiveTextureFn(
        static_cast<GLenum>(GL_TEXTURE0 + active_texture_unit_));
  }
}

void GLES2DecoderPassthroughImpl::RebindTexture(TexturePassthrough* texture) {
  DCHECK(texture != nullptr);
  size_t cur_texture_unit = active_texture_unit_;
  GLenum target = texture->target();
  auto& target_bound_textures =
      bound_textures_[static_cast<size_t>(GLenumToTextureTarget(target))];
  for (size_t bound_texture_index = 0;
       bound_texture_index < target_bound_textures.size();
       bound_texture_index++) {
    if (target_bound_textures[bound_texture_index].texture == texture) {
      // Update the active texture unit if needed
      if (bound_texture_index != cur_texture_unit) {
        api()->glActiveTextureFn(
            static_cast<GLenum>(GL_TEXTURE0 + bound_texture_index));
        cur_texture_unit = bound_texture_index;
      }

      // Update the texture binding
      api()->glBindTextureFn(target, texture->service_id());
    }
  }

  // Reset the active texture unit if it was changed
  if (cur_texture_unit != active_texture_unit_) {
    api()->glActiveTextureFn(
        static_cast<GLenum>(GL_TEXTURE0 + active_texture_unit_));
  }
}

void GLES2DecoderPassthroughImpl::UpdateTextureSizeFromTexturePassthrough(
    TexturePassthrough* texture,
    GLuint client_id) {
  if (texture == nullptr) {
    return;
  }

  CheckErrorCallbackState();

  GLenum target = texture->target();
  TextureTarget internal_texture_type = GLenumToTextureTarget(target);
  BoundTexture& bound_texture =
      bound_textures_[static_cast<size_t>(internal_texture_type)]
                     [active_texture_unit_];
  bool needs_rebind = bound_texture.texture == texture;
  if (needs_rebind) {
    glBindTexture(target, texture->service_id());
  }

  UpdateBoundTexturePassthroughSize(api(), texture);

  // If a client ID is available, notify the discardable manager of the size
  // change
  if (client_id != 0) {
    group_->passthrough_discardable_manager()->UpdateTextureSize(
        client_id, group_.get(), texture->estimated_size());
  }

  if (needs_rebind) {
    GLuint old_texture =
        bound_texture.texture ? bound_texture.texture->service_id() : 0;
    glBindTexture(target, old_texture);
  }

  DCHECK(!CheckErrorCallbackState());
}

void GLES2DecoderPassthroughImpl::UpdateTextureSizeFromTarget(GLenum target) {
  GLenum texture_type = TextureTargetToTextureType(target);
  TextureTarget internal_texture_type = GLenumToTextureTarget(texture_type);
  DCHECK(internal_texture_type != TextureTarget::kUnkown);
  BoundTexture& bound_texture =
      bound_textures_[static_cast<size_t>(internal_texture_type)]
                     [active_texture_unit_];
  UpdateTextureSizeFromTexturePassthrough(bound_texture.texture.get(),
                                          bound_texture.client_id);
}

void GLES2DecoderPassthroughImpl::UpdateTextureSizeFromClientID(
    GLuint client_id) {
  scoped_refptr<TexturePassthrough> texture;
  if (resources_->texture_object_map.GetServiceID(client_id, &texture) &&
      texture != nullptr) {
    UpdateTextureSizeFromTexturePassthrough(texture.get(), client_id);
  }
}

void GLES2DecoderPassthroughImpl::
    LazilyUpdateCurrentlyBoundElementArrayBuffer() {
  if (!bound_element_array_buffer_dirty_)
    return;

  GLint service_element_array_buffer = 0;
  api_->glGetIntegervFn(GL_ELEMENT_ARRAY_BUFFER_BINDING,
                        &service_element_array_buffer);

  GLuint client_element_array_buffer = 0;
  if (service_element_array_buffer != 0) {
    GetClientID(&resources_->buffer_id_map,
                static_cast<GLuint>(service_element_array_buffer),
                &client_element_array_buffer);
  }

  bound_buffers_[GL_ELEMENT_ARRAY_BUFFER] = client_element_array_buffer;
  bound_element_array_buffer_dirty_ = false;
}

error::Error GLES2DecoderPassthroughImpl::HandleSetActiveURLCHROMIUM(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile cmds::SetActiveURLCHROMIUM& c =
      *static_cast<const volatile cmds::SetActiveURLCHROMIUM*>(cmd_data);
  Bucket* url_bucket = GetBucket(c.url_bucket_id);
  static constexpr size_t kMaxStrLen = 1024;
  if (!url_bucket || url_bucket->size() == 0 ||
      url_bucket->size() > kMaxStrLen + 1) {
    return error::kInvalidArguments;
  }

  size_t size = url_bucket->size() - 1;
  const char* url_str = url_bucket->GetDataAs<const char*>(0, size);
  if (!url_str)
    return error::kInvalidArguments;

  GURL url(std::string_view(url_str, size));
  client()->SetActiveURL(std::move(url));
  return error::kNoError;
}

void GLES2DecoderPassthroughImpl::VerifyServiceTextureObjectsExist() {
  resources_->texture_object_map.ForEach(
      [this](GLuint client_id, scoped_refptr<TexturePassthrough> texture) {
        DCHECK_EQ(GL_TRUE, api()->glIsTextureFn(texture->service_id()));
      });
}

bool GLES2DecoderPassthroughImpl::IsEmulatedFramebufferBound(
    GLenum target) const {
  if (!emulated_back_buffer_ && !external_default_framebuffer_) {
    return false;
  }

  if ((target == GL_FRAMEBUFFER_EXT || target == GL_DRAW_FRAMEBUFFER) &&
      bound_draw_framebuffer_ == 0) {
    return true;
  }

  if (target == GL_READ_FRAMEBUFFER && bound_read_framebuffer_ == 0) {
    return true;
  }

  return false;
}

void GLES2DecoderPassthroughImpl::CheckSwapBuffersAsyncResult(
    const char* function_name,
    uint64_t swap_id,
    gfx::SwapCompletionResult result) {
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "gpu", "AsyncSwapBuffers",
      TRACE_ID_WITH_SCOPE("AsyncSwapBuffers", swap_id));
  CheckSwapBuffersResult(result.swap_result, function_name);
}

error::Error GLES2DecoderPassthroughImpl::CheckSwapBuffersResult(
    gfx::SwapResult result,
    const char* function_name) {
  if (result == gfx::SwapResult::SWAP_FAILED) {
    // If SwapBuffers failed, we may not have a current context any more.
    LOG(ERROR) << "Context lost because " << function_name << " failed.";
    if (!context_->IsCurrent(surface_.get()) || !CheckResetStatus()) {
      MarkContextLost(error::kUnknown);
      group_->LoseContexts(error::kUnknown);
      return error::kLostContext;
    }
  }

  return error::kNoError;
}

// static
GLES2DecoderPassthroughImpl::TextureTarget
GLES2DecoderPassthroughImpl::GLenumToTextureTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return TextureTarget::k2D;
    case GL_TEXTURE_CUBE_MAP:
      return TextureTarget::kCubeMap;
    case GL_TEXTURE_2D_ARRAY:
      return TextureTarget::k2DArray;
    case GL_TEXTURE_3D:
      return TextureTarget::k3D;
    case GL_TEXTURE_2D_MULTISAMPLE:
      return TextureTarget::k2DMultisample;
    case GL_TEXTURE_EXTERNAL_OES:
      return TextureTarget::kExternal;
    case GL_TEXTURE_RECTANGLE_ARB:
      return TextureTarget::kRectangle;
    case GL_TEXTURE_BUFFER:
      return TextureTarget::kBuffer;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
      return TextureTarget::kCubeMapArray;
    case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
      return TextureTarget::k2DMultisampleArray;
    default:
      return TextureTarget::kUnkown;
  }
}

bool GLES2DecoderPassthroughImpl::CheckErrorCallbackState() {
  bool had_error_ = had_error_callback_;
  had_error_callback_ = false;
  if (had_error_) {
    // Make sure lose-context-on-OOM logic is triggered as early as possible.
    FlushErrors();
  }
  return had_error_;
}

#define GLES2_CMD_OP(name)                                 \
  {                                                        \
      &GLES2DecoderPassthroughImpl::Handle##name,          \
      cmds::name::kArgFlags,                               \
      cmds::name::cmd_flags,                               \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }, /* NOLINT */

constexpr GLES2DecoderPassthroughImpl::CommandInfo
    GLES2DecoderPassthroughImpl::command_info[] = {
        GLES2_COMMAND_LIST(GLES2_CMD_OP)};

#undef GLES2_CMD_OP

}  // namespace gles2
}  // namespace gpu
