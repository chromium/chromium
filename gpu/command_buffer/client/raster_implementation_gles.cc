// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation_gles.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "cc/paint/color_space_transfer_cache_entry.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"  // nogncheck
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace gpu {
namespace raster {

static GLenum GetImageTextureTarget(const gpu::Capabilities& caps,
                                    gfx::BufferUsage usage,
                                    viz::ResourceFormat format) {
  gfx::BufferFormat buffer_format = viz::BufferFormat(format);
  return GetBufferTextureTarget(usage, buffer_format, caps);
}

RasterImplementationGLES::Texture::Texture(GLuint id,
                                           GLenum target,
                                           bool use_buffer,
                                           gfx::BufferUsage buffer_usage,
                                           viz::ResourceFormat format)
    : id(id),
      target(target),
      use_buffer(use_buffer),
      buffer_usage(buffer_usage),
      format(format) {}

RasterImplementationGLES::Texture* RasterImplementationGLES::GetTexture(
    GLuint texture_id) {
  auto it = texture_info_.find(texture_id);
  DCHECK(it != texture_info_.end()) << "Undefined texture id";
  return &it->second;
}

RasterImplementationGLES::Texture* RasterImplementationGLES::EnsureTextureBound(
    RasterImplementationGLES::Texture* texture) {
  DCHECK(texture);
  // Reads client side cache of bindings in GLES2Implementation.
  GLint bound_texture = 0;
  GLenum pname = 0;
  switch (texture->target) {
    case GL_TEXTURE_2D:
      pname = GL_TEXTURE_BINDING_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      pname = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      pname = GL_TEXTURE_BINDING_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
  }
  if (pname != 0)
    gl_->GetIntegerv(pname, &bound_texture);
  if (bound_texture != static_cast<GLint>(texture->id))
    gl_->BindTexture(texture->target, texture->id);

  return texture;
}

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    const gpu::Capabilities& caps)
    : gl_(gl),
      caps_(caps),
      use_texture_storage_(caps.texture_storage),
      use_texture_storage_image_(caps.texture_storage_image) {}

RasterImplementationGLES::~RasterImplementationGLES() {}

void RasterImplementationGLES::Finish() {
  gl_->Finish();
}

void RasterImplementationGLES::Flush() {
  gl_->Flush();
}

void RasterImplementationGLES::ShallowFlushCHROMIUM() {
  gl_->ShallowFlushCHROMIUM();
}

void RasterImplementationGLES::OrderingBarrierCHROMIUM() {
  gl_->OrderingBarrierCHROMIUM();
}

void RasterImplementationGLES::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  gl_->GenSyncTokenCHROMIUM(sync_token);
}

void RasterImplementationGLES::GenUnverifiedSyncTokenCHROMIUM(
    GLbyte* sync_token) {
  gl_->GenUnverifiedSyncTokenCHROMIUM(sync_token);
}

void RasterImplementationGLES::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                        GLsizei count) {
  gl_->VerifySyncTokensCHROMIUM(sync_tokens, count);
}

void RasterImplementationGLES::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  gl_->WaitSyncTokenCHROMIUM(sync_token);
}

GLenum RasterImplementationGLES::GetError() {
  return gl_->GetError();
}

GLenum RasterImplementationGLES::GetGraphicsResetStatusKHR() {
  return gl_->GetGraphicsResetStatusKHR();
}

void RasterImplementationGLES::GetIntegerv(GLenum pname, GLint* params) {
  gl_->GetIntegerv(pname, params);
}

void RasterImplementationGLES::LoseContextCHROMIUM(GLenum current,
                                                   GLenum other) {
  gl_->LoseContextCHROMIUM(current, other);
}

void RasterImplementationGLES::GenQueriesEXT(GLsizei n, GLuint* queries) {
  gl_->GenQueriesEXT(n, queries);
}

void RasterImplementationGLES::DeleteQueriesEXT(GLsizei n,
                                                const GLuint* queries) {
  gl_->DeleteQueriesEXT(n, queries);
}

void RasterImplementationGLES::BeginQueryEXT(GLenum target, GLuint id) {
  gl_->BeginQueryEXT(target, id);
}

void RasterImplementationGLES::EndQueryEXT(GLenum target) {
  gl_->EndQueryEXT(target);
}

void RasterImplementationGLES::GetQueryObjectuivEXT(GLuint id,
                                                    GLenum pname,
                                                    GLuint* params) {
  gl_->GetQueryObjectuivEXT(id, pname, params);
}

GLuint RasterImplementationGLES::CreateTexture(bool use_buffer,
                                               gfx::BufferUsage buffer_usage,
                                               viz::ResourceFormat format) {
  GLuint texture_id = 0;
  gl_->GenTextures(1, &texture_id);
  DCHECK(texture_id);
  DCHECK(!viz::IsResourceFormatCompressed(format));
  GLenum target = use_buffer
                      ? GetImageTextureTarget(caps_, buffer_usage, format)
                      : GL_TEXTURE_2D;
  texture_info_.emplace(std::make_pair(
      texture_id,
      Texture(texture_id, target, use_buffer, buffer_usage, format)));
  return texture_id;
}

void RasterImplementationGLES::DeleteTextures(GLsizei n,
                                              const GLuint* textures) {
  DCHECK_GT(n, 0);
  for (GLsizei i = 0; i < n; i++) {
    auto texture_iter = texture_info_.find(textures[i]);
    DCHECK(texture_iter != texture_info_.end());

    texture_info_.erase(texture_iter);
  }

  gl_->DeleteTextures(n, textures);
}

void RasterImplementationGLES::SetColorSpaceMetadata(GLuint texture_id,
                                                     GLColorSpace color_space) {
  Texture* texture = GetTexture(texture_id);
  gl_->SetColorSpaceMetadataCHROMIUM(texture->id, color_space);
}

