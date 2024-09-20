// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <memory>

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/discardable_handle.h"
#include "gpu/command_buffer/service/copy_shared_image_helper.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/multi_draw_manager.h"
#include "gpu/command_buffer/service/passthrough_discardable_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace gles2 {

namespace {

template <typename ClientType, typename ServiceType, typename GenFunction>
error::Error GenHelper(GLsizei n,
                       const volatile ClientType* client_ids,
                       ClientServiceMap<ClientType, ServiceType>* id_map,
                       GenFunction gen_function) {
  DCHECK(n >= 0);
  std::vector<ClientType> client_ids_copy(client_ids, client_ids + n);
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (id_map->HasClientID(client_ids_copy[ii])) {
      return error::kInvalidArguments;
    }
  }
  if (!CheckUniqueAndNonNullIds(n, client_ids_copy.data())) {
    return error::kInvalidArguments;
  }

  std::vector<ServiceType> service_ids(n, 0);
  gen_function(n, service_ids.data());
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_map->SetIDMapping(client_ids_copy[ii], service_ids[ii]);
  }

  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename GenFunction>
error::Error CreateHelper(ClientType client_id,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          GenFunction create_function) {
  if (id_map->HasClientID(client_id)) {
    return error::kInvalidArguments;
  }
  ServiceType service_id = create_function();
  id_map->SetIDMapping(client_id, service_id);
  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename DeleteFunction>
error::Error DeleteHelper(GLsizei n,
                          const volatile ClientType* client_ids,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          DeleteFunction delete_function) {
  DCHECK(n >= 0);
  std::vector<ServiceType> service_ids(n, 0);
  for (GLsizei ii = 0; ii < n; ++ii) {
    ClientType client_id = client_ids[ii];

    // Don't pass service IDs of objects with a client ID of 0.  They are
    // emulated and should not be deleteable
    if (client_id != 0) {
      service_ids[ii] = id_map->GetServiceIDOrInvalid(client_id);
      id_map->RemoveClientID(client_id);
    }
  }

  delete_function(n, service_ids.data());

  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename DeleteFunction>
error::Error DeleteHelper(ClientType client_id,
                          ClientServiceMap<ClientType, ServiceType>* id_map,
                          DeleteFunction delete_function) {
  delete_function(id_map->GetServiceIDOrInvalid(client_id));
  id_map->RemoveClientID(client_id);
  return error::kNoError;
}

template <typename ClientType, typename ServiceType, typename GenFunction>
ServiceType GetServiceID(ClientType client_id,
                         ClientServiceMap<ClientType, ServiceType>* id_map,
                         bool create_if_missing,
                         GenFunction gen_function) {
  ServiceType service_id = id_map->invalid_service_id();
  if (id_map->GetServiceID(client_id, &service_id)) {
    return service_id;
  }

  if (create_if_missing) {
    service_id = gen_function();
    id_map->SetIDMapping(client_id, service_id);
    return service_id;
  }

  return id_map->invalid_service_id();
}

GLuint GetTextureServiceID(gl::GLApi* api,
                           GLuint client_id,
                           PassthroughResources* resources,
                           bool create_if_missing) {
  GLuint service_id = resources->texture_id_map.invalid_service_id();
  if (resources->texture_id_map.GetServiceID(client_id, &service_id)) {
    return service_id;
  }

  if (create_if_missing) {
    service_id = 0;
    api->glGenTexturesFn(1, &service_id);
    resources->texture_id_map.SetIDMapping(client_id, service_id);
    return service_id;
  }

  return resources->texture_id_map.invalid_service_id();
}

GLuint GetBufferServiceID(gl::GLApi* api,
                          GLuint client_id,
                          PassthroughResources* resources,
                          bool create_if_missing) {
  return GetServiceID(client_id, &resources->buffer_id_map, create_if_missing,
                      [api]() {
                        GLuint service_id = 0;
                        api->glGenBuffersARBFn(1, &service_id);
                        return service_id;
                      });
}

GLuint GetRenderbufferServiceID(gl::GLApi* api,
                                GLuint client_id,
                                PassthroughResources* resources,
                                bool create_if_missing) {
  return GetServiceID(client_id, &resources->renderbuffer_id_map,
                      create_if_missing, [api]() {
                        GLuint service_id = 0;
                        api->glGenRenderbuffersEXTFn(1, &service_id);
                        return service_id;
                      });
}

GLuint GetFramebufferServiceID(gl::GLApi* api,
                               GLuint client_id,
                               ClientServiceMap<GLuint, GLuint>* id_map,
                               bool create_if_missing) {
  return GetServiceID(client_id, id_map, create_if_missing, [api]() {
    GLuint service_id = 0;
    api->glGenFramebuffersEXTFn(1, &service_id);
    return service_id;
  });
}

GLuint GetTransformFeedbackServiceID(GLuint client_id,
                                     ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetVertexArrayServiceID(GLuint client_id,
                               ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetProgramServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->program_id_map.GetServiceIDOrInvalid(client_id);
}

GLuint GetShaderServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->shader_id_map.GetServiceIDOrInvalid(client_id);
}

GLuint GetQueryServiceID(GLuint client_id,
                         ClientServiceMap<GLuint, GLuint>* id_map) {
  return id_map->GetServiceIDOrInvalid(client_id);
}

GLuint GetSamplerServiceID(GLuint client_id, PassthroughResources* resources) {
  return resources->sampler_id_map.GetServiceIDOrInvalid(client_id);
}

GLsync GetSyncServiceID(GLuint client_id, PassthroughResources* resources) {
  return reinterpret_cast<GLsync>(
      resources->sync_id_map.GetServiceIDOrInvalid(client_id));
}

template <typename T>
void InsertValueIntoBuffer(std::vector<uint8_t>* data,
                           const T& value,
                           size_t offset) {
  DCHECK_LE(offset + sizeof(T), data->size());
  memcpy(data->data() + offset, &value, sizeof(T));
}

template <typename T>
void AppendValueToBuffer(std::vector<uint8_t>* data, const T& value) {
  const base::CheckedNumeric<size_t> old_size = data->size();
  data->resize((old_size + sizeof(T)).ValueOrDie());
  memcpy(data->data() + old_size.ValueOrDie(), &value, sizeof(T));
}

void AppendStringToBuffer(std::vector<uint8_t>* data,
                          const char* str,
                          size_t len) {
  const base::CheckedNumeric<size_t> old_size = data->size();
  data->resize((old_size + len).ValueOrDie());
  memcpy(data->data() + old_size.ValueOrDie(), str, len);
}

// In order to minimize the amount of data copied, the command buffer client
// unpack pixels before sending the glTex[Sub]Image[2|3]D calls. The only
// parameter it doesn't handle is the alignment. Resetting the unpack state is
// not needed when uploading from a PBO and for compressed formats which the
// client sends untouched. This class handles resetting and restoring the unpack
// state.
// TODO(cwallez@chromium.org) it would be nicer to handle the resetting /
// restoring on the client side.
class ScopedUnpackStateButAlignmentReset {
 public:
  ScopedUnpackStateButAlignmentReset(gl::GLApi* api, bool enable, bool is_3d)
      : api_(api) {
    if (!enable) {
      return;
    }

    api_->glGetIntegervFn(GL_UNPACK_SKIP_PIXELS, &skip_pixels_);
    api_->glPixelStoreiFn(GL_UNPACK_SKIP_PIXELS, 0);
    api_->glGetIntegervFn(GL_UNPACK_SKIP_ROWS, &skip_rows_);
    api_->glPixelStoreiFn(GL_UNPACK_SKIP_ROWS, 0);
    api_->glGetIntegervFn(GL_UNPACK_ROW_LENGTH, &row_length_);
    api_->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, 0);

    if (is_3d) {
      api_->glGetIntegervFn(GL_UNPACK_SKIP_IMAGES, &skip_images_);
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_IMAGES, 0);
      api_->glGetIntegervFn(GL_UNPACK_IMAGE_HEIGHT, &image_height_);
      api_->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, 0);
    }
  }

  ~ScopedUnpackStateButAlignmentReset() {
    if (skip_pixels_ != 0) {
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_PIXELS, skip_pixels_);
    }
    if (skip_rows_ != 0) {
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_ROWS, skip_rows_);
    }
    if (skip_images_ != 0) {
      api_->glPixelStoreiFn(GL_UNPACK_SKIP_IMAGES, skip_images_);
    }
    if (row_length_ != 0) {
      api_->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, row_length_);
    }
    if (image_height_ != 0) {
      api_->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, image_height_);
    }
  }

 private:
  raw_ptr<gl::GLApi> api_;
  GLint skip_pixels_ = 0;
  GLint skip_rows_ = 0;
  GLint skip_images_ = 0;
  GLint row_length_ = 0;
  GLint image_height_ = 0;
};

class ScopedPackStateRowLengthReset {
 public:
  ScopedPackStateRowLengthReset(gl::GLApi* api, bool enable) : api_(api) {
    if (!enable) {
      return;
    }

    api_->glGetIntegervFn(GL_PACK_ROW_LENGTH, &row_length_);
    api_->glPixelStoreiFn(GL_PACK_ROW_LENGTH, 0);
  }

  ~ScopedPackStateRowLengthReset() {
    if (row_length_ != 0) {
      api_->glPixelStoreiFn(GL_PACK_ROW_LENGTH, row_length_);
    }
  }

 private:
  raw_ptr<gl::GLApi> api_;
  GLint row_length_ = 0;
};

bool ModifyAttachmentForEmulatedFramebuffer(GLenum* attachment) {
  switch (*attachment) {
    case GL_BACK:
      *attachment = GL_COLOR_ATTACHMENT0;
      return true;

    case GL_DEPTH:
      *attachment = GL_DEPTH_ATTACHMENT;
      return true;

    case GL_STENCIL:
      *attachment = GL_STENCIL_ATTACHMENT;
      return true;

    default:
      return false;
  }
}

bool ModifyAttachmentsForEmulatedFramebuffer(std::vector<GLenum>* attachments) {
  for (GLenum& attachment : *attachments) {
    if (!ModifyAttachmentForEmulatedFramebuffer(&attachment)) {
      return false;
    }
  }

  return true;
}

SkYUVAPixmapInfo::DataType ToSkYUVADataType(viz::SharedImageFormat format) {
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return SkYUVAPixmapInfo::DataType::kUnorm8;
    case viz::SharedImageFormat::ChannelFormat::k10:
      return SkYUVAPixmapInfo::DataType::kUnorm10_Unorm2;
    case viz::SharedImageFormat::ChannelFormat::k16:
      return SkYUVAPixmapInfo::DataType::kUnorm16;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return SkYUVAPixmapInfo::DataType::kFloat16;
  }
  NOTREACHED();
}

bool IsGLFormatAndTypeSupported(GLenum format, GLenum type) {
  switch (format) {
    case GL_RGBA:
      return type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_4_4_4_4;
    case GL_RGB:
      return type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5;
    case GL_RGBA8:
    case GL_RGB565:
    case GL_RGBA16F:
    case GL_RGB8:
    case GL_RGB10_A2:
    case GL_RGBA4:
    case GL_SRGB8_ALPHA8:
      return true;
    default:
      return false;
  }
}

}  // anonymous namespace

