// Copyright 2017 The Chromium Authors
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
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"  // nogncheck
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gl_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace gpu {
namespace raster {

namespace {

GLenum SkColorTypeToGLDataFormat(SkColorType color_type, bool supports_rg) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
      return GL_RGBA;
    case kBGRA_8888_SkColorType:
      return GL_BGRA_EXT;
    case kR8G8_unorm_SkColorType:
    case kR16G16_unorm_SkColorType:
      return GL_RG_EXT;
    case kGray_8_SkColorType:
      return supports_rg ? GL_RED : GL_LUMINANCE;
    case kAlpha_8_SkColorType:
    case kA16_unorm_SkColorType:
      return supports_rg ? GL_RED : GL_ALPHA;
    // kA16_float_SkColorType is only used by LUMINANCE_F16 format and hence
    // should only support GL_LUMINANCE.
    case kA16_float_SkColorType:
      return GL_LUMINANCE;
    default:
      DLOG(ERROR) << "Unknown SkColorType " << color_type;
  }
  NOTREACHED();
  return 0;
}

GLenum SkColorTypeToGLDataType(SkColorType color_type) {
  switch (color_type) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
    case kR8G8_unorm_SkColorType:
    case kGray_8_SkColorType:
    case kAlpha_8_SkColorType:
      return GL_UNSIGNED_BYTE;
    case kA16_unorm_SkColorType:
    case kR16G16_unorm_SkColorType:
      return GL_UNSIGNED_SHORT;
    case kA16_float_SkColorType:
      return GL_HALF_FLOAT_OES;
    default:
      DLOG(ERROR) << "Unknown SkColorType " << color_type;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

RasterImplementationGLES::RasterImplementationGLES(
    gles2::GLES2Interface* gl,
    ContextSupport* context_support,
    const gpu::Capabilities& caps)
    : gl_(gl), context_support_(context_support), capabilities_(caps) {}

RasterImplementationGLES::~RasterImplementationGLES() = default;

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

GLenum RasterImplementationGLES::GetError() {
  return gl_->GetError();
}

GLenum RasterImplementationGLES::GetGraphicsResetStatusKHR() {
  return gl_->GetGraphicsResetStatusKHR();
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

void RasterImplementationGLES::QueryCounterEXT(GLuint id, GLenum target) {
  gl_->QueryCounterEXT(id, target);
}

void RasterImplementationGLES::GetQueryObjectuivEXT(GLuint id,
                                                    GLenum pname,
                                                    GLuint* params) {
  gl_->GetQueryObjectuivEXT(id, pname, params);
}

void RasterImplementationGLES::GetQueryObjectui64vEXT(GLuint id,
                                                      GLenum pname,
                                                      GLuint64* params) {
  gl_->GetQueryObjectui64vEXT(id, pname, params);
}

void RasterImplementationGLES::CopySharedImage(
    const gpu::Mailbox& source_mailbox,
    const gpu::Mailbox& dest_mailbox,
    GLenum dest_target,
    GLint xoffset,
    GLint yoffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLboolean unpack_flip_y,
    GLboolean unpack_premultiply_alpha) {
  // CopySharedImage does not support legacy mailboxes so fallback to
  // CopySubTexture.
  if (capabilities_.supports_yuv_rgb_conversion &&
      source_mailbox.IsSharedImage() && dest_mailbox.IsSharedImage()) {
    if (width < 0) {
      LOG(ERROR) << "GL_INVALID_VALUE, glCopySharedImage, width < 0";
      return;
    }
    if (height < 0) {
      LOG(ERROR) << "GL_INVALID_VALUE, glCopySharedImage, height < 0";
      return;
    }
    GLbyte mailboxes[sizeof(source_mailbox.name) * 2];
    memcpy(mailboxes, source_mailbox.name, sizeof(source_mailbox.name));
    memcpy(mailboxes + sizeof(source_mailbox.name), dest_mailbox.name,
           sizeof(dest_mailbox.name));
    gl_->CopySharedImageINTERNAL(xoffset, yoffset, x, y, width, height,
                                 unpack_flip_y, mailboxes);
  } else {
    GLuint texture_ids[2] = {
        CreateAndConsumeForGpuRaster(source_mailbox),
        CreateAndConsumeForGpuRaster(dest_mailbox),
    };
    DCHECK(texture_ids[0]);
    DCHECK(texture_ids[1]);

    BeginSharedImageAccessDirectCHROMIUM(
        texture_ids[0], GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    BeginSharedImageAccessDirectCHROMIUM(
        texture_ids[1], GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

    gl_->CopySubTextureCHROMIUM(texture_ids[0], 0, dest_target, texture_ids[1],
                                0, xoffset, yoffset, x, y, width, height,
                                unpack_flip_y, unpack_premultiply_alpha,
                                false /* upack_unmultiply_alpha */);

    EndSharedImageAccessDirectCHROMIUM(texture_ids[0]);
    EndSharedImageAccessDirectCHROMIUM(texture_ids[1]);
    gl_->DeleteTextures(2, texture_ids);
  }
}

void RasterImplementationGLES::WritePixels(const gpu::Mailbox& dest_mailbox,
                                           int dst_x_offset,
                                           int dst_y_offset,
                                           int dst_plane_index,
                                           GLenum texture_target,
                                           const SkPixmap& src_sk_pixmap) {
  const auto& src_info = src_sk_pixmap.info();
  const auto& src_row_bytes = src_sk_pixmap.rowBytes();
  DCHECK_GE(src_row_bytes, src_info.minRowBytes());
  GLuint texture_id = CreateAndConsumeForGpuRaster(dest_mailbox);
  BeginSharedImageAccessDirectCHROMIUM(
      texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);

  GLint old_align = 0;
  gl_->GetIntegerv(GL_UNPACK_ALIGNMENT, &old_align);
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl_->PixelStorei(GL_UNPACK_ROW_LENGTH,
                   src_row_bytes / src_info.bytesPerPixel());
  gl_->BindTexture(texture_target, texture_id);
  gl_->TexSubImage2D(
      texture_target, 0, dst_x_offset, dst_y_offset, src_info.width(),
      src_info.height(),
      SkColorTypeToGLDataFormat(src_info.colorType(), capabilities_.texture_rg),
      SkColorTypeToGLDataType(src_info.colorType()), src_sk_pixmap.addr());
  gl_->BindTexture(texture_target, 0);
  gl_->PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  gl_->PixelStorei(GL_UNPACK_ALIGNMENT, old_align);

  EndSharedImageAccessDirectCHROMIUM(texture_id);
  DeleteGpuRasterTexture(texture_id);
}

void RasterImplementationGLES::WritePixelsYUV(
    const gpu::Mailbox& dest_mailbox,
    const SkYUVAPixmaps& src_yuv_pixmap) {
  const auto& src_yuv_info = src_yuv_pixmap.yuvaInfo();
  const auto& src_yuv_pixmap_info = src_yuv_pixmap.pixmapsInfo();
  const std::array<SkPixmap, SkYUVAInfo::kMaxPlanes>& src_sk_pixmaps =
      src_yuv_pixmap.planes();

  gl_->WritePixelsYUVINTERNAL(
      dest_mailbox.name, src_sk_pixmaps[0].computeByteSize(),
      src_sk_pixmaps[1].computeByteSize(), src_sk_pixmaps[2].computeByteSize(),
      src_sk_pixmaps[3].computeByteSize(), src_yuv_info.width(),
      src_yuv_info.height(), static_cast<int>(src_yuv_info.planeConfig()),
      static_cast<int>(src_yuv_info.subsampling()),
      static_cast<int>(src_yuv_pixmap_info.dataType()),
      src_sk_pixmaps[0].rowBytes(), src_sk_pixmaps[1].rowBytes(),
      src_sk_pixmaps[2].rowBytes(), src_sk_pixmaps[3].rowBytes(),
      src_sk_pixmaps[0].addr(), src_sk_pixmaps[1].addr(),
      src_sk_pixmaps[2].addr(), src_sk_pixmaps[3].addr());
}

void RasterImplementationGLES::ConvertYUVAMailboxesToRGB(
    const gpu::Mailbox& dest_mailbox,
    GLint src_x,
    GLint src_y,
    GLsizei width,
    GLsizei height,
    SkYUVColorSpace planes_yuv_color_space,
    const SkColorSpace* planes_rgb_color_space,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    const gpu::Mailbox yuva_plane_mailboxes[]) {
  skcms_Matrix3x3 primaries = {{{0}}};
  skcms_TransferFunction transfer = {0};
  if (planes_rgb_color_space) {
    planes_rgb_color_space->toXYZD50(&primaries);
    planes_rgb_color_space->transferFn(&transfer);
  } else {
    // Specify an invalid transfer function exponent, to ensure that when
    // SkColorSpace::MakeRGB is called in the decoder, the result is nullptr.
    transfer.g = -99;
  }

  constexpr size_t kByteSize =
      sizeof(gpu::Mailbox) * (SkYUVAInfo::kMaxPlanes + 1) +
      sizeof(skcms_TransferFunction) + sizeof(skcms_Matrix3x3);
  // 144 is the count in build_gles2_cmd_buffer.py for GL_MAILBOX_SIZE_CHROMIUM
  // x5 + 16 floats.
  static_assert(kByteSize == 144);
  GLbyte bytes[kByteSize] = {0};
  size_t offset = 0;
  for (int i = 0; i < SkYUVAInfo::NumPlanes(plane_config); ++i) {
    memcpy(bytes + offset, yuva_plane_mailboxes + i, sizeof(gpu::Mailbox));
    offset += sizeof(gpu::Mailbox);
  }
  offset = SkYUVAInfo::kMaxPlanes * sizeof(gpu::Mailbox);
  memcpy(bytes + offset, &dest_mailbox, sizeof(gpu::Mailbox));
  offset += sizeof(gpu::Mailbox);
  memcpy(bytes + offset, &transfer, sizeof(transfer));
  offset += sizeof(transfer);
  memcpy(bytes + offset, &primaries, sizeof(primaries));
  offset += sizeof(primaries);
  DCHECK_EQ(offset, kByteSize);

  gl_->ConvertYUVAMailboxesToRGBINTERNAL(
      src_x, src_y, width, height, planes_yuv_color_space,
      static_cast<GLenum>(plane_config), static_cast<GLenum>(subsampling),
      reinterpret_cast<GLbyte*>(bytes));
}

void RasterImplementationGLES::ConvertRGBAToYUVAMailboxes(
    SkYUVColorSpace planes_yuv_color_space,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    const gpu::Mailbox yuva_plane_mailboxes[],
    const gpu::Mailbox& source_mailbox) {
  gpu::Mailbox mailboxes[SkYUVAInfo::kMaxPlanes + 1];
  for (int i = 0; i < SkYUVAInfo::NumPlanes(plane_config); ++i) {
    mailboxes[i] = yuva_plane_mailboxes[i];
  }
  mailboxes[SkYUVAInfo::kMaxPlanes] = source_mailbox;
  gl_->ConvertRGBAToYUVAMailboxesINTERNAL(
      planes_yuv_color_space, static_cast<GLenum>(plane_config),
      static_cast<GLenum>(subsampling), reinterpret_cast<GLbyte*>(mailboxes));
}

void RasterImplementationGLES::BeginRasterCHROMIUM(
    SkColor4f sk_color_4f,
    GLboolean needs_clear,
    GLuint msaa_sample_count,
    MsaaMode msaa_mode,
    GLboolean can_use_lcd_text,
    GLboolean visible,
    const gfx::ColorSpace& color_space,
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
    const gfx::Vector2dF& post_scale,
    bool requires_clear,
    size_t* max_op_size_hint) {
  NOTREACHED();
}

void RasterImplementationGLES::SetActiveURLCHROMIUM(const char* url) {
  gl_->SetActiveURLCHROMIUM(url);
}

void RasterImplementationGLES::EndRasterCHROMIUM() {
  NOTREACHED();
}

SyncToken RasterImplementationGLES::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    uint32_t transfer_cache_entry_id,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  NOTREACHED();
  return SyncToken();
}

void RasterImplementationGLES::ReadbackARGBPixelsAsync(
    const gpu::Mailbox& source_mailbox,
    GLenum source_target,
    GrSurfaceOrigin src_origin,
    const gfx::Size& source_size,
    const gfx::Point& source_starting_point,
    const SkImageInfo& dst_info,
    GLuint dst_row_bytes,
    unsigned char* out,
    base::OnceCallback<void(bool)> readback_done) {
  DCHECK(!readback_done.is_null());
  DCHECK(dst_info.colorType() == kRGBA_8888_SkColorType ||
         dst_info.colorType() == kBGRA_8888_SkColorType);
  GLenum format =
      dst_info.colorType() == kRGBA_8888_SkColorType ? GL_RGBA : GL_BGRA_EXT;
  gfx::Size dst_gfx_size(dst_info.width(), dst_info.height());
  GLuint texture_id = CreateAndConsumeForGpuRaster(source_mailbox);
  BeginSharedImageAccessDirectCHROMIUM(
      texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);

  // Convert bottom-left GL coordinates to top-left coordinates expected
  // by RI clients.
  bool flip_y;
  gfx::Point starting_point(source_starting_point);
  switch (src_origin) {
    case kTopLeft_GrSurfaceOrigin:
      flip_y = false;
      break;
    case kBottomLeft_GrSurfaceOrigin:
      // Since RI clients always expect top-left origin, two things need to be
      // done when texture's origin is bottom-left.

      // 1. Rows in the output buffer need to be switched vertically.
      flip_y = true;

      // 2. Starting of a target rectangle needs to be adjusted from top-left
      //    to bottom-left. That's how glReadPixels expects it.
      // It's okay if we accidentally go negative here, glReadPixels checks
      // its input.
      starting_point.set_y(source_size.height() - starting_point.y() -
                           dst_gfx_size.height());
      break;
  }

  GetGLHelper()->ReadbackTextureAsync(
      texture_id, source_target, starting_point, dst_gfx_size, out,
      dst_row_bytes, flip_y, format,
      base::BindOnce(&RasterImplementationGLES::OnReadARGBPixelsAsync,
                     weak_ptr_factory_.GetWeakPtr(), texture_id,
                     std::move(readback_done)));
}

void RasterImplementationGLES::OnReadARGBPixelsAsync(
    GLuint texture_id,
    base::OnceCallback<void(bool)> readback_done,
    bool success) {
  DCHECK(texture_id);
  EndSharedImageAccessDirectCHROMIUM(texture_id);
  DeleteGpuRasterTexture(texture_id);

  std::move(readback_done).Run(success);
}

void RasterImplementationGLES::ReadbackYUVPixelsAsync(
    const gpu::Mailbox& source_mailbox,
    GLenum source_target,
    const gfx::Size& source_size,
    const gfx::Rect& output_rect,
    bool vertically_flip_texture,
    int y_plane_row_stride_bytes,
    unsigned char* y_plane_data,
    int u_plane_row_stride_bytes,
    unsigned char* u_plane_data,
    int v_plane_row_stride_bytes,
    unsigned char* v_plane_data,
    const gfx::Point& paste_location,
    base::OnceCallback<void()> release_mailbox,
    base::OnceCallback<void(bool)> readback_done) {
  GLuint shared_texture_id = CreateAndConsumeForGpuRaster(source_mailbox);
  BeginSharedImageAccessDirectCHROMIUM(
      shared_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  base::OnceCallback<void()> on_release_mailbox =
      base::BindOnce(&RasterImplementationGLES::OnReleaseMailbox,
                     weak_ptr_factory_.GetWeakPtr(), shared_texture_id,
                     std::move(release_mailbox));

  // The YUV readback path only works for 2D textures.
  GLuint texture_for_readback = shared_texture_id;
  GLuint copy_texture_id = 0;
  if (source_target != GL_TEXTURE_2D) {
    int width = source_size.width();
    int height = source_size.height();

    gl_->GenTextures(1, &copy_texture_id);
    gl_->BindTexture(GL_TEXTURE_2D, copy_texture_id);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, nullptr);
    gl_->CopyTextureCHROMIUM(shared_texture_id, 0, GL_TEXTURE_2D,
                             copy_texture_id, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0,
                             0, 0);
    texture_for_readback = copy_texture_id;

    // |copy_texture_id| now contains the texture we want to copy, release the
    // pinned mailbox.
    std::move(on_release_mailbox).Run();
  }

  DCHECK(GetGLHelper());
  gpu::ReadbackYUVInterface* const yuv_reader =
      GetGLHelper()->GetReadbackPipelineYUV(vertically_flip_texture);
  yuv_reader->ReadbackYUV(
      texture_for_readback, source_size, gfx::Rect(source_size),
      y_plane_row_stride_bytes, y_plane_data, u_plane_row_stride_bytes,
      u_plane_data, v_plane_row_stride_bytes, v_plane_data, paste_location,
      base::BindOnce(&RasterImplementationGLES::OnReadYUVPixelsAsync,
                     weak_ptr_factory_.GetWeakPtr(), copy_texture_id,
                     std::move(on_release_mailbox), std::move(readback_done)));
}

void RasterImplementationGLES::OnReadYUVPixelsAsync(
    GLuint copy_texture_id,
    base::OnceCallback<void()> on_release_mailbox,
    base::OnceCallback<void(bool)> readback_done,
    bool success) {
  if (copy_texture_id) {
    DCHECK(on_release_mailbox.is_null());
    gl_->DeleteTextures(1, &copy_texture_id);
  } else {
    DCHECK(!on_release_mailbox.is_null());
    std::move(on_release_mailbox).Run();
  }

  std::move(readback_done).Run(success);
}

void RasterImplementationGLES::OnReleaseMailbox(
    GLuint shared_texture_id,
    base::OnceCallback<void()> release_mailbox) {
  DCHECK(shared_texture_id);
  DCHECK(!release_mailbox.is_null());

  EndSharedImageAccessDirectCHROMIUM(shared_texture_id);
  DeleteGpuRasterTexture(shared_texture_id);
  std::move(release_mailbox).Run();
}

void RasterImplementationGLES::ReadbackImagePixels(
    const gpu::Mailbox& source_mailbox,
    const SkImageInfo& dst_info,
    GLuint dst_row_bytes,
    int src_x,
    int src_y,
    int plane_index,
    void* dst_pixels) {
  DCHECK_GE(dst_row_bytes, dst_info.minRowBytes());

  sk_sp<SkData> dst_color_space_data;
  if (dst_info.colorSpace()) {
    dst_color_space_data = dst_info.colorSpace()->serialize();
  }

  GLuint dst_size = dst_info.computeByteSize(dst_row_bytes);
  gl_->ReadbackARGBImagePixelsINTERNAL(
      source_mailbox.name,
      dst_color_space_data ? dst_color_space_data->data() : nullptr,
      dst_color_space_data ? dst_color_space_data->size() : 0, dst_size,
      dst_info.width(), dst_info.height(), dst_info.colorType(),
      dst_info.alphaType(), dst_row_bytes, src_x, src_y, plane_index,
      dst_pixels);
}

GLuint RasterImplementationGLES::CreateAndConsumeForGpuRaster(
    const gpu::Mailbox& mailbox) {
  return mailbox.IsSharedImage()
             ? gl_->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name)
             : gl_->CreateAndConsumeTextureCHROMIUM(mailbox.name);
}

void RasterImplementationGLES::DeleteGpuRasterTexture(GLuint texture) {
  gl_->DeleteTextures(1u, &texture);
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

void RasterImplementationGLES::BeginSharedImageAccessDirectCHROMIUM(
    GLuint texture,
    GLenum mode) {
  gl_->BeginSharedImageAccessDirectCHROMIUM(texture, mode);
}

void RasterImplementationGLES::EndSharedImageAccessDirectCHROMIUM(
    GLuint texture) {
  gl_->EndSharedImageAccessDirectCHROMIUM(texture);
}

void RasterImplementationGLES::InitializeDiscardableTextureCHROMIUM(
    GLuint texture) {
  gl_->InitializeDiscardableTextureCHROMIUM(texture);
}

void RasterImplementationGLES::UnlockDiscardableTextureCHROMIUM(
    GLuint texture) {
  gl_->UnlockDiscardableTextureCHROMIUM(texture);
}

bool RasterImplementationGLES::LockDiscardableTextureCHROMIUM(GLuint texture) {
  return gl_->LockDiscardableTextureCHROMIUM(texture);
}

void RasterImplementationGLES::TraceBeginCHROMIUM(const char* category_name,
                                                  const char* trace_name) {
  gl_->TraceBeginCHROMIUM(category_name, trace_name);
}

void RasterImplementationGLES::TraceEndCHROMIUM() {
  gl_->TraceEndCHROMIUM();
}

// InterfaceBase implementation.
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

GLHelper* RasterImplementationGLES::GetGLHelper() {
  if (!gl_helper_) {
    DCHECK(gl_);
    DCHECK(context_support_);
    gl_helper_ = std::make_unique<GLHelper>(gl_, context_support_);
  }

  return gl_helper_.get();
}

}  // namespace raster
}  // namespace gpu