void RasterImplementationGLES::TexParameteri(GLuint texture_id,
                                             GLenum pname,
                                             GLint param) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));
  gl_->TexParameteri(texture->target, pname, param);
}

void RasterImplementationGLES::ProduceTextureDirect(GLuint texture_id,
                                                    GLbyte* mailbox) {
  Texture* texture = GetTexture(texture_id);
  gl_->ProduceTextureDirectCHROMIUM(texture->id, mailbox);
}

GLuint RasterImplementationGLES::CreateAndConsumeTexture(
    bool use_buffer,
    gfx::BufferUsage buffer_usage,
    viz::ResourceFormat format,
    const GLbyte* mailbox) {
  GLuint texture_id = gl_->CreateAndConsumeTextureCHROMIUM(mailbox);
  DCHECK(texture_id);
  DCHECK(!viz::IsResourceFormatCompressed(format));

  GLenum target = use_buffer
                      ? GetImageTextureTarget(caps_, buffer_usage, format)
                      : GL_TEXTURE_2D;
  texture_info_.emplace(std::make_pair(
      texture_id,
      Texture(texture_id, target, use_buffer, buffer_usage, format)));

  return texture_id;
}

GLuint RasterImplementationGLES::CreateImageCHROMIUM(ClientBuffer buffer,
                                                     GLsizei width,
                                                     GLsizei height,
                                                     GLenum internalformat) {
  return gl_->CreateImageCHROMIUM(buffer, width, height, internalformat);
}

void RasterImplementationGLES::BindTexImage2DCHROMIUM(GLuint texture_id,
                                                      GLint image_id) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));
  gl_->BindTexImage2DCHROMIUM(texture->target, image_id);
}

void RasterImplementationGLES::ReleaseTexImage2DCHROMIUM(GLuint texture_id,
                                                         GLint image_id) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));
  gl_->ReleaseTexImage2DCHROMIUM(texture->target, image_id);
}

void RasterImplementationGLES::DestroyImageCHROMIUM(GLuint image_id) {
  gl_->DestroyImageCHROMIUM(image_id);
}

void RasterImplementationGLES::TexStorage2D(GLuint texture_id,
                                            GLsizei width,
                                            GLsizei height) {
  Texture* texture = EnsureTextureBound(GetTexture(texture_id));

  if (texture->use_buffer) {
    DCHECK(use_texture_storage_image_);
    gl_->TexStorage2DImageCHROMIUM(texture->target,
                                   viz::TextureStorageFormat(texture->format),
                                   GL_SCANOUT_CHROMIUM, width, height);
  } else if (use_texture_storage_) {
    gl_->TexStorage2DEXT(texture->target, /*levels=*/1,
                         viz::TextureStorageFormat(texture->format), width,
                         height);
  } else {
    DCHECK(GLSupportsFormat(texture->format));
    gl_->TexImage2D(texture->target, 0, viz::GLInternalFormat(texture->format),
                    width, height, 0, viz::GLDataFormat(texture->format),
                    viz::GLDataType(texture->format), nullptr);
  }
}

void RasterImplementationGLES::CopySubTexture(GLuint source_id,
                                              GLuint dest_id,
                                              GLint xoffset,
                                              GLint yoffset,
                                              GLint x,
                                              GLint y,
                                              GLsizei width,
                                              GLsizei height) {
  Texture* source = GetTexture(source_id);
  Texture* dest = GetTexture(dest_id);

  gl_->CopySubTextureCHROMIUM(source->id, 0, dest->target, dest->id, 0, xoffset,
                              yoffset, x, y, width, height, false, false,
                              false);
}

void RasterImplementationGLES::UnpremultiplyAndDitherCopyCHROMIUM(
    GLuint source_id,
    GLuint dest_id,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height) {
  Texture* source = GetTexture(source_id);
  Texture* dest = GetTexture(dest_id);

  gl_->UnpremultiplyAndDitherCopyCHROMIUM(source->id, dest->id, x, y, width,
                                          height);
}

void RasterImplementationGLES::BeginRasterCHROMIUM(
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    GLint color_type,
    const cc::RasterColorSpace& raster_color_space,
    const GLbyte* mailbox) {
  NOTREACHED();
}

void RasterImplementationGLES::RasterCHROMIUM(
    const cc::DisplayItemList* list,
    cc::ImageProvider* provider,
    const gfx::Size& content_size,
    const gfx::Rect& full_raster_rect,
    const gfx::Rect& playback_rect,
    const gfx::Vector2dF& post_translate,
    GLfloat post_scale,
    bool requires_clear) {
  NOTREACHED();
}

void RasterImplementationGLES::SetActiveURLCHROMIUM(const char* url) {
  gl_->SetActiveURLCHROMIUM(url);
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  NOTREACHED();
}

void RasterImplementationGLES::BeginGpuRaster() {
  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceBeginCHROMIUM("BeginGpuRaster", "GpuRasterization");
}

void RasterImplementationGLES::EndGpuRaster() {
  // Restore default GL unpack alignment.  TextureUploader expects this.
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, 4);

  // Using push/pop functions directly incurs cost to evaluate function
  // arguments even when tracing is disabled.
  gl_->TraceEndCHROMIUM();

  // Reset cached raster state.
  gl_->ActiveTexture(GL_TEXTURE0);
}

void RasterImplementationGLES::TraceBeginCHROMIUM(const char* category_name,
                                                  const char* trace_name) {
  gl_->TraceBeginCHROMIUM(category_name, trace_name);
}

void RasterImplementationGLES::TraceEndCHROMIUM() {
  gl_->TraceEndCHROMIUM();
}

}  // namespace raster
}  // namespace gpu