// Implementations of commands
error::Error GLES2DecoderPassthroughImpl::DoActiveTexture(GLenum texture) {
  CheckErrorCallbackState();
  api()->glActiveTextureFn(texture);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  active_texture_unit_ = static_cast<size_t>(texture) - GL_TEXTURE0;
  DCHECK(active_texture_unit_ < kMaxTextureUnits);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoAttachShader(GLuint program,
                                                         GLuint shader) {
  api()->glAttachShaderFn(GetProgramServiceID(program, resources_),
                          GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindAttribLocation(
    GLuint program,
    GLuint index,
    const char* name) {
  api()->glBindAttribLocationFn(GetProgramServiceID(program, resources_), index,
                                name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBuffer(GLenum target,
                                                       GLuint buffer) {
  CheckErrorCallbackState();
  api()->glBindBufferFn(target, GetBufferServiceID(api(), buffer, resources_,
                                                   bind_generates_resource_));
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  DCHECK(base::Contains(bound_buffers_, target));
  bound_buffers_[target] = buffer;
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    bound_element_array_buffer_dirty_ = false;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferBase(GLenum target,
                                                           GLuint index,
                                                           GLuint buffer) {
  CheckErrorCallbackState();
  api()->glBindBufferBaseFn(
      target, index,
      GetBufferServiceID(api(), buffer, resources_, bind_generates_resource_));
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  DCHECK(base::Contains(bound_buffers_, target));
  bound_buffers_[target] = buffer;
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    bound_element_array_buffer_dirty_ = false;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindBufferRange(GLenum target,
                                                            GLuint index,
                                                            GLuint buffer,
                                                            GLintptr offset,
                                                            GLsizeiptr size) {
  CheckErrorCallbackState();
  api()->glBindBufferRangeFn(
      target, index,
      GetBufferServiceID(api(), buffer, resources_, bind_generates_resource_),
      offset, size);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  DCHECK(base::Contains(bound_buffers_, target));
  bound_buffers_[target] = buffer;
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    bound_element_array_buffer_dirty_ = false;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFramebuffer(
    GLenum target,
    GLuint framebuffer) {
  CheckErrorCallbackState();
  api()->glBindFramebufferEXTFn(
      target, GetFramebufferServiceID(api(), framebuffer, &framebuffer_id_map_,
                                      bind_generates_resource_));
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  // Update tracking of the bound framebuffer
  switch (target) {
    case GL_FRAMEBUFFER_EXT:
      bound_draw_framebuffer_ = framebuffer;
      bound_read_framebuffer_ = framebuffer;
      break;

    case GL_DRAW_FRAMEBUFFER:
      bound_draw_framebuffer_ = framebuffer;
      break;

    case GL_READ_FRAMEBUFFER:
      bound_read_framebuffer_ = framebuffer;
      break;

    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindImageTexture(GLuint unit,
                                                             GLuint texture,
                                                             GLint level,
                                                             GLboolean layered,
                                                             GLint layer,
                                                             GLenum access,
                                                             GLenum format) {
  api()->glBindImageTextureEXTFn(
      unit,
      GetTextureServiceID(api(), texture, resources_, bind_generates_resource_),
      level, layered, layer, access, format);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindRenderbuffer(
    GLenum target,
    GLuint renderbuffer) {
  api()->glBindRenderbufferEXTFn(
      target, GetRenderbufferServiceID(api(), renderbuffer, resources_,
                                       bind_generates_resource_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindSampler(GLuint unit,
                                                        GLuint sampler) {
  api()->glBindSamplerFn(unit, GetSamplerServiceID(sampler, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTexture(GLenum target,
                                                        GLuint texture) {
  GLuint service_id =
      GetTextureServiceID(api(), texture, resources_, bind_generates_resource_);

  CheckErrorCallbackState();

  api()->glBindTextureFn(target, service_id);

  // Only update tracking if no error was generated in the bind call
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  // Track the currently bound textures
  TextureTarget texture_target = GLenumToTextureTarget(target);
  CHECK_NE(texture_target, TextureTarget::kUnkown);
  scoped_refptr<TexturePassthrough> texture_passthrough;

  if (service_id != 0) {
    // Label the texture with additional context info
    const char* label = ContextTypeToLabel(feature_info_->context_type());
    api()->glObjectLabelFn(GL_TEXTURE, service_id, strlen(label), label);

    // Create a new texture object to track this texture
    if (!resources_->texture_object_map.GetServiceID(texture,
                                                     &texture_passthrough) ||
        texture_passthrough == nullptr) {
      texture_passthrough = new TexturePassthrough(service_id, target);
      resources_->texture_object_map.SetIDMapping(texture, texture_passthrough);
    } else {
      // Shouldn't be possible to get here if this texture has a different
      // target than the one it was just bound to
      DCHECK(texture_passthrough->target() == target);
    }
  }

  BoundTexture* bound_texture =
      &bound_textures_[static_cast<size_t>(texture_target)]
                      [active_texture_unit_];
  bound_texture->client_id = texture;
  bound_texture->texture = std::move(texture_passthrough);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindTransformFeedback(
    GLenum target,
    GLuint transformfeedback) {
  api()->glBindTransformFeedbackFn(
      target, GetTransformFeedbackServiceID(transformfeedback,
                                            &transform_feedback_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  api()->glBlendColorFn(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquation(GLenum mode) {
  api()->glBlendEquationFn(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquationiOES(GLuint buf,
                                                              GLenum mode) {
  api()->glBlendEquationiOESFn(buf, mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquationSeparate(
    GLenum modeRGB,
    GLenum modeAlpha) {
  api()->glBlendEquationSeparateFn(modeRGB, modeAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendEquationSeparateiOES(
    GLuint buf,
    GLenum modeRGB,
    GLenum modeAlpha) {
  api()->glBlendEquationSeparateiOESFn(buf, modeRGB, modeAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFunc(GLenum sfactor,
                                                      GLenum dfactor) {
  api()->glBlendFuncFn(sfactor, dfactor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFunciOES(GLuint buf,
                                                          GLenum sfactor,
                                                          GLenum dfactor) {
  api()->glBlendFunciOESFn(buf, sfactor, dfactor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFuncSeparate(GLenum srcRGB,
                                                              GLenum dstRGB,
                                                              GLenum srcAlpha,
                                                              GLenum dstAlpha) {
  api()->glBlendFuncSeparateFn(srcRGB, dstRGB, srcAlpha, dstAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendFuncSeparateiOES(
    GLuint buf,
    GLenum srcRGB,
    GLenum dstRGB,
    GLenum srcAlpha,
    GLenum dstAlpha) {
  api()->glBlendFuncSeparateiOESFn(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferData(GLenum target,
                                                       GLsizeiptr size,
                                                       const void* data,
                                                       GLenum usage) {
  CheckErrorCallbackState();
  api()->glBufferDataFn(target, size, data, usage);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    LazilyUpdateCurrentlyBoundElementArrayBuffer();
  }

  // Calling buffer data on a mapped buffer will implicitly unmap it
  resources_->mapped_buffer_map.erase(bound_buffers_[target]);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBufferSubData(GLenum target,
                                                          GLintptr offset,
                                                          GLsizeiptr size,
                                                          const void* data) {
  api()->glBufferSubDataFn(target, offset, size, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCheckFramebufferStatus(
    GLenum target,
    uint32_t* result) {
  *result = api()->glCheckFramebufferStatusEXTFn(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClear(GLbitfield mask) {
  api()->glClearFn(mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfi(GLenum buffer,
                                                          GLint drawbuffers,
                                                          GLfloat depth,
                                                          GLint stencil) {
  api()->glClearBufferfiFn(buffer, drawbuffers, depth, stencil);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferfv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLfloat* value) {
  api()->glClearBufferfvFn(buffer, drawbuffers,
                           const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferiv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLint* value) {
  api()->glClearBufferivFn(buffer, drawbuffers,
                           const_cast<const GLint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearBufferuiv(
    GLenum buffer,
    GLint drawbuffers,
    const volatile GLuint* value) {
  api()->glClearBufferuivFn(buffer, drawbuffers,
                            const_cast<const GLuint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearColor(GLclampf red,
                                                       GLclampf green,
                                                       GLclampf blue,
                                                       GLclampf alpha) {
  api()->glClearColorFn(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearDepthf(GLclampf depth) {
  api()->glClearDepthfFn(depth);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClearStencil(GLint s) {
  api()->glClearStencilFn(s);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClientWaitSync(GLuint sync,
                                                           GLbitfield flags,
                                                           GLuint64 timeout,
                                                           GLenum* result) {
  // Force GL_SYNC_FLUSH_COMMANDS_BIT to avoid infinite wait.
  GLbitfield modified_flags = flags | GL_SYNC_FLUSH_COMMANDS_BIT;
  *result = api()->glClientWaitSyncFn(GetSyncServiceID(sync, resources_),
                                      modified_flags, timeout);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoColorMask(GLboolean red,
                                                      GLboolean green,
                                                      GLboolean blue,
                                                      GLboolean alpha) {
  api()->glColorMaskFn(red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoColorMaskiOES(GLuint buf,
                                                          GLboolean red,
                                                          GLboolean green,
                                                          GLboolean blue,
                                                          GLboolean alpha) {
  api()->glColorMaskiOESFn(buf, red, green, blue, alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompileShader(GLuint shader) {
  api()->glCompileShaderFn(GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  CheckErrorCallbackState();
  api()->glCompressedTexImage2DRobustANGLEFn(target, level, internalformat,
                                             width, height, border, image_size,
                                             data_size, data);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  UpdateTextureSizeFromTarget(target);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  api()->glCompressedTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset,
                                                width, height, format,
                                                image_size, data_size, data);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexImage3D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  CheckErrorCallbackState();
  api()->glCompressedTexImage3DRobustANGLEFn(target, level, internalformat,
                                             width, height, depth, border,
                                             image_size, data_size, data);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  UpdateTextureSizeFromTarget(target);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCompressedTexSubImage3D(
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLint zoffset,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLenum format,
    GLsizei image_size,
    GLsizei data_size,
    const void* data) {
  api()->glCompressedTexSubImage3DRobustANGLEFn(
      target, level, xoffset, yoffset, zoffset, width, height, depth, format,
      image_size, data_size, data);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoContextVisibilityHintCHROMIUM(
    GLboolean visibility) {
  if (feature_info_->IsWebGLContext())
    context_->SetVisibility(visibility == GL_TRUE);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyBufferSubData(
    GLenum readtarget,
    GLenum writetarget,
    GLintptr readoffset,
    GLintptr writeoffset,
    GLsizeiptr size) {
  api()->glCopyBufferSubDataFn(readtarget, writetarget, readoffset, writeoffset,
                               size);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexImage2D(
    GLenum target,
    GLint level,
    GLenum internalformat,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLint border) {
  CheckErrorCallbackState();
  api()->glCopyTexImage2DFn(target, level, internalformat, x, y, width, height,
                            border);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  UpdateTextureSizeFromTarget(target);

  // Texture data copying can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexSubImage2D(GLenum target,
                                                              GLint level,
                                                              GLint xoffset,
                                                              GLint yoffset,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  api()->glCopyTexSubImage2DFn(target, level, xoffset, yoffset, x, y, width,
                               height);

  // Texture data copying can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTexSubImage3D(GLenum target,
                                                              GLint level,
                                                              GLint xoffset,
                                                              GLint yoffset,
                                                              GLint zoffset,
                                                              GLint x,
                                                              GLint y,
                                                              GLsizei width,
                                                              GLsizei height) {
  api()->glCopyTexSubImage3DFn(target, level, xoffset, yoffset, zoffset, x, y,
                               width, height);

  // Texture data copying can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateProgram(GLuint client_id) {
  return CreateHelper(client_id, &resources_->program_id_map,
                      [this]() { return api()->glCreateProgramFn(); });
}

error::Error GLES2DecoderPassthroughImpl::DoCreateShader(GLenum type,
                                                         GLuint client_id) {
  return CreateHelper(client_id, &resources_->shader_id_map,
                      [this, type]() { return api()->glCreateShaderFn(type); });
}

error::Error GLES2DecoderPassthroughImpl::DoCullFace(GLenum mode) {
  api()->glCullFaceFn(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteBuffers(
    GLsizei n,
    const volatile GLuint* buffers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }

  LazilyUpdateCurrentlyBoundElementArrayBuffer();

  std::vector<GLuint> service_ids(n, 0);
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = buffers[ii];

    // Update the bound and mapped buffer state tracking
    for (auto& buffer_binding : bound_buffers_) {
      if (buffer_binding.second == client_id) {
        buffer_binding.second = 0;
      }
      resources_->mapped_buffer_map.erase(client_id);
    }

    service_ids[ii] =
        resources_->buffer_id_map.GetServiceIDOrInvalid(client_id);
    resources_->buffer_id_map.RemoveClientID(client_id);

    auto is_the_deleted_buffer = [client_id](const auto& update) {
      return update.first == client_id;
    };
    base::EraseIf(buffer_shadow_updates_, is_the_deleted_buffer);
    for (PendingQuery& pending_query : pending_queries_) {
      base::EraseIf(pending_query.buffer_shadow_updates, is_the_deleted_buffer);
    }
  }
  api()->glDeleteBuffersARBFn(n, service_ids.data());

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteFramebuffers(
    GLsizei n,
    const volatile GLuint* framebuffers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLuint> framebuffers_copy(framebuffers, framebuffers + n);

  // If a bound framebuffer is deleted, it's binding is reset to 0.  In the case
  // of an emulated default framebuffer, bind the emulated one.
  for (GLuint framebuffer : framebuffers_copy) {
    if (framebuffer == bound_draw_framebuffer_) {
      bound_draw_framebuffer_ = 0;
      if (emulated_back_buffer_) {
        api()->glBindFramebufferEXTFn(
            GL_DRAW_FRAMEBUFFER, emulated_back_buffer_->framebuffer_service_id);
      }
    }
    if (framebuffer == bound_read_framebuffer_) {
      bound_read_framebuffer_ = 0;
      if (emulated_back_buffer_) {
        api()->glBindFramebufferEXTFn(
            GL_READ_FRAMEBUFFER, emulated_back_buffer_->framebuffer_service_id);
      }
    }
  }

  return DeleteHelper(n, framebuffers_copy.data(), &framebuffer_id_map_,
                      [this](GLsizei n, GLuint* framebuffers) {
                        api()->glDeleteFramebuffersEXTFn(n, framebuffers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteProgram(GLuint program) {
  return DeleteHelper(
      program, &resources_->program_id_map,
      [this](GLuint program) { api()->glDeleteProgramFn(program); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteRenderbuffers(
    GLsizei n,
    const volatile GLuint* renderbuffers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }
  return DeleteHelper(n, renderbuffers, &resources_->renderbuffer_id_map,
                      [this](GLsizei n, GLuint* renderbuffers) {
                        api()->glDeleteRenderbuffersEXTFn(n, renderbuffers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSamplers(
    GLsizei n,
    const volatile GLuint* samplers) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }
  return DeleteHelper(n, samplers, &resources_->sampler_id_map,
                      [this](GLsizei n, GLuint* samplers) {
                        api()->glDeleteSamplersFn(n, samplers);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteSync(GLuint sync) {
  return DeleteHelper(sync, &resources_->sync_id_map, [this](uintptr_t sync) {
    api()->glDeleteSyncFn(reinterpret_cast<GLsync>(sync));
  });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteShader(GLuint shader) {
  return DeleteHelper(
      shader, &resources_->shader_id_map,
      [this](GLuint shader) { api()->glDeleteShaderFn(shader); });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTextures(
    GLsizei n,
    const volatile GLuint* textures) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }

  // Textures that are currently associated with a mailbox are stored in the
  // texture_object_map_ and are deleted automatically when they are
  // unreferenced.  Only delete textures that are not in this map.
  std::vector<GLuint> non_mailbox_client_ids;
  for (GLsizei ii = 0; ii < n; ++ii) {
    GLuint client_id = textures[ii];
    scoped_refptr<TexturePassthrough> texture;
    if (!resources_->texture_object_map.GetServiceID(client_id, &texture) ||
        texture == nullptr) {
      // Delete with DeleteHelper
      non_mailbox_client_ids.push_back(client_id);
    } else {
      // Deleted when unreferenced
      resources_->texture_id_map.RemoveClientID(client_id);
      resources_->texture_object_map.RemoveClientID(client_id);
      resources_->texture_shared_image_map.erase(client_id);
      UpdateTextureBinding(texture->target(), client_id, nullptr);
    }

    // Notify the discardable manager that the texture is deleted
    group_->passthrough_discardable_manager()->DeleteTexture(client_id,
                                                             group_.get());
  }
  return DeleteHelper(
      non_mailbox_client_ids.size(), non_mailbox_client_ids.data(),
      &resources_->texture_id_map, [this](GLsizei n, GLuint* textures) {
        api()->glDeleteTexturesFn(n, textures);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteTransformFeedbacks(
    GLsizei n,
    const volatile GLuint* ids) {
  // DeleteHelper requires that n is non-negative because it allocates a copy of
  // the IDs
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "n cannot be negative.");
    return error::kNoError;
  }
  return DeleteHelper(n, ids, &transform_feedback_id_map_,
                      [this](GLsizei n, GLuint* transform_feedbacks) {
                        api()->glDeleteTransformFeedbacksFn(
                            n, transform_feedbacks);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoDepthFunc(GLenum func) {
  api()->glDepthFuncFn(func);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthMask(GLboolean flag) {
  api()->glDepthMaskFn(flag);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDepthRangef(GLclampf zNear,
                                                        GLclampf zFar) {
  api()->glDepthRangefFn(zNear, zFar);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDetachShader(GLuint program,
                                                         GLuint shader) {
  api()->glDetachShaderFn(GetProgramServiceID(program, resources_),
                          GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisable(GLenum cap) {
  if (IsIgnoredCap(cap)) {
    InsertError(GL_INVALID_ENUM, "Invalid cap.");
    return error::kNoError;
  }

  api()->glDisableFn(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisableVertexAttribArray(
    GLuint index) {
  api()->glDisableVertexAttribArrayFn(index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDispatchCompute(
    GLuint num_groups_x,
    GLuint num_groups_y,
    GLuint num_groups_z) {
  api()->glDispatchComputeFn(num_groups_x, num_groups_y, num_groups_z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDispatchComputeIndirect(
    GLintptr offset) {
  // TODO(jiajie.hu@intel.com): Use glDispatchComputeIndirectRobustANGLEFn()
  // when it's ready in ANGLE.
  api()->glDispatchComputeIndirectFn(offset);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArrays(GLenum mode,
                                                       GLint first,
                                                       GLsizei count) {
  api()->glDrawArraysFn(mode, first, count);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArraysIndirect(
    GLenum mode,
    const void* offset) {
  // TODO(jiajie.hu@intel.com): Use glDrawArraysIndirectRobustANGLEFn() when
  // it's ready in ANGLE.
  api()->glDrawArraysIndirectFn(mode, offset);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElements(GLenum mode,
                                                         GLsizei count,
                                                         GLenum type,
                                                         const void* indices) {
  api()->glDrawElementsFn(mode, count, type, indices);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElementsIndirect(
    GLenum mode,
    GLenum type,
    const void* offset) {
  // TODO(jiajie.hu@intel.com): Use glDrawElementsIndirectRobustANGLEFn() when
  // it's ready in ANGLE.
  api()->glDrawElementsIndirectFn(mode, type, offset);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnable(GLenum cap) {
  if (IsIgnoredCap(cap)) {
    InsertError(GL_INVALID_ENUM, "Invalid cap.");
    return error::kNoError;
  }

  api()->glEnableFn(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableVertexAttribArray(
    GLuint index) {
  api()->glEnableVertexAttribArrayFn(index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFenceSync(GLenum condition,
                                                      GLbitfield flags,
                                                      GLuint client_id) {
  if (resources_->sync_id_map.HasClientID(client_id)) {
    return error::kInvalidArguments;
  }

  CheckErrorCallbackState();
  GLsync service_id = api()->glFenceSyncFn(condition, flags);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  resources_->sync_id_map.SetIDMapping(client_id,
                                       reinterpret_cast<uintptr_t>(service_id));

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFinish() {
  // Finish can take a long time, make sure the watchdog gives it the most
  // amount of time to complete.
  group_->ReportProgress();

  api()->glFinishFn();

  group_->ReportProgress();

  error::Error error = ProcessReadPixels(true);
  if (error != error::kNoError) {
    return error;
  }
  return ProcessQueries(true);
}

error::Error GLES2DecoderPassthroughImpl::DoFlush() {
  api()->glFlushFn();

  error::Error error = ProcessReadPixels(false);
  if (error != error::kNoError) {
    return error;
  }
  return ProcessQueries(false);
}

error::Error GLES2DecoderPassthroughImpl::DoFlushMappedBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size) {
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    LazilyUpdateCurrentlyBoundElementArrayBuffer();
  }

  auto bound_buffers_iter = bound_buffers_.find(target);
  if (bound_buffers_iter == bound_buffers_.end() ||
      bound_buffers_iter->second == 0) {
    InsertError(GL_INVALID_OPERATION, "No buffer bound to this target.");
    return error::kNoError;
  }

  GLuint client_buffer = bound_buffers_iter->second;
  auto mapped_buffer_info_iter =
      resources_->mapped_buffer_map.find(client_buffer);
  if (mapped_buffer_info_iter == resources_->mapped_buffer_map.end()) {
    InsertError(GL_INVALID_OPERATION, "Buffer is not mapped.");
    return error::kNoError;
  }

  const MappedBuffer& map_info = mapped_buffer_info_iter->second;

  if (offset < 0) {
    InsertError(GL_INVALID_VALUE, "Offset cannot be negative.");
    return error::kNoError;
  }

  if (size < 0) {
    InsertError(GL_INVALID_VALUE, "Size cannot be negative.");
    return error::kNoError;
  }

  base::CheckedNumeric<size_t> range_start(offset);
  base::CheckedNumeric<size_t> range_end = range_start + size;
  if (!range_end.IsValid() || range_end.ValueOrDefault(0) > map_info.size) {
    InsertError(GL_INVALID_OPERATION,
                "Flush range is not within the original mapping size.");
    return error::kNoError;
  }

  uint8_t* mem = GetSharedMemoryAs<uint8_t*>(
      map_info.data_shm_id, map_info.data_shm_offset, map_info.size);
  if (!mem) {
    return error::kOutOfBounds;
  }

  memcpy(map_info.map_ptr + offset, mem + offset, size);
  api()->glFlushMappedBufferRangeFn(target, offset, size);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferParameteri(GLenum target,
                                                                  GLenum pname,
                                                                  GLint param) {
  api()->glFramebufferParameteriFn(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferRenderbuffer(
    GLenum target,
    GLenum attachment,
    GLenum renderbuffertarget,
    GLuint renderbuffer) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  api()->glFramebufferRenderbufferEXTFn(
      target, attachment, renderbuffertarget,
      GetRenderbufferServiceID(api(), renderbuffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2D(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  api()->glFramebufferTexture2DEXTFn(
      target, attachment, textarget,
      GetTextureServiceID(api(), texture, resources_, false), level);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTextureLayer(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint layer) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  api()->glFramebufferTextureLayerFn(
      target, attachment,
      GetTextureServiceID(api(), texture, resources_, false), level, layer);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTextureMultiviewOVR(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint base_view_index,
    GLsizei num_views) {
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  api()->glFramebufferTextureMultiviewOVRFn(
      target, attachment,
      GetTextureServiceID(api(), texture, resources_, false), level,
      base_view_index, num_views);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFrontFace(GLenum mode) {
  api()->glFrontFaceFn(mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenBuffers(
    GLsizei n,
    volatile GLuint* buffers) {
  return GenHelper(n, buffers, &resources_->buffer_id_map,
                   [this](GLsizei n, GLuint* buffers) {
                     api()->glGenBuffersARBFn(n, buffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenerateMipmap(GLenum target) {
  api()->glGenerateMipmapEXTFn(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenFramebuffers(
    GLsizei n,
    volatile GLuint* framebuffers) {
  return GenHelper(n, framebuffers, &framebuffer_id_map_,
                   [this](GLsizei n, GLuint* framebuffers) {
                     api()->glGenFramebuffersEXTFn(n, framebuffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenRenderbuffers(
    GLsizei n,
    volatile GLuint* renderbuffers) {
  return GenHelper(n, renderbuffers, &resources_->renderbuffer_id_map,
                   [this](GLsizei n, GLuint* renderbuffers) {
                     api()->glGenRenderbuffersEXTFn(n, renderbuffers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenSamplers(
    GLsizei n,
    volatile GLuint* samplers) {
  return GenHelper(n, samplers, &resources_->sampler_id_map,
                   [this](GLsizei n, GLuint* samplers) {
                     api()->glGenSamplersFn(n, samplers);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenTextures(
    GLsizei n,
    volatile GLuint* textures) {
  return GenHelper(n, textures, &resources_->texture_id_map,
                   [this](GLsizei n, GLuint* textures) {
                     api()->glGenTexturesFn(n, textures);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGenTransformFeedbacks(
    GLsizei n,
    volatile GLuint* ids) {
  return GenHelper(n, ids, &transform_feedback_id_map_,
                   [this](GLsizei n, GLuint* transform_feedbacks) {
                     api()->glGenTransformFeedbacksFn(n, transform_feedbacks);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveAttrib(GLuint program,
                                                            GLuint index,
                                                            GLint* size,
                                                            GLenum* type,
                                                            std::string* name,
                                                            int32_t* success) {
  CheckErrorCallbackState();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint active_attribute_max_length = 0;
  api()->glGetProgramivFn(service_id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                          &active_attribute_max_length);
  if (CheckErrorCallbackState()) {
    *success = 0;
    return error::kNoError;
  }

  std::vector<char> name_buffer(active_attribute_max_length, 0);
  GLsizei length = 0;
  api()->glGetActiveAttribFn(service_id, index, name_buffer.size(), &length,
                             size, type, name_buffer.data());
  DCHECK(length <= active_attribute_max_length);
  *name = length > 0 ? std::string(name_buffer.data(), length) : std::string();
  *success = CheckErrorCallbackState() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniform(GLuint program,
                                                             GLuint index,
                                                             GLint* size,
                                                             GLenum* type,
                                                             std::string* name,
                                                             int32_t* success) {
  CheckErrorCallbackState();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint active_uniform_max_length = 0;
  api()->glGetProgramivFn(service_id, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                          &active_uniform_max_length);
  if (CheckErrorCallbackState()) {
    *success = 0;
    return error::kNoError;
  }

  std::vector<char> name_buffer(active_uniform_max_length, 0);
  GLsizei length = 0;
  api()->glGetActiveUniformFn(service_id, index, name_buffer.size(), &length,
                              size, type, name_buffer.data());
  DCHECK(length <= active_uniform_max_length);
  *name = length > 0 ? std::string(name_buffer.data(), length) : std::string();
  *success = CheckErrorCallbackState() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockiv(
    GLuint program,
    GLuint index,
    GLenum pname,
    GLsizei bufSize,
    GLsizei* length,
    GLint* params) {
  api()->glGetActiveUniformBlockivRobustANGLEFn(
      GetProgramServiceID(program, resources_), index, pname, bufSize, length,
      params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformBlockName(
    GLuint program,
    GLuint index,
    std::string* name) {
  CheckErrorCallbackState();

  GLuint program_service_id = GetProgramServiceID(program, resources_);
  GLint max_name_length = 0;
  api()->glGetProgramivFn(program_service_id,
                          GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                          &max_name_length);

  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  std::vector<GLchar> buffer(max_name_length, 0);
  GLsizei length = 0;
  api()->glGetActiveUniformBlockNameFn(program_service_id, index,
                                       max_name_length, &length, buffer.data());
  DCHECK(length <= max_name_length);
  *name = length > 0 ? std::string(buffer.data(), length) : std::string();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetActiveUniformsiv(
    GLuint program,
    GLsizei count,
    const GLuint* indices,
    GLenum pname,
    GLint* params) {
  api()->glGetActiveUniformsivFn(GetProgramServiceID(program, resources_),
                                 count, indices, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttachedShaders(
    GLuint program,
    GLsizei maxcount,
    GLsizei* count,
    GLuint* shaders) {
  api()->glGetAttachedShadersFn(GetProgramServiceID(program, resources_),
                                maxcount, count, shaders);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetAttribLocation(GLuint program,
                                                              const char* name,
                                                              GLint* result) {
  *result = api()->glGetAttribLocationFn(
      GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBooleanv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLboolean* params) {
  return GetNumericHelper(pname, bufsize, length, params,
                          [this](GLenum pname, GLsizei bufsize, GLsizei* length,
                                 GLboolean* params) {
                            api()->glGetBooleanvRobustANGLEFn(pname, bufsize,
                                                              length, params);
                          });
}

error::Error GLES2DecoderPassthroughImpl::DoGetBooleani_v(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLboolean* data) {
  glGetBooleani_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteri64v(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint64* params) {
  CheckErrorCallbackState();
  api()->glGetBufferParameteri64vRobustANGLEFn(target, pname, bufsize, length,
                                               params);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }
  PatchGetBufferResults(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetBufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  CheckErrorCallbackState();
  api()->glGetBufferParameterivRobustANGLEFn(target, pname, bufsize, length,
                                             params);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }
  PatchGetBufferResults(target, pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetError(uint32_t* result) {
  FlushErrors();
  *result = PopError();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFloatv(GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLfloat* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [this](GLenum pname, GLsizei bufsize, GLsizei* length, GLfloat* params) {
        api()->glGetFloatvRobustANGLEFn(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataLocation(
    GLuint program,
    const char* name,
    GLint* result) {
  *result = api()->glGetFragDataLocationFn(
      GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFramebufferAttachmentParameteriv(
    GLenum target,
    GLenum attachment,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  GLenum updated_attachment = attachment;
  if (IsEmulatedFramebufferBound(target)) {
    // Update the attachment do the equivalent one in the emulated framebuffer
    if (!ModifyAttachmentForEmulatedFramebuffer(&updated_attachment)) {
      InsertError(GL_INVALID_OPERATION, "Invalid attachment.");
      *length = 0;
      return error::kNoError;
    }

    // Generate errors for parameter names that are only valid for non-default
    // framebuffers
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        InsertError(GL_INVALID_ENUM, "Invalid parameter name.");
        *length = 0;
        return error::kNoError;
    }
  }

  CheckErrorCallbackState();

  api()->glGetFramebufferAttachmentParameterivRobustANGLEFn(
      target, updated_attachment, pname, bufsize, length, params);

  if (CheckErrorCallbackState()) {
    DCHECK(*length == 0);
    return error::kNoError;
  }

  // Update the results of the query, if needed
  const error::Error error = PatchGetFramebufferAttachmentParameter(
      target, updated_attachment, pname, *length, params);
  if (error != error::kNoError) {
    *length = 0;
    return error;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64v(GLenum pname,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint64* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [this](GLenum pname, GLsizei bufsize, GLsizei* length, GLint64* params) {
        api()->glGetInteger64vRobustANGLEFn(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegeri_v(GLenum pname,
                                                          GLuint index,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLint* data) {
  glGetIntegeri_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetInteger64i_v(GLenum pname,
                                                            GLuint index,
                                                            GLsizei bufsize,
                                                            GLsizei* length,
                                                            GLint64* data) {
  glGetInteger64i_vRobustANGLE(pname, index, bufsize, length, data);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetIntegerv(GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  return GetNumericHelper(
      pname, bufsize, length, params,
      [this](GLenum pname, GLsizei bufsize, GLsizei* length, GLint* params) {
        api()->glGetIntegervRobustANGLEFn(pname, bufsize, length, params);
      });
}

error::Error GLES2DecoderPassthroughImpl::DoGetInternalformativ(GLenum target,
                                                                GLenum format,
                                                                GLenum pname,
                                                                GLsizei bufSize,
                                                                GLsizei* length,
                                                                GLint* params) {
  api()->glGetInternalformativRobustANGLEFn(target, format, pname, bufSize,
                                            length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramiv(GLuint program,
                                                         GLenum pname,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  api()->glGetProgramivRobustANGLEFn(GetProgramServiceID(program, resources_),
                                     pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoLog(
    GLuint program,
    std::string* infolog) {
  CheckErrorCallbackState();
  GLint info_log_len = 0;
  api()->glGetProgramivFn(GetProgramServiceID(program, resources_),
                          GL_INFO_LOG_LENGTH, &info_log_len);

  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  std::vector<char> buffer(info_log_len, 0);
  GLsizei length = 0;
  api()->glGetProgramInfoLogFn(GetProgramServiceID(program, resources_),
                               info_log_len, &length, buffer.data());
  DCHECK(length <= info_log_len);
  *infolog = length > 0 ? std::string(buffer.data(), length) : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInterfaceiv(
    GLuint program,
    GLenum program_interface,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  // glGetProgramInterfaceivRobustANGLE remains to be implemented in ANGLE.
  if (bufsize < 1) {
    return error::kOutOfBounds;
  }
  *length = 1;
  api()->glGetProgramInterfaceivFn(GetProgramServiceID(program, resources_),
                                   program_interface, pname, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramResourceiv(
    GLuint program,
    GLenum program_interface,
    GLuint index,
    GLsizei prop_count,
    const GLenum* props,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  api()->glGetProgramResourceivFn(GetProgramServiceID(program, resources_),
                                  program_interface, index, prop_count, props,
                                  bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramResourceIndex(
    GLuint program,
    GLenum program_interface,
    const char* name,
    GLuint* index) {
  *index = api()->glGetProgramResourceIndexFn(
      GetProgramServiceID(program, resources_), program_interface, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramResourceLocation(
    GLuint program,
    GLenum program_interface,
    const char* name,
    GLint* location) {
  *location = api()->glGetProgramResourceLocationFn(
      GetProgramServiceID(program, resources_), program_interface, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramResourceName(
    GLuint program,
    GLenum program_interface,
    GLuint index,
    std::string* name) {
  CheckErrorCallbackState();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint max_name_length = 0;
  api()->glGetProgramInterfaceivFn(service_id, program_interface,
                                   GL_MAX_NAME_LENGTH, &max_name_length);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  std::vector<GLchar> buffer(max_name_length, 0);
  GLsizei length = 0;
  api()->glGetProgramResourceNameFn(service_id, program_interface, index,
                                    max_name_length, &length, buffer.data());
  DCHECK_LE(length, max_name_length);
  *name = length > 0 ? std::string(buffer.data(), length) : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRenderbufferParameteriv(
    GLenum target,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  api()->glGetRenderbufferParameterivRobustANGLEFn(target, pname, bufsize,
                                                   length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLfloat* params) {
  api()->glGetSamplerParameterfvRobustANGLEFn(
      GetSamplerServiceID(sampler, resources_), pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  api()->glGetSamplerParameterivRobustANGLEFn(
      GetSamplerServiceID(sampler, resources_), pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderiv(GLuint shader,
                                                        GLenum pname,
                                                        GLsizei bufsize,
                                                        GLsizei* length,
                                                        GLint* params) {
  api()->glGetShaderivRobustANGLEFn(GetShaderServiceID(shader, resources_),
                                    pname, bufsize, length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderInfoLog(
    GLuint shader,
    std::string* infolog) {
  CheckErrorCallbackState();

  GLuint service_id = GetShaderServiceID(shader, resources_);
  GLint info_log_len = 0;
  api()->glGetShaderivFn(service_id, GL_INFO_LOG_LENGTH, &info_log_len);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  std::vector<char> buffer(info_log_len, 0);
  GLsizei length = 0;
  api()->glGetShaderInfoLogFn(service_id, info_log_len, &length, buffer.data());
  DCHECK(length <= info_log_len);
  *infolog = length > 0 ? std::string(buffer.data(), length) : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderPrecisionFormat(
    GLenum shadertype,
    GLenum precisiontype,
    GLint* range,
    GLint* precision,
    int32_t* success) {
  CheckErrorCallbackState();
  api()->glGetShaderPrecisionFormatFn(shadertype, precisiontype, range,
                                      precision);
  *success = CheckErrorCallbackState() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetShaderSource(
    GLuint shader,
    std::string* source) {
  CheckErrorCallbackState();

  GLuint shader_service_id = GetShaderServiceID(shader, resources_);
  GLint shader_source_length = 0;
  api()->glGetShaderivFn(shader_service_id, GL_SHADER_SOURCE_LENGTH,
                         &shader_source_length);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  std::vector<char> buffer(shader_source_length, 0);
  GLsizei length = 0;
  api()->glGetShaderSourceFn(shader_service_id, shader_source_length, &length,
                             buffer.data());
  DCHECK(length <= shader_source_length);
  *source = shader_source_length > 0 ? std::string(buffer.data(), length)
                                     : std::string();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetString(GLenum name,
                                                      uint32_t bucket_id) {
  std::string extensions;
  const char* str = nullptr;

  switch (name) {
    case GL_VERSION:
      str = GetServiceVersionString(feature_info_.get());
      break;
    case GL_SHADING_LANGUAGE_VERSION:
      str = GetServiceShadingLanguageVersionString(feature_info_.get());
      break;
    case GL_EXTENSIONS: {
      extensions = gfx::MakeExtensionString(feature_info_->extensions());
      str = extensions.c_str();
      break;
    }
    default:
      str = reinterpret_cast<const char*>(api()->glGetStringFn(name));
      break;
  }

  Bucket* bucket = CreateBucket(bucket_id);
  bucket->SetFromString(str);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetSynciv(GLuint sync,
                                                      GLenum pname,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* values) {
  api()->glGetSyncivFn(GetSyncServiceID(sync, resources_), pname, bufsize,
                       length, values);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameterfv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  api()->glGetTexParameterfvRobustANGLEFn(target, pname, bufsize, length,
                                          params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTexParameteriv(GLenum target,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  api()->glGetTexParameterivRobustANGLEFn(target, pname, bufsize, length,
                                          params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVarying(
    GLuint program,
    GLuint index,
    GLsizei* size,
    GLenum* type,
    std::string* name,
    int32_t* success) {
  CheckErrorCallbackState();

  GLuint service_id = GetProgramServiceID(program, resources_);
  GLint transform_feedback_varying_max_length = 0;
  api()->glGetProgramivFn(service_id, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                          &transform_feedback_varying_max_length);
  if (CheckErrorCallbackState()) {
    *success = 0;
    return error::kNoError;
  }

  std::vector<char> name_buffer(transform_feedback_varying_max_length, 0);
  GLsizei length = 0;
  api()->glGetTransformFeedbackVaryingFn(service_id, index, name_buffer.size(),
                                         &length, size, type,
                                         name_buffer.data());
  DCHECK(length <= transform_feedback_varying_max_length);
  *name = length > 0 ? std::string(name_buffer.data(), length) : std::string();
  *success = CheckErrorCallbackState() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlockIndex(
    GLuint program,
    const char* name,
    GLint* index) {
  *index = api()->glGetUniformBlockIndexFn(
      GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformfv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLfloat* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  api()->glGetUniformfvRobustANGLEFn(GetProgramServiceID(program, resources_),
                                     location, bufsize * sizeof(*params),
                                     length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformiv(GLuint program,
                                                         GLint location,
                                                         GLsizei bufsize,
                                                         GLsizei* length,
                                                         GLint* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  api()->glGetUniformivRobustANGLEFn(GetProgramServiceID(program, resources_),
                                     location, bufsize * sizeof(*params),
                                     length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformuiv(GLuint program,
                                                          GLint location,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          GLuint* params) {
  // GetUniform*RobustANGLE entry points expect bufsize in bytes like the entry
  // points in GL_EXT_robustness
  api()->glGetUniformuivRobustANGLEFn(GetProgramServiceID(program, resources_),
                                      location, bufsize * sizeof(*params),
                                      length, params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformIndices(
    GLuint program,
    GLsizei count,
    const char* const* names,
    GLsizei bufSize,
    GLuint* indices) {
  api()->glGetUniformIndicesFn(GetProgramServiceID(program, resources_), count,
                               names, indices);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformLocation(
    GLuint program,
    const char* name,
    GLint* location) {
  *location = api()->glGetUniformLocationFn(
      GetProgramServiceID(program, resources_), name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribfv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLfloat* params) {
  api()->glGetVertexAttribfvRobustANGLEFn(index, pname, bufsize, length,
                                          params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribiv(GLuint index,
                                                              GLenum pname,
                                                              GLsizei bufsize,
                                                              GLsizei* length,
                                                              GLint* params) {
  api()->glGetVertexAttribivRobustANGLEFn(index, pname, bufsize, length,
                                          params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIiv(GLuint index,
                                                               GLenum pname,
                                                               GLsizei bufsize,
                                                               GLsizei* length,
                                                               GLint* params) {
  api()->glGetVertexAttribIivRobustANGLEFn(index, pname, bufsize, length,
                                           params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribIuiv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* params) {
  api()->glGetVertexAttribIuivRobustANGLEFn(index, pname, bufsize, length,
                                            params);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetVertexAttribPointerv(
    GLuint index,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLuint* pointer) {
  std::array<void*, 1> temp_pointers{{nullptr}};
  GLsizei temp_length = 0;
  api()->glGetVertexAttribPointervRobustANGLEFn(
      index, pname, static_cast<GLsizei>(temp_pointers.size()), &temp_length,
      temp_pointers.data());
  DCHECK(temp_length >= 0 &&
         temp_length <= static_cast<GLsizei>(temp_pointers.size()) &&
         temp_length <= bufsize);
  for (GLsizei ii = 0; ii < temp_length; ii++) {
    pointer[ii] =
        static_cast<GLuint>(reinterpret_cast<uintptr_t>(temp_pointers[ii]));
  }
  *length = temp_length;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoHint(GLenum target, GLenum mode) {
  api()->glHintFn(target, mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  if (IsEmulatedFramebufferBound(target)) {
    // Update the attachment do the equivalent one in the emulated framebuffer
    if (!ModifyAttachmentsForEmulatedFramebuffer(&attachments_copy)) {
      InsertError(GL_INVALID_OPERATION, "Invalid attachment.");
      return error::kNoError;
    }
  }
  api()->glInvalidateFramebufferFn(target, count, attachments_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInvalidateSubFramebuffer(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLenum> attachments_copy(attachments, attachments + count);
  if (IsEmulatedFramebufferBound(target)) {
    // Update the attachment do the equivalent one in the emulated framebuffer
    if (!ModifyAttachmentsForEmulatedFramebuffer(&attachments_copy)) {
      InsertError(GL_INVALID_OPERATION, "Invalid attachment.");
      return error::kNoError;
    }
  }
  api()->glInvalidateSubFramebufferFn(target, count, attachments_copy.data(), x,
                                      y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsBuffer(GLuint buffer,
                                                     uint32_t* result) {
  *result =
      api()->glIsBufferFn(GetBufferServiceID(api(), buffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsEnabled(GLenum cap,
                                                      uint32_t* result) {
  *result = api()->glIsEnabledFn(cap);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsEnablediOES(GLenum target,
                                                          GLuint index,
                                                          uint32_t* result) {
  *result = api()->glIsEnablediOESFn(target, index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsFramebuffer(GLuint framebuffer,
                                                          uint32_t* result) {
  *result = api()->glIsFramebufferEXTFn(
      GetFramebufferServiceID(api(), framebuffer, &framebuffer_id_map_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsProgram(GLuint program,
                                                      uint32_t* result) {
  *result = api()->glIsProgramFn(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsRenderbuffer(GLuint renderbuffer,
                                                           uint32_t* result) {
  *result = api()->glIsRenderbufferEXTFn(
      GetRenderbufferServiceID(api(), renderbuffer, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSampler(GLuint sampler,
                                                      uint32_t* result) {
  *result = api()->glIsSamplerFn(GetSamplerServiceID(sampler, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsShader(GLuint shader,
                                                     uint32_t* result) {
  *result = api()->glIsShaderFn(GetShaderServiceID(shader, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsSync(GLuint sync,
                                                   uint32_t* result) {
  *result = api()->glIsSyncFn(GetSyncServiceID(sync, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTexture(GLuint texture,
                                                      uint32_t* result) {
  *result = api()->glIsTextureFn(
      GetTextureServiceID(api(), texture, resources_, false));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoIsTransformFeedback(
    GLuint transformfeedback,
    uint32_t* result) {
  *result = api()->glIsTransformFeedbackFn(GetTransformFeedbackServiceID(
      transformfeedback, &transform_feedback_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLineWidth(GLfloat width) {
  api()->glLineWidthFn(width);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLinkProgram(GLuint program) {
  TRACE_EVENT0("gpu", "GLES2DecoderPassthroughImpl::DoLinkProgram");
  SCOPED_UMA_HISTOGRAM_TIMER("GPU.PassthroughDoLinkProgramTime");
  GLuint program_service_id = GetProgramServiceID(program, resources_);

  // Call report progress to delay GPU Watchdog timeout.
  group_->ReportProgress();

  api()->glLinkProgramFn(program_service_id);

  group_->ReportProgress();

  // Program linking can be very slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  linking_program_service_id_ = program_service_id;

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMemoryBarrierEXT(
    GLbitfield barriers) {
  api()->glMemoryBarrierEXTFn(barriers);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMemoryBarrierByRegion(
    GLbitfield barriers) {
  api()->glMemoryBarrierByRegionFn(barriers);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMultiDrawBeginCHROMIUM(
    GLsizei drawcount) {
  if (drawcount < 0) {
    InsertError(GL_INVALID_VALUE, "drawcount cannot be negative.");
    return error::kNoError;
  }

  if (!multi_draw_manager_->Begin(drawcount)) {
    return error::kInvalidArguments;
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMultiDrawEndCHROMIUM() {
  MultiDrawManager::ResultData result;
  if (!multi_draw_manager_->End(&result)) {
    return error::kInvalidArguments;
  }
  switch (result.draw_function) {
    case MultiDrawManager::DrawFunction::DrawArrays:
      api()->glMultiDrawArraysANGLEFn(result.mode, result.firsts.data(),
                                      result.counts.data(), result.drawcount);
      return error::kNoError;
    case MultiDrawManager::DrawFunction::DrawArraysInstanced:
      api()->glMultiDrawArraysInstancedANGLEFn(
          result.mode, result.firsts.data(), result.counts.data(),
          result.instance_counts.data(), result.drawcount);
      return error::kNoError;
    case MultiDrawManager::DrawFunction::DrawArraysInstancedBaseInstance:
      api()->glMultiDrawArraysInstancedBaseInstanceANGLEFn(
          result.mode, result.firsts.data(), result.counts.data(),
          result.instance_counts.data(), result.baseinstances.data(),
          result.drawcount);
      return error::kNoError;
    case MultiDrawManager::DrawFunction::DrawElements:
      api()->glMultiDrawElementsANGLEFn(result.mode, result.counts.data(),
                                        result.type, result.indices.data(),
                                        result.drawcount);
      return error::kNoError;
    case MultiDrawManager::DrawFunction::DrawElementsInstanced:
      api()->glMultiDrawElementsInstancedANGLEFn(
          result.mode, result.counts.data(), result.type, result.indices.data(),
          result.instance_counts.data(), result.drawcount);
      return error::kNoError;
    case MultiDrawManager::DrawFunction::
        DrawElementsInstancedBaseVertexBaseInstance:
      api()->glMultiDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
          result.mode, result.counts.data(), result.type, result.indices.data(),
          result.instance_counts.data(), result.basevertices.data(),
          result.baseinstances.data(), result.drawcount);
      return error::kNoError;
    default:
      NOTREACHED_IN_MIGRATION();
      return error::kLostContext;
  }
}

error::Error GLES2DecoderPassthroughImpl::DoPauseTransformFeedback() {
  api()->glPauseTransformFeedbackFn();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPixelStorei(GLenum pname,
                                                        GLint param) {
  api()->glPixelStoreiFn(pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPolygonOffset(GLfloat factor,
                                                          GLfloat units) {
  api()->glPolygonOffsetFn(factor, units);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadBuffer(GLenum src) {
  api()->glReadBufferFn(src);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWritePixelsYUVINTERNAL(
    GLuint src_width,
    GLuint src_height,
    GLuint src_row_bytes_plane1,
    GLuint src_row_bytes_plane2,
    GLuint src_row_bytes_plane3,
    GLuint src_row_bytes_plane4,
    GLuint src_yuv_plane_config,
    GLuint src_yuv_subsampling,
    GLuint src_yuv_datatype,
    GLint shm_id,
    GLuint shm_offset,
    GLuint pixels_offset_plane1,
    GLuint pixels_offset_plane2,
    GLuint pixels_offset_plane3,
    GLuint pixels_offset_plane4) {
  if (!lazy_context_) {
    lazy_context_ = LazySharedContextState::Create(this);
    if (!lazy_context_) {
      return error::kNoError;
    }
  }
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  auto* shared_context_state = lazy_context_->shared_context_state();
  ui::ScopedMakeCurrent smc(shared_context_state->context(),
                            shared_context_state->surface());

  if (src_yuv_plane_config > static_cast<int>(SkYUVAInfo::PlaneConfig::kLast)) {
    InsertError(GL_INVALID_ENUM,
                "src_yuv_plane_config must be a valid PlaneConfig");
    return error::kNoError;
  }
  if (src_yuv_subsampling > static_cast<int>(SkYUVAInfo::Subsampling::kLast)) {
    InsertError(GL_INVALID_ENUM,
                "src_yuv_subsampling must be a valid Subsampling");
    return error::kNoError;
  }
  if (src_yuv_datatype > static_cast<int>(SkYUVAPixmapInfo::DataType::kLast)) {
    InsertError(GL_INVALID_ENUM,
                "src_yuv_datatype must be a valid SkYUVAPixmapInfo::DataType");
    return error::kNoError;
  }

  auto* mailbox_memory =
      GetSharedMemoryAs<Mailbox*>(shm_id, shm_offset, sizeof(Mailbox));
  if (!mailbox_memory) {
    InsertError(GL_INVALID_OPERATION,
                "Failed to retrieve memory for WritePixelsYUV mailbox");
    return error::kOutOfBounds;
  }

  auto* representation_factory = group_->shared_image_representation_factory();

  Mailbox dest_mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox_memory));
  DLOG_IF(ERROR, !dest_mailbox.Verify())
      << "WritePixelsYUV was passed an invalid mailbox";
  auto dest_shared_image =
      representation_factory->ProduceSkia(dest_mailbox, shared_context_state);
  if (!dest_shared_image) {
    InsertError(GL_INVALID_OPERATION,
                "Attempting to write to unknown mailbox.");
    return error::kOutOfBounds;
  }

  SkYUVAInfo::PlaneConfig src_plane_config =
      static_cast<SkYUVAInfo::PlaneConfig>(src_yuv_plane_config);
  SkYUVAInfo::Subsampling src_subsampling =
      static_cast<SkYUVAInfo::Subsampling>(src_yuv_subsampling);
  SkYUVAPixmapInfo::DataType src_datatype =
      static_cast<SkYUVAPixmapInfo::DataType>(src_yuv_datatype);
  viz::SharedImageFormat dest_format = dest_shared_image->format();
  if (!dest_format.is_multi_plane()) {
    InsertError(GL_INVALID_OPERATION,
                "dest_format must be a valid multiplanar SharedImageFormat.");
    return error::kNoError;
  }
  if (src_plane_config != ToSkYUVAPlaneConfig(dest_format)) {
    InsertError(GL_INVALID_OPERATION,
                "PlaneConfig mismatch between source texture format and "
                "the destination shared image format");
    return error::kNoError;
  }
  if (src_subsampling != ToSkYUVASubsampling(dest_format)) {
    InsertError(GL_INVALID_OPERATION,
                "Subsampling mismatch between source texture format and "
                "the destination shared image format");
    return error::kNoError;
  }
  if (src_datatype != ToSkYUVADataType(dest_format)) {
    InsertError(GL_INVALID_OPERATION,
                "ChannelFormat mismatch between source texture format "
                "and the destination shared image format");
    return error::kNoError;
  }

  SkYUVAInfo yuv_info(SkISize::Make(src_width, src_height), src_plane_config,
                      src_subsampling,
                      SkYUVColorSpace::kIdentity_SkYUVColorSpace);
  if (yuv_info.numPlanes() != dest_format.NumberOfPlanes()) {
    InsertError(GL_INVALID_OPERATION,
                "Planes mismatch between source texture format and the "
                "destination shared image format");
    return error::kNoError;
  }

  if (gfx::Size(src_width, src_height) != dest_shared_image->size()) {
    InsertError(GL_INVALID_OPERATION,
                "Unexpected size for multiplanar shared image");
    return error::kNoError;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  // Allow uncleared access, as we manually handle clear tracking.
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      dest_scoped_access = dest_shared_image->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes,
          /*use_sk_surface=*/false);
  if (!dest_scoped_access) {
    InsertError(GL_INVALID_OPERATION, "Failed to begin scoped write access.");
    return error::kNoError;
  }
  if (!begin_semaphores.empty()) {
    // The Graphite SharedImage representation does not set semaphores.
    CHECK(shared_context_state->gr_context());
    bool result = shared_context_state->gr_context()->wait(
        begin_semaphores.size(), begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
    CHECK(result);
  }

  size_t row_bytes[SkYUVAInfo::kMaxPlanes];
  row_bytes[0] = src_row_bytes_plane1;
  row_bytes[1] = src_row_bytes_plane2;
  row_bytes[2] = src_row_bytes_plane3;
  row_bytes[3] = src_row_bytes_plane4;

  size_t plane_offsets[SkYUVAInfo::kMaxPlanes];
  plane_offsets[0] = pixels_offset_plane1;
  plane_offsets[1] = pixels_offset_plane2;
  plane_offsets[2] = pixels_offset_plane3;
  plane_offsets[3] = pixels_offset_plane4;

  std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pixmaps = {};

  size_t prev_byte_size = 0;
  for (int plane = 0; plane < yuv_info.numPlanes(); plane++) {
    auto color_type = viz::ToClosestSkColorType(true, dest_format, plane);
    auto plane_size =
        dest_format.GetPlaneSize(plane, gfx::Size(src_width, src_height));
    SkImageInfo src_info =
        SkImageInfo::Make(gfx::SizeToSkISize(plane_size), color_type,
                          SkAlphaType::kPremul_SkAlphaType, nullptr);

    if (row_bytes[plane] < src_info.minRowBytes()) {
      dest_scoped_access->ApplyBackendSurfaceEndState();
      InsertError(GL_INVALID_VALUE,
                  "row_bytes must be >= "
                  "SkImageInfo::minRowBytes() for source image.");
      return error::kNoError;
    }

    size_t byte_size = src_info.computeByteSize(row_bytes[plane]);
    if (byte_size > UINT32_MAX) {
      dest_scoped_access->ApplyBackendSurfaceEndState();
      InsertError(GL_INVALID_VALUE,
                  "Cannot request a memory chunk larger than UINT32_MAX bytes");
      return error::kOutOfBounds;
    }
    if (plane > 0 &&
        plane_offsets[plane] < plane_offsets[plane - 1] + prev_byte_size) {
      dest_scoped_access->ApplyBackendSurfaceEndState();
      InsertError(GL_INVALID_VALUE,
                  "plane_offsets[plane] must be >= plane_offsets[plane "
                  "- 1] + prev_byte_size");
      return error::kOutOfBounds;
    }

    // The pixels are stored contiguously for all the planes one after another
    // with padding.
    void* pixel_data = GetSharedMemoryAs<void*>(
        shm_id, shm_offset + plane_offsets[plane], byte_size);
    if (!pixel_data) {
      dest_scoped_access->ApplyBackendSurfaceEndState();
      InsertError(GL_INVALID_OPERATION, "Couldn't retrieve pixel data.");
      return error::kNoError;
    }

    // Create an SkPixmap for the plane.
    pixmaps[plane] = SkPixmap(src_info, pixel_data, row_bytes[plane]);
    prev_byte_size = byte_size;
  }

  // Try a direct texture upload without using SkSurface.
  CopySharedImageHelper helper(group_->shared_image_representation_factory(),
                               lazy_context_->shared_context_state());
  auto helper_result = helper.WritePixelsYUV(
      src_width, src_height, pixmaps, std::move(end_semaphores),
      std::move(dest_shared_image), std::move(dest_scoped_access));
  if (!helper_result.has_value()) {
    InsertError(helper_result.error().gl_error, helper_result.error().msg);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadbackARGBImagePixelsINTERNAL(
    GLint src_x,
    GLint src_y,
    GLint plane_index,
    GLuint dst_width,
    GLuint dst_height,
    GLuint row_bytes,
    GLuint dst_sk_color_type,
    GLuint dst_sk_alpha_type,
    GLint shm_id,
    GLuint shm_offset,
    GLuint color_space_offset,
    GLuint pixels_offset,
    GLuint mailbox_offset) {
  if (!lazy_context_) {
    lazy_context_ = LazySharedContextState::Create(this);
    if (!lazy_context_) {
      return error::kNoError;
    }
  }
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ui::ScopedMakeCurrent smc(lazy_context_->shared_context_state()->context(),
                            lazy_context_->shared_context_state()->surface());

  if (dst_sk_color_type > kLastEnum_SkColorType) {
    InsertError(GL_INVALID_ENUM,
                "dst_sk_color_type must be a valid SkColorType");
    return error::kNoError;
  }
  if (dst_sk_alpha_type > kLastEnum_SkAlphaType) {
    InsertError(GL_INVALID_ENUM,
                "dst_sk_alpha_type must be a valid SkAlphaType");
    return error::kNoError;
  }

  auto* mailbox_memory = GetSharedMemoryAs<Mailbox*>(
      shm_id, shm_offset + mailbox_offset, sizeof(Mailbox));
  if (!mailbox_memory) {
    InsertError(GL_INVALID_OPERATION,
                "Failed to retrieve memory for readPixels mailbox");
    return error::kOutOfBounds;
  }
  Mailbox source_mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox_memory));
  DLOG_IF(ERROR, !source_mailbox.Verify())
      << "ReadbackImagePixels was passed an invalid mailbox";
  auto source_shared_image =
      group_->shared_image_representation_factory()->ProduceSkia(
          source_mailbox, scoped_refptr<gpu::SharedContextState>(
                              lazy_context_->shared_context_state()));
  if (!source_shared_image) {
    InsertError(GL_INVALID_VALUE, "Unknown mailbox");
    return error::kNoError;
  }

  viz::SharedImageFormat source_format = source_shared_image->format();

  // If present, the color space is serialized into shared memory after the
  // Result and before the mailbox.
  if (color_space_offset > mailbox_offset) {
    InsertError(GL_INVALID_VALUE,
                "|mailbox_offset| must be >= |color_space_offset|");
    return error::kOutOfBounds;
  }
  unsigned int color_space_size = mailbox_offset - color_space_offset;

  sk_sp<SkColorSpace> dst_color_space;
  if (color_space_size) {
    // For multiplanar formats readback is per plane, and destination color
    // space must be nullptr to avoid unexpected color conversions.
    if (source_format.is_multi_plane()) {
      InsertError(GL_INVALID_OPERATION,
                  "Unexpected color space for multiplanar shared image");
      return error::kNoError;
    }
    void* color_space_bytes = GetSharedMemoryAs<void*>(
        shm_id, shm_offset + color_space_offset, color_space_size);
    if (!color_space_bytes) {
      InsertError(GL_INVALID_OPERATION,
                  "Failed to retrieve serialized SkColorSpace.");
      return error::kOutOfBounds;
    }
    dst_color_space =
        SkColorSpace::Deserialize(color_space_bytes, color_space_size);
    if (!dst_color_space) {
      InsertError(GL_INVALID_OPERATION,
                  "Failed to deserialize expected SkColorSpace");
      return error::kNoError;
    }
  }

  SkImageInfo dst_info = SkImageInfo::Make(
      dst_width, dst_height, static_cast<SkColorType>(dst_sk_color_type),
      static_cast<SkAlphaType>(dst_sk_alpha_type), std::move(dst_color_space));

  if (row_bytes < dst_info.minRowBytes()) {
    InsertError(GL_INVALID_VALUE,
                "row_bytes be >= "
                "SkImageInfo::minRowBytes() for dest image.");
    return error::kNoError;
  }

  size_t byte_size = dst_info.computeByteSize(row_bytes);
  if (byte_size > UINT32_MAX) {
    InsertError(GL_INVALID_VALUE,
                "Cannot request a memory chunk larger than UINT32_MAX bytes");
    return error::kNoError;
  }

  void* pixel_address =
      GetSharedMemoryAs<void*>(shm_id, shm_offset + pixels_offset, byte_size);
  if (!pixel_address) {
    InsertError(GL_INVALID_OPERATION,
                "Failed to retrieve memory for readPixels output");
    return error::kOutOfBounds;
  }

  typedef cmds::ReadbackARGBImagePixelsINTERNAL::Result Result;
  Result* result =
      GetSharedMemoryAs<Result*>(shm_id, shm_offset, sizeof(Result));
  if (!result) {
    InsertError(GL_INVALID_OPERATION,
                "Failed to retrieve memory for readPixels result");
    return error::kOutOfBounds;
  }
  if (!source_format.IsValidPlaneIndex(plane_index)) {
    InsertError(GL_INVALID_OPERATION, "Invalid plane_index");
    return error::kNoError;
  }

  CopySharedImageHelper helper(group_->shared_image_representation_factory(),
                               lazy_context_->shared_context_state());
  auto helper_result =
      helper.ReadPixels(src_x, src_y, plane_index, row_bytes, dst_info,
                        pixel_address, std::move(source_shared_image));

  if (!helper_result.has_value()) {
    InsertError(helper_result.error().gl_error, helper_result.error().msg);
  } else {
    *result = 1;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadPixels(GLint x,
                                                       GLint y,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       GLsizei* columns,
                                                       GLsizei* rows,
                                                       void* pixels,
                                                       int32_t* success) {
  CheckErrorCallbackState();
  ScopedPackStateRowLengthReset reset_row_length(
      api(), bufsize != 0 && feature_info_->gl_version_info().is_es3);
  api()->glReadPixelsRobustANGLEFn(x, y, width, height, format, type, bufsize,
                                   length, columns, rows, pixels);
  *success = CheckErrorCallbackState() ? 0 : 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReadPixelsAsync(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLsizei bufsize,
    GLsizei* length,
    GLsizei* columns,
    GLsizei* rows,
    uint32_t pixels_shm_id,
    uint32_t pixels_shm_offset,
    uint32_t result_shm_id,
    uint32_t result_shm_offset) {
  DCHECK(feature_info_->feature_flags().use_async_readpixels &&
         bound_buffers_[GL_PIXEL_PACK_BUFFER] == 0);

  CheckErrorCallbackState();
  ScopedPackStateRowLengthReset reset_row_length(
      api(), bufsize != 0 && feature_info_->gl_version_info().is_es3);

  PendingReadPixels pending_read_pixels;
  pending_read_pixels.pixels_shm_id = pixels_shm_id;
  pending_read_pixels.pixels_shm_offset = pixels_shm_offset;
  pending_read_pixels.result_shm_id = result_shm_id;
  pending_read_pixels.result_shm_offset = result_shm_offset;

  api()->glGenBuffersARBFn(1, &pending_read_pixels.buffer_service_id);
  api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB,
                        pending_read_pixels.buffer_service_id);

  // GL_STREAM_READ is not available until ES3.
  const GLenum usage_hint = feature_info_->gl_version_info().IsAtLeastGLES(3, 0)
                                ? GL_STREAM_READ
                                : GL_STATIC_DRAW;

  const uint32_t bytes_per_pixel =
      GLES2Util::ComputeImageGroupSize(format, type);
  if (bytes_per_pixel == 0) {
    InsertError(GL_INVALID_ENUM, "Invalid ReadPixels format or type.");
    return error::kNoError;
  }

  if (width < 0 || height < 0) {
    InsertError(GL_INVALID_VALUE, "Width and height cannot be negative.");
    return error::kNoError;
  }

  if (!base::CheckMul(bytes_per_pixel, width, height)
           .AssignIfValid(&pending_read_pixels.pixels_size)) {
    return error::kOutOfBounds;
  }

  api()->glBufferDataFn(GL_PIXEL_PACK_BUFFER_ARB,
                        pending_read_pixels.pixels_size, nullptr, usage_hint);

  // No need to worry about ES3 pixel pack parameters, because no
  // PIXEL_PACK_BUFFER is bound, and all these settings haven't been
  // sent to GL.
  api()->glReadPixelsFn(x, y, width, height, format, type, nullptr);

  api()->glBindBufferFn(GL_PIXEL_PACK_BUFFER_ARB, 0);

  // Test for errors now before creating a fence
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  pending_read_pixels.fence = gl::GLFence::Create();
  if (!pending_read_pixels.fence) {
    InsertError(GL_INVALID_OPERATION, "gl::GLFence::Create() failed.");
    MarkContextLost(error::kUnknown);
    group_->LoseContexts(error::kUnknown);
    return error::kLostContext;
  }

  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  pending_read_pixels_.push_back(std::move(pending_read_pixels));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoReleaseShaderCompiler() {
  api()->glReleaseShaderCompilerFn();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorage(
    GLenum target,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  api()->glRenderbufferStorageEXTFn(target, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResumeTransformFeedback() {
  api()->glResumeTransformFeedbackFn();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSampleCoverage(GLclampf value,
                                                           GLboolean invert) {
  api()->glSampleCoverageFn(value, invert);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterf(GLuint sampler,
                                                              GLenum pname,
                                                              GLfloat param) {
  api()->glSamplerParameterfFn(GetSamplerServiceID(sampler, resources_), pname,
                               param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameterfv(
    GLuint sampler,
    GLenum pname,
    const volatile GLfloat* params) {
  std::array<GLfloat, 1> params_copy{{params[0]}};
  api()->glSamplerParameterfvRobustANGLEFn(
      GetSamplerServiceID(sampler, resources_), pname,
      static_cast<GLsizei>(params_copy.size()), params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteri(GLuint sampler,
                                                              GLenum pname,
                                                              GLint param) {
  api()->glSamplerParameteriFn(GetSamplerServiceID(sampler, resources_), pname,
                               param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSamplerParameteriv(
    GLuint sampler,
    GLenum pname,
    const volatile GLint* params) {
  std::array<GLint, 1> params_copy{{params[0]}};
  api()->glSamplerParameterivRobustANGLEFn(
      GetSamplerServiceID(sampler, resources_), pname,
      static_cast<GLsizei>(params_copy.size()), params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoScissor(GLint x,
                                                    GLint y,
                                                    GLsizei width,
                                                    GLsizei height) {
  api()->glScissorFn(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoShaderBinary(GLsizei n,
                                                         const GLuint* shaders,
                                                         GLenum binaryformat,
                                                         const void* binary,
                                                         GLsizei length) {
#if 1  // No binary shader support.
  InsertError(GL_INVALID_ENUM, "Invalid enum.");
  return error::kNoError;
#else
  std::vector<GLuint> service_shaders(n, 0);
  for (GLsizei i = 0; i < n; i++) {
    service_shaders[i] = GetShaderServiceID(shaders[i], resources_);
  }
  api()->glShaderBinaryFn(n, service_shaders.data(), binaryformat, binary,
                          length);
  return error::kNoError;
#endif
}

error::Error GLES2DecoderPassthroughImpl::DoShaderSource(GLuint shader,
                                                         GLsizei count,
                                                         const char** string,
                                                         const GLint* length) {
  api()->glShaderSourceFn(GetShaderServiceID(shader, resources_), count, string,
                          length);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFunc(GLenum func,
                                                        GLint ref,
                                                        GLuint mask) {
  api()->glStencilFuncFn(func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilFuncSeparate(GLenum face,
                                                                GLenum func,
                                                                GLint ref,
                                                                GLuint mask) {
  api()->glStencilFuncSeparateFn(face, func, ref, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMask(GLuint mask) {
  api()->glStencilMaskFn(mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilMaskSeparate(GLenum face,
                                                                GLuint mask) {
  api()->glStencilMaskSeparateFn(face, mask);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOp(GLenum fail,
                                                      GLenum zfail,
                                                      GLenum zpass) {
  api()->glStencilOpFn(fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoStencilOpSeparate(GLenum face,
                                                              GLenum fail,
                                                              GLenum zfail,
                                                              GLenum zpass) {
  api()->glStencilOpSeparateFn(face, fail, zfail, zpass);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexImage2D(GLenum target,
                                                       GLint level,
                                                       GLint internalformat,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLint border,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei image_size,
                                                       const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      api(), image_size != 0 && feature_info_->gl_version_info().is_es3, false);

  CheckErrorCallbackState();
  api()->glTexImage2DRobustANGLEFn(target, level, internalformat, width, height,
                                   border, format, type, image_size, pixels);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  UpdateTextureSizeFromTarget(target);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexImage3D(GLenum target,
                                                       GLint level,
                                                       GLint internalformat,
                                                       GLsizei width,
                                                       GLsizei height,
                                                       GLsizei depth,
                                                       GLint border,
                                                       GLenum format,
                                                       GLenum type,
                                                       GLsizei image_size,
                                                       const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      api(), image_size != 0 && feature_info_->gl_version_info().is_es3, true);

  CheckErrorCallbackState();
  api()->glTexImage3DRobustANGLEFn(target, level, internalformat, width, height,
                                   depth, border, format, type, image_size,
                                   pixels);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  UpdateTextureSizeFromTarget(target);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterf(GLenum target,
                                                          GLenum pname,
                                                          GLfloat param) {
  // Don't allow clients to modify the resource initialization state.
  if (pname == GL_RESOURCE_INITIALIZED_ANGLE) {
    InsertError(GL_INVALID_ENUM, "Invalid enum.");
    return error::kNoError;
  }

  api()->glTexParameterfFn(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameterfv(
    GLenum target,
    GLenum pname,
    const volatile GLfloat* params) {
  // Don't allow clients to modify the resource initialization state.
  if (pname == GL_RESOURCE_INITIALIZED_ANGLE) {
    InsertError(GL_INVALID_ENUM, "Invalid enum.");
    return error::kNoError;
  }

  std::array<GLfloat, 1> params_copy{{params[0]}};
  api()->glTexParameterfvRobustANGLEFn(target, pname,
                                       static_cast<GLsizei>(params_copy.size()),
                                       params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameteri(GLenum target,
                                                          GLenum pname,
                                                          GLint param) {
  // Don't allow clients to modify the resource initialization state.
  if (pname == GL_RESOURCE_INITIALIZED_ANGLE) {
    InsertError(GL_INVALID_ENUM, "Invalid enum.");
    return error::kNoError;
  }

  api()->glTexParameteriFn(target, pname, param);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexParameteriv(
    GLenum target,
    GLenum pname,
    const volatile GLint* params) {
  // Don't allow clients to modify the resource initialization state.
  if (pname == GL_RESOURCE_INITIALIZED_ANGLE) {
    InsertError(GL_INVALID_ENUM, "Invalid enum.");
    return error::kNoError;
  }

  std::array<GLint, 1> params_copy{{params[0]}};
  api()->glTexParameterivRobustANGLEFn(target, pname,
                                       static_cast<GLsizei>(params_copy.size()),
                                       params_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage3D(GLenum target,
                                                         GLsizei levels,
                                                         GLenum internalFormat,
                                                         GLsizei width,
                                                         GLsizei height,
                                                         GLsizei depth) {
  CheckErrorCallbackState();
  api()->glTexStorage3DFn(target, levels, internalFormat, width, height, depth);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  UpdateTextureSizeFromTarget(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexSubImage2D(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei image_size,
                                                          const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      api(), image_size != 0 && feature_info_->gl_version_info().is_es3, false);
  api()->glTexSubImage2DRobustANGLEFn(target, level, xoffset, yoffset, width,
                                      height, format, type, image_size, pixels);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexSubImage3D(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLint zoffset,
                                                          GLsizei width,
                                                          GLsizei height,
                                                          GLsizei depth,
                                                          GLenum format,
                                                          GLenum type,
                                                          GLsizei image_size,
                                                          const void* pixels) {
  ScopedUnpackStateButAlignmentReset reset_unpack(
      api(), image_size != 0 && feature_info_->gl_version_info().is_es3, true);
  api()->glTexSubImage3DRobustANGLEFn(target, level, xoffset, yoffset, zoffset,
                                      width, height, depth, format, type,
                                      image_size, pixels);

  // Texture data upload can be slow.  Exit command processing to allow for
  // context preemption and GPU watchdog checks.
  ExitCommandProcessingEarly();

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTransformFeedbackVaryings(
    GLuint program,
    GLsizei count,
    const char** varyings,
    GLenum buffermode) {
  api()->glTransformFeedbackVaryingsFn(GetProgramServiceID(program, resources_),
                                       count, varyings, buffermode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1f(GLint location,
                                                      GLfloat x) {
  api()->glUniform1fFn(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  api()->glUniform1fvFn(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1i(GLint location, GLint x) {
  api()->glUniform1iFn(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  api()->glUniform1ivFn(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1ui(GLint location,
                                                       GLuint x) {
  api()->glUniform1uiFn(location, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform1uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  api()->glUniform1uivFn(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y) {
  api()->glUniform2fFn(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  api()->glUniform2fvFn(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2i(GLint location,
                                                      GLint x,
                                                      GLint y) {
  api()->glUniform2iFn(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  api()->glUniform2ivFn(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2ui(GLint location,
                                                       GLuint x,
                                                       GLuint y) {
  api()->glUniform2uiFn(location, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform2uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  api()->glUniform2uivFn(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z) {
  api()->glUniform3fFn(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  api()->glUniform3fvFn(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z) {
  api()->glUniform3iFn(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  api()->glUniform3ivFn(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z) {
  api()->glUniform3uiFn(location, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform3uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  api()->glUniform3uivFn(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4f(GLint location,
                                                      GLfloat x,
                                                      GLfloat y,
                                                      GLfloat z,
                                                      GLfloat w) {
  api()->glUniform4fFn(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4fv(
    GLint location,
    GLsizei count,
    const volatile GLfloat* v) {
  api()->glUniform4fvFn(location, count, const_cast<const GLfloat*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4i(GLint location,
                                                      GLint x,
                                                      GLint y,
                                                      GLint z,
                                                      GLint w) {
  api()->glUniform4iFn(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4iv(
    GLint location,
    GLsizei count,
    const volatile GLint* v) {
  api()->glUniform4ivFn(location, count, const_cast<const GLint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4ui(GLint location,
                                                       GLuint x,
                                                       GLuint y,
                                                       GLuint z,
                                                       GLuint w) {
  api()->glUniform4uiFn(location, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniform4uiv(
    GLint location,
    GLsizei count,
    const volatile GLuint* v) {
  api()->glUniform4uivFn(location, count, const_cast<const GLuint*>(v));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformBlockBinding(
    GLuint program,
    GLuint index,
    GLuint binding) {
  api()->glUniformBlockBindingFn(GetProgramServiceID(program, resources_),
                                 index, binding);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix2fvFn(location, count, transpose,
                              const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix2x3fvFn(location, count, transpose,
                                const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix2x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix2x4fvFn(location, count, transpose,
                                const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix3fvFn(location, count, transpose,
                              const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix3x2fvFn(location, count, transpose,
                                const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix3x4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix3x4fvFn(location, count, transpose,
                                const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix4fvFn(location, count, transpose,
                              const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x2fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix4x2fvFn(location, count, transpose,
                                const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUniformMatrix4x3fv(
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const volatile GLfloat* value) {
  api()->glUniformMatrix4x3fvFn(location, count, transpose,
                                const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUseProgram(GLuint program) {
  api()->glUseProgramFn(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoValidateProgram(GLuint program) {
  api()->glValidateProgramFn(GetProgramServiceID(program, resources_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1f(GLuint indx,
                                                           GLfloat x) {
  api()->glVertexAttrib1fFn(indx, x);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib1fv(
    GLuint indx,
    const volatile GLfloat* values) {
  api()->glVertexAttrib1fvFn(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y) {
  api()->glVertexAttrib2fFn(indx, x, y);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib2fv(
    GLuint indx,
    const volatile GLfloat* values) {
  api()->glVertexAttrib2fvFn(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z) {
  api()->glVertexAttrib3fFn(indx, x, y, z);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib3fv(
    GLuint indx,
    const volatile GLfloat* values) {
  api()->glVertexAttrib3fvFn(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4f(GLuint indx,
                                                           GLfloat x,
                                                           GLfloat y,
                                                           GLfloat z,
                                                           GLfloat w) {
  api()->glVertexAttrib4fFn(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttrib4fv(
    GLuint indx,
    const volatile GLfloat* values) {
  api()->glVertexAttrib4fvFn(indx, const_cast<const GLfloat*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4i(GLuint indx,
                                                            GLint x,
                                                            GLint y,
                                                            GLint z,
                                                            GLint w) {
  api()->glVertexAttribI4iFn(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4iv(
    GLuint indx,
    const volatile GLint* values) {
  api()->glVertexAttribI4ivFn(indx, const_cast<const GLint*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4ui(GLuint indx,
                                                             GLuint x,
                                                             GLuint y,
                                                             GLuint z,
                                                             GLuint w) {
  api()->glVertexAttribI4uiFn(indx, x, y, z, w);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribI4uiv(
    GLuint indx,
    const volatile GLuint* values) {
  api()->glVertexAttribI4uivFn(indx, const_cast<const GLuint*>(values));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribIPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLsizei stride,
    const void* ptr) {
  api()->glVertexAttribIPointerFn(indx, size, type, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribPointer(
    GLuint indx,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void* ptr) {
  api()->glVertexAttribPointerFn(indx, size, type, normalized, stride, ptr);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoViewport(GLint x,
                                                     GLint y,
                                                     GLsizei width,
                                                     GLsizei height) {
  api()->glViewportFn(x, y, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitSync(GLuint sync,
                                                     GLbitfield flags,
                                                     GLuint64 timeout) {
  api()->glWaitSyncFn(GetSyncServiceID(sync, resources_), flags, timeout);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlitFramebufferCHROMIUM(
    GLint srcX0,
    GLint srcY0,
    GLint srcX1,
    GLint srcY1,
    GLint dstX0,
    GLint dstY0,
    GLint dstX1,
    GLint dstY1,
    GLbitfield mask,
    GLenum filter) {
  DCHECK(feature_info_->feature_flags().chromium_framebuffer_multisample);
  api()->glBlitFramebufferFn(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                             dstY1, mask, filter);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleCHROMIUM(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  DCHECK(feature_info_->feature_flags().chromium_framebuffer_multisample);
  api()->glRenderbufferStorageMultisampleFn(target, samples, internalformat,
                                            width, height);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleAdvancedAMD(
    GLenum target,
    GLsizei samples,
    GLsizei storageSamples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  DCHECK(feature_info_->feature_flags().amd_framebuffer_multisample_advanced);
  api()->glRenderbufferStorageMultisampleAdvancedAMDFn(
      target, samples, storageSamples, internalformat, width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRenderbufferStorageMultisampleEXT(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height) {
  DCHECK(feature_info_->feature_flags().multisampled_render_to_texture);
  api()->glRenderbufferStorageMultisampleEXTFn(target, samples, internalformat,
                                               width, height);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFramebufferTexture2DMultisampleEXT(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples) {
  DCHECK(feature_info_->feature_flags().multisampled_render_to_texture);
  if (IsEmulatedFramebufferBound(target)) {
    InsertError(GL_INVALID_OPERATION,
                "Cannot change the attachments of the default framebuffer.");
    return error::kNoError;
  }
  api()->glFramebufferTexture2DMultisampleEXTFn(
      target, attachment, textarget,
      GetTextureServiceID(api(), texture, resources_, false), level, samples);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTexStorage2DEXT(
    GLenum target,
    GLsizei levels,
    GLenum internalFormat,
    GLsizei width,
    GLsizei height) {
  CheckErrorCallbackState();
  api()->glTexStorage2DEXTFn(target, levels, internalFormat, width, height);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }
  UpdateTextureSizeFromTarget(target);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenQueriesEXT(
    GLsizei n,
    volatile GLuint* queries) {
  return GenHelper(
      n, queries, &query_id_map_, [this](GLsizei n, GLuint* queries) {
        if (feature_info_->feature_flags().occlusion_query_boolean) {
          // glGenQueries is not loaded unless GL_EXT_occlusion_query_boolean is
          // present. All queries must be emulated so they don't need to be
          // generated.
          api()->glGenQueriesFn(n, queries);
        } else {
          for (GLsizei i = 0; i < n; i++) {
            queries[i] = 0;
          }
        }
      });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteQueriesEXT(
    GLsizei n,
    const volatile GLuint* queries) {
  // Validate n is non-negative before allcoating a vector of size n
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }

  std::vector<GLuint> queries_copy(queries, queries + n);
  // If any of these queries are pending or active, remove them from the lists
  for (GLuint query_client_id : queries_copy) {
    GLuint query_service_id = 0;
    if (!query_id_map_.GetServiceID(query_client_id, &query_service_id) ||
        query_service_id == 0) {
      continue;
    }

    QueryInfo query_info = query_info_map_[query_service_id];
    query_info_map_.erase(query_service_id);

    if (query_info.type == GL_NONE) {
      // Query was never started
      continue;
    }

    if (base::Contains(active_queries_, query_info.type)) {
      active_queries_.erase(query_info.type);
    }

    RemovePendingQuery(query_service_id);
  }
  return DeleteHelper(
      queries_copy.size(), queries_copy.data(), &query_id_map_,
      [this](GLsizei n, GLuint* queries) {
        if (feature_info_->feature_flags().occlusion_query_boolean) {
          // glDeleteQueries is not loaded unless GL_EXT_occlusion_query_boolean
          // is present. All queries must be emulated so they don't need to be
          // deleted.
          api()->glDeleteQueriesFn(n, queries);
        }
      });
}

error::Error GLES2DecoderPassthroughImpl::DoQueryCounterEXT(
    GLuint id,
    GLenum target,
    int32_t sync_shm_id,
    uint32_t sync_shm_offset,
    uint32_t submit_count) {
  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  GLuint service_id = GetQueryServiceID(id, &query_id_map_);

  if (IsEmulatedQueryTarget(target)) {
    DCHECK_EQ(target,
              static_cast<GLenum>(GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM));
  } else {
    // glQueryCounter is not loaded unless GL_EXT_disjoint_timer_query is present
    if (!feature_info_->feature_flags().ext_disjoint_timer_query) {
      InsertError(GL_INVALID_ENUM, "Invalid query target.");
      return error::kNoError;
    }

    // Flush all previous errors
    CheckErrorCallbackState();

    api()->glQueryCounterFn(service_id, target);

    // Check if a new error was generated
    if (CheckErrorCallbackState()) {
      return error::kNoError;
    }
  }

  QueryInfo* query_info = &query_info_map_[service_id];
  query_info->type = target;

  // Make sure to stop tracking this query if it was still pending a result from
  // a previous glEndQuery
  RemovePendingQuery(service_id);

  PendingQuery pending_query;
  pending_query.target = target;
  pending_query.service_id = service_id;
  pending_query.shm = std::move(buffer);
  pending_query.sync = sync;
  pending_query.submit_count = submit_count;
  if (target == GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM)
    pending_query.commands_issued_timestamp = base::TimeTicks::Now();
  pending_queries_.push_back(std::move(pending_query));

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginQueryEXT(
    GLenum target,
    GLuint id,
    int32_t sync_shm_id,
    uint32_t sync_shm_offset) {
  GLuint service_id = GetQueryServiceID(id, &query_id_map_);
  QueryInfo* query_info = &query_info_map_[service_id];

  scoped_refptr<gpu::Buffer> buffer = GetSharedMemoryBuffer(sync_shm_id);
  if (!buffer)
    return error::kInvalidArguments;
  QuerySync* sync = static_cast<QuerySync*>(
      buffer->GetDataAddress(sync_shm_offset, sizeof(QuerySync)));
  if (!sync)
    return error::kOutOfBounds;

  if (target == GL_PROGRAM_COMPLETION_QUERY_CHROMIUM) {
    linking_program_service_id_ = 0u;
  }
  if (IsEmulatedQueryTarget(target)) {
    if (base::Contains(active_queries_, target)) {
      InsertError(GL_INVALID_OPERATION, "Query already active on target.");
      return error::kNoError;
    }

    if (id == 0) {
      InsertError(GL_INVALID_OPERATION, "Query id is 0.");
      return error::kNoError;
    }

    if (query_info->type != GL_NONE && query_info->type != target) {
      InsertError(GL_INVALID_OPERATION,
                  base::StringPrintf(
                      "Query type (%s) does not match the target (%s).",
                      gl::GLEnums::GetStringEnum(query_info->type).c_str(),
                      gl::GLEnums::GetStringEnum(target).c_str()));
      return error::kNoError;
    }
  } else {
    // glBeginQuery is not loaded unless GL_EXT_occlusion_query_boolean is
    // present
    if (!feature_info_->feature_flags().occlusion_query_boolean) {
      InsertError(GL_INVALID_ENUM, "Invalid query target.");
      return error::kNoError;
    }

    // Flush all previous errors
    CheckErrorCallbackState();

    api()->glBeginQueryFn(target, service_id);

    // Check if a new error was generated
    if (CheckErrorCallbackState()) {
      return error::kNoError;
    }
  }

  query_info->type = target;

  // Make sure to stop tracking this query if it was still pending a result from
  // a previous glEndQuery
  RemovePendingQuery(service_id);

  ActiveQuery query;
  query.service_id = service_id;
  query.shm = std::move(buffer);
  query.sync = sync;
  if (target == GL_COMMANDS_ISSUED_CHROMIUM)
    query.command_processing_start_time = base::TimeTicks::Now();
  active_queries_[target] = std::move(query);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginTransformFeedback(
    GLenum primitivemode) {
  api()->glBeginTransformFeedbackFn(primitivemode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndQueryEXT(GLenum target,
                                                        uint32_t submit_count) {
  if (IsEmulatedQueryTarget(target)) {
    auto active_query_iter = active_queries_.find(target);
    if (active_query_iter == active_queries_.end()) {
      InsertError(GL_INVALID_OPERATION, "No active query on target.");
      return error::kNoError;
    }
    if (target == GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM &&
        !pending_read_pixels_.empty()) {
      GLuint query_service_id = active_query_iter->second.service_id;
      pending_read_pixels_.back().waiting_async_pack_queries.insert(
          query_service_id);
    }
  } else {
    // glEndQuery is not loaded unless GL_EXT_occlusion_query_boolean is present
    if (!feature_info_->feature_flags().occlusion_query_boolean) {
      InsertError(GL_INVALID_ENUM, "Invalid query target.");
      return error::kNoError;
    }

    // Flush all previous errors
    CheckErrorCallbackState();

    api()->glEndQueryFn(target);

    // Check if a new error was generated
    if (CheckErrorCallbackState()) {
      return error::kNoError;
    }
  }

  DCHECK(active_queries_.find(target) != active_queries_.end());
  ActiveQuery active_query = std::move(active_queries_[target]);
  active_queries_.erase(target);

  PendingQuery pending_query;
  pending_query.target = target;
  pending_query.service_id = active_query.service_id;
  pending_query.shm = std::move(active_query.shm);
  pending_query.sync = active_query.sync;
  pending_query.submit_count = submit_count;
  switch (target) {
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      pending_query.commands_completed_fence = gl::GLFence::Create();
      if (!pending_query.commands_completed_fence) {
        InsertError(GL_INVALID_OPERATION, "gl::GLFence::Create() failed.");
        MarkContextLost(error::kUnknown);
        group_->LoseContexts(error::kUnknown);
        return error::kLostContext;
      }
      break;

    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
      pending_query.buffer_shadow_update_fence = gl::GLFence::Create();
      if (!pending_query.buffer_shadow_update_fence) {
        InsertError(GL_INVALID_OPERATION, "gl::GLFence::Create() failed.");
        MarkContextLost(error::kUnknown);
        group_->LoseContexts(error::kUnknown);
        return error::kLostContext;
      }
      pending_query.buffer_shadow_updates = std::move(buffer_shadow_updates_);
      buffer_shadow_updates_.clear();
      break;

    case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
      pending_query.program_service_id = linking_program_service_id_;
      break;

    case GL_COMMANDS_ISSUED_CHROMIUM:
      pending_query.commands_issued_time =
          active_query.active_time +
          (base::TimeTicks::Now() - active_query.command_processing_start_time);
      break;

    default:
      break;
  }
  pending_queries_.push_back(std::move(pending_query));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndTransformFeedback() {
  api()->glEndTransformFeedbackFn();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSetDisjointValueSyncCHROMIUM(
    DisjointValueSync* sync) {
  GLint disjoint = 0;
  api()->glGetIntegervFn(GL_GPU_DISJOINT_EXT, &disjoint);
  if (disjoint > 0) {
    sync->SetDisjointCount(disjoint);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoInsertEventMarkerEXT(
    GLsizei length,
    const char* marker) {
  api()->glInsertEventMarkerEXTFn(length, marker);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPushGroupMarkerEXT(
    GLsizei length,
    const char* marker) {
  api()->glPushGroupMarkerEXTFn(length, marker);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPopGroupMarkerEXT() {
  api()->glPopGroupMarkerEXTFn();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGenVertexArraysOES(
    GLsizei n,
    volatile GLuint* arrays) {
  return GenHelper(n, arrays, &vertex_array_id_map_,
                   [this](GLsizei n, GLuint* arrays) {
                     api()->glGenVertexArraysOESFn(n, arrays);
                   });
}

error::Error GLES2DecoderPassthroughImpl::DoDeleteVertexArraysOES(
    GLsizei n,
    const volatile GLuint* arrays) {
  return DeleteHelper(n, arrays, &vertex_array_id_map_,
                      [this](GLsizei n, GLuint* arrays) {
                        api()->glDeleteVertexArraysOESFn(n, arrays);
                      });
}

error::Error GLES2DecoderPassthroughImpl::DoIsVertexArrayOES(GLuint array,
                                                             uint32_t* result) {
  *result = api()->glIsVertexArrayOESFn(
      GetVertexArrayServiceID(array, &vertex_array_id_map_));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindVertexArrayOES(GLuint array) {
  api()->glBindVertexArrayOESFn(
      GetVertexArrayServiceID(array, &vertex_array_id_map_));
  bound_element_array_buffer_dirty_ = true;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoSwapBuffers(uint64_t swap_id,
                                                        GLbitfield flags) {
  if (offscreen_) {
    // We don't support SwapBuffers on the offscreen contexts.
    LOG(ERROR) << "SwapBuffers called for the offscreen context";
    return error::kUnknownCommand;
  }

  client()->OnSwapBuffers(swap_id, flags);
  if (surface_->SupportsAsyncSwap()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "gpu", "AsyncSwapBuffers",
        TRACE_ID_WITH_SCOPE("AsyncSwapBuffers", swap_id));
    surface_->SwapBuffersAsync(
        base::BindOnce(
            &GLES2DecoderPassthroughImpl::CheckSwapBuffersAsyncResult,
            weak_ptr_factory_.GetWeakPtr(), "SwapBuffers", swap_id),
        base::DoNothing(), gfx::FrameData());
    return error::kNoError;
  } else {
    return CheckSwapBuffersResult(
        surface_->SwapBuffers(base::DoNothing(), gfx::FrameData()),
        "SwapBuffers");
  }
}

error::Error GLES2DecoderPassthroughImpl::DoGetMaxValueInBufferCHROMIUM(
    GLuint buffer_id,
    GLsizei count,
    GLenum type,
    GLuint offset,
    uint32_t* result) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableFeatureCHROMIUM(
    const char* feature) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMapBufferRange(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    GLbitfield access,
    void* ptr,
    int32_t data_shm_id,
    uint32_t data_shm_offset,
    uint32_t* result) {
  CheckErrorCallbackState();

  GLbitfield filtered_access = access;

  // Always filter out GL_MAP_UNSYNCHRONIZED_BIT to get rid of undefined
  // behaviors.
  filtered_access = (filtered_access & ~GL_MAP_UNSYNCHRONIZED_BIT);

  if ((filtered_access & GL_MAP_INVALIDATE_BUFFER_BIT) != 0) {
    // To be on the safe side, always map GL_MAP_INVALIDATE_BUFFER_BIT to
    // GL_MAP_INVALIDATE_RANGE_BIT.
    filtered_access = (filtered_access & ~GL_MAP_INVALIDATE_BUFFER_BIT);
    filtered_access = (filtered_access | GL_MAP_INVALIDATE_RANGE_BIT);
  }
  if ((filtered_access & GL_MAP_INVALIDATE_RANGE_BIT) == 0) {
    // If this user intends to use this buffer without invalidating the data, we
    // need to also add GL_MAP_READ_BIT to preserve the original data when
    // copying it to shared memory.
    filtered_access = (filtered_access | GL_MAP_READ_BIT);
  }

  void* mapped_ptr =
      api()->glMapBufferRangeFn(target, offset, size, filtered_access);
  if (CheckErrorCallbackState() || mapped_ptr == nullptr) {
    // Had an error while mapping, don't copy any data
    *result = 0;
    return error::kNoError;
  }

  if ((filtered_access & GL_MAP_INVALIDATE_RANGE_BIT) == 0) {
    memcpy(ptr, mapped_ptr, size);
  }

  // Track the mapping of this buffer so that data can be synchronized when it
  // is unmapped
  DCHECK(bound_buffers_.find(target) != bound_buffers_.end());
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    LazilyUpdateCurrentlyBoundElementArrayBuffer();
  }
  GLuint client_buffer = bound_buffers_.at(target);

  MappedBuffer mapped_buffer_info;
  mapped_buffer_info.size = size;
  mapped_buffer_info.original_access = access;
  mapped_buffer_info.filtered_access = filtered_access;
  mapped_buffer_info.map_ptr = static_cast<uint8_t*>(mapped_ptr);
  mapped_buffer_info.data_shm_id = data_shm_id;
  mapped_buffer_info.data_shm_offset = data_shm_offset;

  DCHECK(resources_->mapped_buffer_map.find(client_buffer) ==
         resources_->mapped_buffer_map.end());
  resources_->mapped_buffer_map.insert(
      std::make_pair(client_buffer, mapped_buffer_info));

  *result = 1;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUnmapBuffer(GLenum target) {
  if (target == GL_ELEMENT_ARRAY_BUFFER) {
    LazilyUpdateCurrentlyBoundElementArrayBuffer();
  }
  auto bound_buffers_iter = bound_buffers_.find(target);
  if (bound_buffers_iter == bound_buffers_.end()) {
    InsertError(GL_INVALID_ENUM, "Invalid buffer target.");
    return error::kNoError;
  }

  if (bound_buffers_iter->second == 0) {
    InsertError(GL_INVALID_OPERATION, "No buffer bound to this target.");
    return error::kNoError;
  }

  GLuint client_buffer = bound_buffers_iter->second;
  auto mapped_buffer_info_iter =
      resources_->mapped_buffer_map.find(client_buffer);
  if (mapped_buffer_info_iter == resources_->mapped_buffer_map.end()) {
    InsertError(GL_INVALID_OPERATION, "Buffer is not mapped.");
    return error::kNoError;
  }

  const MappedBuffer& map_info = mapped_buffer_info_iter->second;
  if ((map_info.filtered_access & GL_MAP_WRITE_BIT) != 0 &&
      (map_info.filtered_access & GL_MAP_FLUSH_EXPLICIT_BIT) == 0) {
    uint8_t* mem = GetSharedMemoryAs<uint8_t*>(
        map_info.data_shm_id, map_info.data_shm_offset, map_info.size);
    if (!mem) {
      return error::kOutOfBounds;
    }

    memcpy(map_info.map_ptr, mem, map_info.size);
  }

  api()->glUnmapBufferFn(target);

  resources_->mapped_buffer_map.erase(mapped_buffer_info_iter);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoResizeCHROMIUM(
    GLuint width,
    GLuint height,
    GLfloat scale_factor,
    gfx::ColorSpace color_space,
    GLboolean alpha) {
  // gfx::Size uses integers, make sure width and height do not overflow
  static_assert(sizeof(GLuint) >= sizeof(int), "Unexpected GLuint size.");
  static const GLuint kMaxDimension =
      static_cast<GLuint>(std::numeric_limits<int>::max());
  gfx::Size safe_size(std::clamp(width, 1U, kMaxDimension),
                      std::clamp(height, 1U, kMaxDimension));
  if (offscreen_) {
    // We don't support resize of offscreen contexts.
    LOG(ERROR) << "Resize called for the offscreen context";
    return error::kUnknownCommand;
  } else {
    if (!surface_->Resize(safe_size, scale_factor, color_space, !!alpha)) {
      LOG(ERROR)
          << "GLES2DecoderPassthroughImpl: Context lost because resize failed.";
      return error::kLostContext;
    }
    DCHECK(context_->IsCurrent(surface_.get()));
    if (!context_->IsCurrent(surface_.get())) {
      LOG(ERROR) << "GLES2DecoderPassthroughImpl: Context lost because context "
                    "no longer current after resize callback.";
      return error::kLostContext;
    }
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetRequestableExtensionsCHROMIUM(
    const char** extensions) {
  *extensions = reinterpret_cast<const char*>(
      api()->glGetStringFn(GL_REQUESTABLE_EXTENSIONS_ANGLE));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoRequestExtensionCHROMIUM(
    const char* extension) {
  api()->glRequestExtensionANGLEFn(extension);

  // Make sure there are no pending GL errors before re-initializing feature
  // info
  FlushErrors();

  // Make sure newly enabled extensions are exposed and usable.
  context_->ReinitializeDynamicBindings();
  InitializeFeatureInfo(feature_info_->context_type(),
                        feature_info_->disallowed_features(), true);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetProgramInfoCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_attributes = 0;
  api()->glGetProgramivFn(service_program, GL_ACTIVE_ATTRIBUTES,
                          &num_attributes);

  GLint num_uniforms = 0;
  api()->glGetProgramivFn(service_program, GL_ACTIVE_UNIFORMS, &num_uniforms);

  const base::CheckedNumeric<size_t> buffer_header_size(
      sizeof(ProgramInfoHeader));
  const base::CheckedNumeric<size_t> buffer_block_size(sizeof(ProgramInput));
  const base::CheckedNumeric<size_t> attribute_block_size =
      buffer_block_size * num_attributes;
  const base::CheckedNumeric<size_t> uniform_block_size =
      buffer_block_size * num_uniforms;
  data->resize((buffer_header_size + attribute_block_size + uniform_block_size)
                   .ValueOrDie(),
               0);

  GLint link_status = 0;
  api()->glGetProgramivFn(service_program, GL_LINK_STATUS, &link_status);

  ProgramInfoHeader header;
  header.link_status = link_status;
  header.num_attribs = num_attributes;
  header.num_uniforms = num_uniforms;
  InsertValueIntoBuffer(data, header, 0);

  GLint active_attribute_max_length = 0;
  api()->glGetProgramivFn(service_program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
                          &active_attribute_max_length);

  std::vector<char> attrib_name_buf(active_attribute_max_length, 0);
  for (GLint attrib_index = 0; attrib_index < num_attributes; attrib_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    api()->glGetActiveAttribFn(service_program, attrib_index,
                               attrib_name_buf.size(), &length, &size, &type,
                               attrib_name_buf.data());

    ProgramInput input;
    input.size = size;
    input.type = type;

    int32_t location =
        api()->glGetAttribLocationFn(service_program, attrib_name_buf.data());
    input.location_offset = data->size();
    AppendValueToBuffer(data, location);

    input.name_offset = data->size();
    input.name_length = length;
    AppendStringToBuffer(data, attrib_name_buf.data(), length);

    InsertValueIntoBuffer(
        data, input,
        (buffer_header_size + (buffer_block_size * attrib_index)).ValueOrDie());
  }

  GLint active_uniform_max_length = 0;
  api()->glGetProgramivFn(service_program, GL_ACTIVE_UNIFORM_MAX_LENGTH,
                          &active_uniform_max_length);

  std::vector<char> uniform_name_buf(active_uniform_max_length, 0);
  for (GLint uniform_index = 0; uniform_index < num_uniforms; uniform_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    api()->glGetActiveUniformFn(service_program, uniform_index,
                                uniform_name_buf.size(), &length, &size, &type,
                                uniform_name_buf.data());

    ProgramInput input;
    input.size = size;
    input.type = type;

    input.location_offset = data->size();
    int32_t base_location =
        api()->glGetUniformLocationFn(service_program, uniform_name_buf.data());
    AppendValueToBuffer(data, base_location);

    GLSLArrayName parsed_service_name(uniform_name_buf.data());
    if (size > 1 || parsed_service_name.IsArrayName()) {
      for (GLint location_index = 1; location_index < size; location_index++) {
        std::string array_element_name = parsed_service_name.base_name() + "[" +
                                         base::NumberToString(location_index) +
                                         "]";
        int32_t element_location = api()->glGetUniformLocationFn(
            service_program, array_element_name.c_str());
        AppendValueToBuffer(data, element_location);
      }
    }

    input.name_offset = data->size();
    input.name_length = length;
    AppendStringToBuffer(data, uniform_name_buf.data(), length);

    InsertValueIntoBuffer(data, input,
                          (buffer_header_size + attribute_block_size +
                           (buffer_block_size * uniform_index))
                              .ValueOrDie());
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformBlocksCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_uniform_blocks = 0;
  api()->glGetProgramivFn(service_program, GL_ACTIVE_UNIFORM_BLOCKS,
                          &num_uniform_blocks);

  // Resize the data to fit the headers and info objects so that strings can be
  // appended.
  const base::CheckedNumeric<size_t> buffer_header_size(
      sizeof(UniformBlocksHeader));
  const base::CheckedNumeric<size_t> buffer_block_size(
      sizeof(UniformBlockInfo));
  data->resize((buffer_header_size + (num_uniform_blocks * buffer_block_size))
                   .ValueOrDie(),
               0);

  UniformBlocksHeader header;
  header.num_uniform_blocks = num_uniform_blocks;
  InsertValueIntoBuffer(data, header, 0);

  GLint active_uniform_block_max_length = 0;
  api()->glGetProgramivFn(service_program,
                          GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH,
                          &active_uniform_block_max_length);

  std::vector<char> uniform_block_name_buf(active_uniform_block_max_length, 0);
  for (GLint uniform_block_index = 0; uniform_block_index < num_uniform_blocks;
       uniform_block_index++) {
    UniformBlockInfo block_info;

    GLint uniform_block_binding = 0;
    api()->glGetActiveUniformBlockivFn(service_program, uniform_block_index,
                                       GL_UNIFORM_BLOCK_BINDING,
                                       &uniform_block_binding);
    block_info.binding = uniform_block_binding;

    GLint uniform_block_data_size = 0;
    api()->glGetActiveUniformBlockivFn(service_program, uniform_block_index,
                                       GL_UNIFORM_BLOCK_DATA_SIZE,
                                       &uniform_block_data_size);
    block_info.data_size = uniform_block_data_size;

    GLint uniform_block_name_length = 0;
    api()->glGetActiveUniformBlockNameFn(
        service_program, uniform_block_index, active_uniform_block_max_length,
        &uniform_block_name_length, uniform_block_name_buf.data());

    DCHECK(uniform_block_name_length + 1 <= active_uniform_block_max_length);
    block_info.name_offset = data->size();
    block_info.name_length = uniform_block_name_length + 1;
    AppendStringToBuffer(data, uniform_block_name_buf.data(),
                         uniform_block_name_length + 1);

    GLint uniform_block_active_uniforms = 0;
    api()->glGetActiveUniformBlockivFn(service_program, uniform_block_index,
                                       GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                                       &uniform_block_active_uniforms);
    block_info.active_uniforms = uniform_block_active_uniforms;

    std::vector<GLint> uniform_block_indices_buf(uniform_block_active_uniforms,
                                                 0);
    api()->glGetActiveUniformBlockivFn(service_program, uniform_block_index,
                                       GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                                       uniform_block_indices_buf.data());
    block_info.active_uniform_offset = data->size();
    for (GLint uniform_block_uniform_index_index = 0;
         uniform_block_uniform_index_index < uniform_block_active_uniforms;
         uniform_block_uniform_index_index++) {
      AppendValueToBuffer(
          data,
          static_cast<uint32_t>(
              uniform_block_indices_buf[uniform_block_uniform_index_index]));
    }

    GLint uniform_block_referenced_by_vertex_shader = 0;
    api()->glGetActiveUniformBlockivFn(
        service_program, uniform_block_index,
        GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER,
        &uniform_block_referenced_by_vertex_shader);
    block_info.referenced_by_vertex_shader =
        uniform_block_referenced_by_vertex_shader;

    GLint uniform_block_referenced_by_fragment_shader = 0;
    api()->glGetActiveUniformBlockivFn(
        service_program, uniform_block_index,
        GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER,
        &uniform_block_referenced_by_fragment_shader);
    block_info.referenced_by_fragment_shader =
        uniform_block_referenced_by_fragment_shader;

    InsertValueIntoBuffer(
        data, block_info,
        (buffer_header_size + (buffer_block_size * uniform_block_index))
            .ValueOrDie());
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoGetTransformFeedbackVaryingsCHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint transform_feedback_buffer_mode = 0;
  api()->glGetProgramivFn(service_program, GL_TRANSFORM_FEEDBACK_BUFFER_MODE,
                          &transform_feedback_buffer_mode);

  GLint num_transform_feedback_varyings = 0;
  api()->glGetProgramivFn(service_program, GL_TRANSFORM_FEEDBACK_VARYINGS,
                          &num_transform_feedback_varyings);

  // Resize the data to fit the headers and info objects so that strings can be
  // appended.
  const base::CheckedNumeric<size_t> buffer_header_size(
      sizeof(TransformFeedbackVaryingsHeader));
  const base::CheckedNumeric<size_t> buffer_block_size(
      sizeof(TransformFeedbackVaryingInfo));
  data->resize((buffer_header_size +
                (num_transform_feedback_varyings * buffer_block_size))
                   .ValueOrDie(),
               0);

  TransformFeedbackVaryingsHeader header;
  header.transform_feedback_buffer_mode = transform_feedback_buffer_mode;
  header.num_transform_feedback_varyings = num_transform_feedback_varyings;
  InsertValueIntoBuffer(data, header, 0);

  GLint max_transform_feedback_varying_length = 0;
  api()->glGetProgramivFn(service_program,
                          GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                          &max_transform_feedback_varying_length);

  std::vector<char> transform_feedback_varying_name_buf(
      max_transform_feedback_varying_length, 0);
  for (GLint transform_feedback_varying_index = 0;
       transform_feedback_varying_index < num_transform_feedback_varyings;
       transform_feedback_varying_index++) {
    GLsizei length = 0;
    GLint size = 0;
    GLenum type = GL_NONE;
    api()->glGetTransformFeedbackVaryingFn(
        service_program, transform_feedback_varying_index,
        max_transform_feedback_varying_length, &length, &size, &type,
        transform_feedback_varying_name_buf.data());

    TransformFeedbackVaryingInfo varying_info;
    varying_info.size = size;
    varying_info.type = type;

    DCHECK(length + 1 <= max_transform_feedback_varying_length);
    varying_info.name_length = data->size();
    varying_info.name_length = length + 1;
    AppendStringToBuffer(data, transform_feedback_varying_name_buf.data(),
                         length + 1);

    InsertValueIntoBuffer(
        data, varying_info,
        (buffer_header_size +
         (buffer_block_size * transform_feedback_varying_index))
            .ValueOrDie());
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetUniformsES3CHROMIUM(
    GLuint program,
    std::vector<uint8_t>* data) {
  GLuint service_program = 0;
  if (!resources_->program_id_map.GetServiceID(program, &service_program)) {
    return error::kNoError;
  }

  GLint num_uniforms = 0;
  api()->glGetProgramivFn(service_program, GL_ACTIVE_UNIFORMS, &num_uniforms);

  UniformsES3Header header;
  header.num_uniforms = num_uniforms;
  AppendValueToBuffer(data, header);

  for (GLuint uniform_index = 0;
       uniform_index < static_cast<GLuint>(num_uniforms); uniform_index++) {
    UniformES3Info uniform_info;

    GLint uniform_block_index = 0;
    api()->glGetActiveUniformsivFn(service_program, 1, &uniform_index,
                                   GL_UNIFORM_BLOCK_INDEX,
                                   &uniform_block_index);
    uniform_info.block_index = uniform_block_index;

    GLint uniform_offset = 0;
    api()->glGetActiveUniformsivFn(service_program, 1, &uniform_index,
                                   GL_UNIFORM_OFFSET, &uniform_offset);
    uniform_info.offset = uniform_offset;

    GLint uniform_array_stride = 0;
    api()->glGetActiveUniformsivFn(service_program, 1, &uniform_index,
                                   GL_UNIFORM_ARRAY_STRIDE,
                                   &uniform_array_stride);
    uniform_info.array_stride = uniform_array_stride;

    GLint uniform_matrix_stride = 0;
    api()->glGetActiveUniformsivFn(service_program, 1, &uniform_index,
                                   GL_UNIFORM_MATRIX_STRIDE,
                                   &uniform_matrix_stride);
    uniform_info.matrix_stride = uniform_matrix_stride;

    GLint uniform_is_row_major = 0;
    api()->glGetActiveUniformsivFn(service_program, 1, &uniform_index,
                                   GL_UNIFORM_IS_ROW_MAJOR,
                                   &uniform_is_row_major);
    uniform_info.is_row_major = uniform_is_row_major;

    AppendValueToBuffer(data, uniform_info);
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetTranslatedShaderSourceANGLE(
    GLuint shader,
    std::string* source) {
  CheckErrorCallbackState();
  GLuint service_id = GetShaderServiceID(shader, resources_);
  GLint translated_source_length = 0;
  api()->glGetShaderivFn(service_id, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE,
                         &translated_source_length);
  if (CheckErrorCallbackState()) {
    return error::kNoError;
  }

  if (translated_source_length > 0) {
    std::vector<char> buffer(translated_source_length, 0);
    GLsizei length = 0;
    api()->glGetTranslatedShaderSourceANGLEFn(
        service_id, translated_source_length, &length, buffer.data());
    DCHECK(length <= translated_source_length);
    *source = length > 0 ? std::string(buffer.data(), length) : std::string();
  } else {
    *source = std::string();
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopyTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint internalformat,
    GLenum dest_type,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  gl::ScopedEnableTextureRectangleInShaderCompiler enable(
      feature_info_->IsWebGLContext() ? api() : nullptr);
  api()->glCopyTextureCHROMIUMFn(
      GetTextureServiceID(api(), source_id, resources_, false), source_level,
      dest_target, GetTextureServiceID(api(), dest_id, resources_, false),
      dest_level, internalformat, dest_type, unpack_flip_y,
      unpack_premultiply_alpha, unpack_unmultiply_alpha);

  UpdateTextureSizeFromClientID(dest_id);

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopySubTextureCHROMIUM(
    GLuint source_id,
    GLint source_level,
    GLenum dest_target,
    GLuint dest_id,
    GLint dest_level,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha,
    GLboolean unpack_unmultiply_alpha) {
  gl::ScopedEnableTextureRectangleInShaderCompiler enable(
      feature_info_->IsWebGLContext() ? api() : nullptr);
  api()->glCopySubTextureCHROMIUMFn(
      GetTextureServiceID(api(), source_id, resources_, false), source_level,
      dest_target, GetTextureServiceID(api(), dest_id, resources_, false),
      dest_level, xoffset, yoffset, x, y, width, height, unpack_flip_y,
      unpack_premultiply_alpha, unpack_unmultiply_alpha);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawArraysInstancedANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount) {
  api()->glDrawArraysInstancedANGLEFn(mode, first, count, primcount);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoDrawArraysInstancedBaseInstanceANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance) {
  api()->glDrawArraysInstancedBaseInstanceANGLEFn(mode, first, count, primcount,
                                                  baseinstance);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawElementsInstancedANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount) {
  api()->glDrawElementsInstancedANGLEFn(mode, count, type, indices, primcount);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoDrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint basevertex,
    GLuint baseinstance) {
  api()->glDrawElementsInstancedBaseVertexBaseInstanceANGLEFn(
      mode, count, type, indices, primcount, basevertex, baseinstance);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoVertexAttribDivisorANGLE(
    GLuint index,
    GLuint divisor) {
  api()->glVertexAttribDivisorANGLEFn(index, divisor);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindUniformLocationCHROMIUM(
    GLuint program,
    GLint location,
    const char* name) {
  api()->glBindUniformLocationCHROMIUMFn(
      GetProgramServiceID(program, resources_), location, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceBeginCHROMIUM(
    const char* category_name,
    const char* trace_name) {
  if (!gpu_tracer_->Begin(category_name, trace_name, kTraceCHROMIUM)) {
    InsertError(GL_INVALID_OPERATION, "Failed to create begin trace");
    return error::kNoError;
  }
  debug_marker_manager_.PushGroup(trace_name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoTraceEndCHROMIUM() {
  if (!gpu_tracer_->End(kTraceCHROMIUM)) {
    InsertError(GL_INVALID_OPERATION, "No trace to end");
    return error::kNoError;
  }
  debug_marker_manager_.PopGroup();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDiscardFramebufferEXT(
    GLenum target,
    GLsizei count,
    const volatile GLenum* attachments) {
  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }
  std::vector<GLenum> attachments_copy(attachments, attachments + count);

  if (feature_info_->gl_version_info().is_es3) {
    api()->glInvalidateFramebufferFn(target, count, attachments_copy.data());
  } else {
    api()->glDiscardFramebufferEXTFn(target, count, attachments_copy.data());
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLoseContextCHROMIUM(GLenum current,
                                                                GLenum other) {
  if (!ValidContextLostReason(current) || !ValidContextLostReason(other)) {
    InsertError(GL_INVALID_ENUM, "invalid context loss reason.");
    return error::kNoError;
  }

  MarkContextLost(GetContextLostReasonFromResetStatus(current));
  group_->LoseContexts(GetContextLostReasonFromResetStatus(other));
  reset_by_robustness_extension_ = true;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDescheduleUntilFinishedCHROMIUM() {
  if (!gl::GLFence::IsSupported()) {
    return error::kNoError;
  }

  auto fence = gl::GLFence::Create();
  if (!fence) {
    InsertError(GL_INVALID_OPERATION, "gl::GLFence::Create() failed.");
    MarkContextLost(error::kUnknown);
    group_->LoseContexts(error::kUnknown);
    return error::kLostContext;
  }

  deschedule_until_finished_fences_.push_back(std::move(fence));

  if (deschedule_until_finished_fences_.size() == 1) {
    return error::kNoError;
  }

  DCHECK_EQ(2u, deschedule_until_finished_fences_.size());
  if (deschedule_until_finished_fences_[0]->HasCompleted()) {
    deschedule_until_finished_fences_.pop_front();
    return error::kNoError;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "cc", "GLES2DecoderPassthroughImpl::DescheduleUntilFinished",
      TRACE_ID_LOCAL(this));
  client()->OnDescheduleUntilFinished();
  return error::kDeferLaterCommands;
}

error::Error GLES2DecoderPassthroughImpl::DoDrawBuffersEXT(
    GLsizei count,
    const volatile GLenum* bufs) {
  if (!feature_info_->feature_flags().ext_draw_buffers &&
      !feature_info_->gl_version_info().is_es3) {
    return error::kUnknownCommand;
  }

  // Validate that count is non-negative before allocating a vector
  if (count < 0) {
    InsertError(GL_INVALID_VALUE, "count cannot be negative.");
    return error::kNoError;
  }
  std::vector<GLenum> bufs_copy(bufs, bufs + count);
  api()->glDrawBuffersARBFn(count, bufs_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoFlushDriverCachesCHROMIUM() {
  // On Adreno Android devices we need to use a workaround to force caches to
  // clear.
  if (feature_info_->workarounds().unbind_egl_context_to_flush_driver_caches) {
    context_->ReleaseCurrent(nullptr);
    context_->MakeCurrent(surface_.get());
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCoverageModulationCHROMIUM(
    GLenum components) {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBlendBarrierKHR() {
  NOTIMPLEMENTED();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationIndexedEXT(
    GLuint program,
    GLuint colorNumber,
    GLuint index,
    const char* name) {
  api()->glBindFragDataLocationIndexedFn(
      GetProgramServiceID(program, resources_), colorNumber, index, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBindFragDataLocationEXT(
    GLuint program,
    GLuint colorNumber,
    const char* name) {
  api()->glBindFragDataLocationFn(GetProgramServiceID(program, resources_),
                                  colorNumber, name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoGetFragDataIndexEXT(
    GLuint program,
    const char* name,
    GLint* index) {
  *index = api()->glGetFragDataIndexFn(GetProgramServiceID(program, resources_),
                                       name);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWindowRectanglesEXT(
    GLenum mode,
    GLsizei n,
    const volatile GLint* box) {
  std::vector<GLint> box_copy(box, box + (n * 4));
  api()->glWindowRectanglesEXTFn(mode, n, box_copy.data());
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCreateGpuFenceINTERNAL(
    GLuint gpu_fence_id) {
  if (!feature_info_->feature_flags().chromium_gpu_fence)
    return error::kUnknownCommand;
  if (!GetGpuFenceManager()->CreateGpuFence(gpu_fence_id))
    return error::kInvalidArguments;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoWaitGpuFenceCHROMIUM(
    GLuint gpu_fence_id) {
  if (!feature_info_->feature_flags().chromium_gpu_fence)
    return error::kUnknownCommand;
  if (!GetGpuFenceManager()->GpuFenceServerWait(gpu_fence_id))
    return error::kInvalidArguments;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDestroyGpuFenceCHROMIUM(
    GLuint gpu_fence_id) {
  if (!feature_info_->feature_flags().chromium_gpu_fence)
    return error::kUnknownCommand;
  if (!GetGpuFenceManager()->RemoveGpuFence(gpu_fence_id))
    return error::kInvalidArguments;
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoSetReadbackBufferShadowAllocationINTERNAL(
    GLuint buffer_id,
    GLuint shm_id,
    GLuint shm_offset,
    GLuint size) {
  BufferShadowUpdate update;
  update.shm = GetSharedMemoryBuffer(shm_id);
  update.shm_offset = shm_offset;
  update.size = size;

  GLuint buffer_service_id = 0;
  if (!resources_->buffer_id_map.GetServiceID(buffer_id, &buffer_service_id)) {
    InsertError(GL_INVALID_OPERATION, "Invalid buffer ID");
    return error::kNoError;
  }

  if (!update.shm) {
    return error::kInvalidArguments;
  }
  if (update.shm->GetRemainingSize(shm_offset) < size) {
    return error::kOutOfBounds;
  }

  buffer_shadow_updates_.emplace(buffer_id, std::move(update));

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoMaxShaderCompilerThreadsKHR(
    GLuint count) {
  api()->glMaxShaderCompilerThreadsKHRFn(count);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoInitializeDiscardableTextureCHROMIUM(
    GLuint texture_id,
    ServiceDiscardableHandle&& discardable_handle) {
  scoped_refptr<TexturePassthrough> texture_passthrough;
  if (!resources_->texture_object_map.GetServiceID(texture_id,
                                                   &texture_passthrough) ||
      texture_passthrough == nullptr) {
    InsertError(GL_INVALID_VALUE, "Invalid texture ID");
    return error::kNoError;
  }

  group_->passthrough_discardable_manager()->InitializeTexture(
      texture_id, group_.get(), texture_passthrough->estimated_size(),
      std::move(discardable_handle));

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoLockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  if (!group_->passthrough_discardable_manager()->LockTexture(texture_id,
                                                              group_.get())) {
    InsertError(GL_INVALID_VALUE, "Texture ID not initialized");
    return error::kNoError;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoUnlockDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  TexturePassthrough* texture_to_unbind = nullptr;
  if (!group_->passthrough_discardable_manager()->UnlockTexture(
          texture_id, group_.get(), &texture_to_unbind)) {
    InsertError(GL_INVALID_VALUE, "Texture ID not initialized");
    return error::kNoError;
  }

  if (texture_to_unbind != nullptr) {
    UpdateTextureBinding(texture_to_unbind->target(), texture_id, nullptr);
  }
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoCreateAndTexStorage2DSharedImageINTERNAL(
    GLuint texture_client_id,
    const volatile GLbyte* mailbox) {
  if (!texture_client_id ||
      resources_->texture_id_map.HasClientID(texture_client_id)) {
    InsertError(GL_INVALID_OPERATION, "invalid client ID");
    return error::kNoError;
  }

  const Mailbox& mb = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox));
  auto shared_image = group_->shared_image_representation_factory()
                          ->ProduceGLTexturePassthrough(mb);
  if (shared_image == nullptr) {
    // Create texture to handle invalid mailbox (see http://crbug.com/472465 and
    // http://crbug.com/851878).
    DoGenTextures(1, &texture_client_id);
    InsertError(GL_INVALID_OPERATION, "invalid mailbox name.");
    return error::kNoError;
  }

  auto texture = shared_image->GetTexturePassthrough();

  // Update id mappings
  resources_->texture_id_map.RemoveClientID(texture_client_id);
  resources_->texture_id_map.SetIDMapping(texture_client_id,
                                          texture->service_id());
  resources_->texture_object_map.RemoveClientID(texture_client_id);
  resources_->texture_object_map.SetIDMapping(texture_client_id, texture);
  resources_->texture_shared_image_map[texture_client_id] =
      PassthroughResources::SharedImageData(this, std::move(shared_image));

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoBeginSharedImageAccessDirectCHROMIUM(
    GLuint client_id,
    GLenum mode) {
  if (mode != GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM &&
      mode != GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM) {
    InsertError(GL_INVALID_ENUM, "unrecognized access mode");
    return error::kNoError;
  }

  auto found = resources_->texture_shared_image_map.find(client_id);
  if (found == resources_->texture_shared_image_map.end()) {
    InsertError(GL_INVALID_OPERATION, "texture is not a shared image");
    return error::kNoError;
  }

  if (found->second.is_being_accessed()) {
    InsertError(GL_INVALID_OPERATION, "shared image is being accessed.");
    return error::kNoError;
  }

  if (!found->second.BeginAccess(mode, api())) {
    InsertError(GL_INVALID_OPERATION, "unable to begin access");
    return error::kNoError;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndSharedImageAccessDirectCHROMIUM(
    GLuint client_id) {
  auto found = resources_->texture_shared_image_map.find(client_id);
  if (found == resources_->texture_shared_image_map.end()) {
    InsertError(GL_INVALID_OPERATION, "texture is not a shared image");
    return error::kNoError;
  }
  if (!found->second.is_being_accessed()) {
    InsertError(GL_INVALID_OPERATION, "shared image is not being accessed.");
    return error::kNoError;
  }
  found->second.EndAccess();
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopySharedImageINTERNAL(
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    const volatile GLbyte* mailboxes) {
  if (!lazy_context_) {
    lazy_context_ = LazySharedContextState::Create(this);
    if (!lazy_context_) {
      return error::kNoError;
    }
  }
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ui::ScopedMakeCurrent smc(lazy_context_->shared_context_state()->context(),
                            lazy_context_->shared_context_state()->surface());
  CopySharedImageHelper helper(group_->shared_image_representation_factory(),
                               lazy_context_->shared_context_state());
  auto result = helper.CopySharedImage(xoffset, yoffset, x, y, width, height,
                                       unpack_flip_y, mailboxes);
  if (!result.has_value()) {
    InsertError(result.error().gl_error, result.error().msg);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoCopySharedImageToTextureINTERNAL(
    GLuint texture,
    GLenum target,
    GLuint internal_format,
    GLenum type,
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    GLboolean flip_y,
    const volatile GLbyte* src_mailbox) {
  if (!lazy_context_) {
    lazy_context_ = LazySharedContextState::Create(this);
    if (!lazy_context_) {
      return error::kNoError;
    }
  }
  ScopedPixelLocalStorageInterrupt scoped_pls_interrupt(this);
  ui::ScopedMakeCurrent smc(lazy_context_->shared_context_state()->context(),
                            lazy_context_->shared_context_state()->surface());

  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE) {
    InsertError(GL_INVALID_VALUE, "Invalid texture target");
    return error::kNoError;
  }
  if (!IsGLFormatAndTypeSupported(internal_format, type)) {
    InsertError(GL_INVALID_VALUE, "Invalid GL format");
    return error::kNoError;
  }

  GLuint gl_texture_service_id = GetTextureServiceID(
      api(), texture, resources_, /*create_if_missing=*/false);
  if (gl_texture_service_id == 0) {
    InsertError(GL_INVALID_OPERATION, "Cannot get texture service id");
    return error::kNoError;
  }

  CopySharedImageHelper helper(group_->shared_image_representation_factory(),
                               lazy_context_->shared_context_state());
  auto result = helper.CopySharedImageToGLTexture(
      gl_texture_service_id, target, internal_format, type, src_x, src_y, width,
      height, flip_y, src_mailbox);
  if (!result.has_value()) {
    InsertError(result.error().gl_error, result.error().msg);
  }
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEnableiOES(GLenum target,
                                                       GLuint index) {
  api()->glEnableiOESFn(target, index);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoDisableiOES(GLenum target,
                                                        GLuint index) {
  api()->glDisableiOESFn(target, index);
  return error::kNoError;
}

constexpr static char kPLSDefaultFramebufferBound[] =
    "Default framebuffer object name 0 does not support pixel local storage.";

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferMemorylessPixelLocalStorageANGLE(
    GLint plane,
    GLenum internalformat) {
  // Memoryless pixel local storage planes cannot be saved and restored for a
  // context switch, so we do not support them in the command buffer.
  InsertError(GL_INVALID_OPERATION,
              "glFramebufferMemorylessPixelLocalStorageANGLE is not supported");
  NOTIMPLEMENTED();
  return error::kUnknownCommand;
}

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferTexturePixelLocalStorageANGLE(
    GLint plane,
    GLuint backingtexture,
    GLint level,
    GLint layer) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  api()->glFramebufferTexturePixelLocalStorageANGLEFn(
      plane, GetTextureServiceID(api(), backingtexture, resources_, false),
      level, layer);
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferPixelLocalClearValuefvANGLE(
    GLint plane,
    const volatile GLfloat* value) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  api()->glFramebufferPixelLocalClearValuefvANGLEFn(
      plane, const_cast<const GLfloat*>(value));
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferPixelLocalClearValueivANGLE(
    GLint plane,
    const volatile GLint* value) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  api()->glFramebufferPixelLocalClearValueivANGLEFn(
      plane, const_cast<const GLint*>(value));
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferPixelLocalClearValueuivANGLE(
    GLint plane,
    const volatile GLuint* value) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  api()->glFramebufferPixelLocalClearValueuivANGLEFn(
      plane, const_cast<const GLuint*>(value));
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoBeginPixelLocalStorageANGLE(
    GLsizei n,
    const volatile GLenum* loadops) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  // Copy the loadops to avoid a TOCTOU race condition.
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "<n> cannot be negative.");
    return error::kNoError;
  }
  if (n > kPassthroughMaxPLSPlanes) {
    InsertError(GL_INVALID_VALUE,
                "<n> must be <= GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    return error::kNoError;
  }
  GLenum loadops_copy[kPassthroughMaxPLSPlanes];
  std::copy(loadops, loadops + n, loadops_copy);
  api()->glBeginPixelLocalStorageANGLEFn(n, loadops_copy);
  has_activated_pixel_local_storage_ = true;
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoEndPixelLocalStorageANGLE(
    GLsizei n,
    const volatile GLenum* storeops) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  // Copy the storeops to avoid a TOCTOU race condition.
  if (n < 0) {
    InsertError(GL_INVALID_VALUE, "<n> cannot be negative.");
    return error::kNoError;
  }
  if (n > kPassthroughMaxPLSPlanes) {
    InsertError(GL_INVALID_VALUE,
                "<n> must be <= GL_MAX_PIXEL_LOCAL_STORAGE_PLANES_ANGLE.");
    return error::kNoError;
  }
  GLenum storeops_copy[kPassthroughMaxPLSPlanes];
  std::copy(storeops, storeops + n, storeops_copy);
  api()->glEndPixelLocalStorageANGLEFn(n, storeops_copy);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPixelLocalStorageBarrierANGLE() {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    return error::kNoError;
  }
  api()->glPixelLocalStorageBarrierANGLEFn();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferPixelLocalStorageInterruptANGLE() {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    return error::kNoError;
  }
  api()->glFramebufferPixelLocalStorageInterruptANGLEFn();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoFramebufferPixelLocalStorageRestoreANGLE() {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    return error::kNoError;
  }
  api()->glFramebufferPixelLocalStorageRestoreANGLEFn();
  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoGetFramebufferPixelLocalStorageParameterfvANGLE(
    GLint plane,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLfloat* params) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    *length = 0;
    return error::kNoError;
  }

  CheckErrorCallbackState();

  api()->glGetFramebufferPixelLocalStorageParameterfvRobustANGLEFn(
      plane, pname, bufsize, length, params);

  if (CheckErrorCallbackState()) {
    *length = 0;
    return error::kNoError;
  }

  return error::kNoError;
}

error::Error
GLES2DecoderPassthroughImpl::DoGetFramebufferPixelLocalStorageParameterivANGLE(
    GLint plane,
    GLenum pname,
    GLsizei bufsize,
    GLsizei* length,
    GLint* params) {
  if (IsEmulatedFramebufferBound(GL_DRAW_FRAMEBUFFER)) {
    InsertError(GL_INVALID_OPERATION, kPLSDefaultFramebufferBound);
    *length = 0;
    return error::kNoError;
  }

  CheckErrorCallbackState();

  api()->glGetFramebufferPixelLocalStorageParameterivRobustANGLEFn(
      plane, pname, bufsize, length, params);

  if (CheckErrorCallbackState()) {
    *length = 0;
    return error::kNoError;
  }

  if (PatchGetFramebufferPixelLocalStorageParameterivANGLE(
          plane, pname, *length, params) != error::kNoError) {
    *length = 0;
    return error::kInvalidArguments;
  }

  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoProvokingVertexANGLE(
    GLenum provokeMode) {
  api()->glProvokingVertexANGLEFn(provokeMode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoClipControlEXT(GLenum origin,
                                                           GLenum depth) {
  api()->glClipControlEXTFn(origin, depth);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPolygonModeANGLE(GLenum face,
                                                             GLenum mode) {
  api()->glPolygonModeANGLEFn(face, mode);
  return error::kNoError;
}

error::Error GLES2DecoderPassthroughImpl::DoPolygonOffsetClampEXT(
    GLfloat factor,
    GLfloat units,
    GLfloat clamp) {
  api()->glPolygonOffsetClampEXTFn(factor, units, clamp);
  return error::kNoError;
}

}  // namespace gles2
}  // namespace gpu
