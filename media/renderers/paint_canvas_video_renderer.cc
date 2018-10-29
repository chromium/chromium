// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/paint_canvas_video_renderer.h"

#include <GLES3/gl3.h>
#include <limits>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/data_buffer.h"
#include "media/base/video_frame.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageGenerator.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

// Skia internal format depends on a platform. On Android it is ABGR, on others
// it is ARGB.
#if SK_B32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_R32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToARGB
#define LIBYUV_I422_TO_ARGB libyuv::I422ToARGB
#define LIBYUV_I444_TO_ARGB libyuv::I444ToARGB
#define LIBYUV_I420ALPHA_TO_ARGB libyuv::I420AlphaToARGB
#define LIBYUV_J420_TO_ARGB libyuv::J420ToARGB
#define LIBYUV_H420_TO_ARGB libyuv::H420ToARGB
#define LIBYUV_I010_TO_ARGB libyuv::I010ToARGB
#define LIBYUV_H010_TO_ARGB libyuv::H010ToARGB
#define LIBYUV_NV12_TO_ARGB libyuv::NV12ToARGB
#elif SK_R32_SHIFT == 0 && SK_G32_SHIFT == 8 && SK_B32_SHIFT == 16 && \
    SK_A32_SHIFT == 24
#define LIBYUV_I420_TO_ARGB libyuv::I420ToABGR
#define LIBYUV_I422_TO_ARGB libyuv::I422ToABGR
#define LIBYUV_I444_TO_ARGB libyuv::I444ToABGR
#define LIBYUV_I420ALPHA_TO_ARGB libyuv::I420AlphaToABGR
#define LIBYUV_J420_TO_ARGB libyuv::J420ToABGR
#define LIBYUV_H420_TO_ARGB libyuv::H420ToABGR
#define LIBYUV_I010_TO_ARGB libyuv::I010ToABGR
#define LIBYUV_H010_TO_ARGB libyuv::H010ToABGR
#define LIBYUV_NV12_TO_ARGB libyuv::NV12ToABGR
#else
#error Unexpected Skia ARGB_8888 layout!
#endif

namespace media {

namespace {

// This class keeps the last image drawn.
// We delete the temporary resource if it is not used for 3 seconds.
const int kTemporaryResourceDeletionDelay = 3;  // Seconds;

class SyncTokenClientImpl : public VideoFrame::SyncTokenClient {
 public:
  explicit SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncTokenClientImpl() override = default;
  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    gl_->GenSyncTokenCHROMIUM(sync_token->GetData());
  }
  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }

 private:
  gpu::gles2::GLES2Interface* gl_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SyncTokenClientImpl);
};

sk_sp<SkImage> YUVGrBackendTexturesToSkImage(GrContext* gr_context,
                                             gfx::ColorSpace video_color_space,
                                             VideoPixelFormat video_format,
                                             GrBackendTexture* textures) {
  // TODO(hubbe): This should really default to rec709.
  // https://crbug.com/828599
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_color_space.ToSkYUVColorSpace(&color_space);

  switch (video_format) {
    case PIXEL_FORMAT_NV12:
      return SkImage::MakeFromNV12TexturesCopy(
          gr_context, color_space, textures, kTopLeft_GrSurfaceOrigin);
    case PIXEL_FORMAT_I420:
      return SkImage::MakeFromYUVTexturesCopy(gr_context, color_space, textures,
                                              kTopLeft_GrSurfaceOrigin);
    default:
      NOTREACHED();
      return nullptr;
  }
}

sk_sp<SkImage> NewSkImageFromVideoFrameYUVTextures(
    const VideoFrame* video_frame,
    const Context3D& context_3d) {
  DCHECK(video_frame->HasTextures());
  // TODO: We should compare the DCHECK vs when UpdateLastImage calls this
  // function. (crbug.com/674185)
  DCHECK(video_frame->format() == PIXEL_FORMAT_I420 ||
         video_frame->format() == PIXEL_FORMAT_NV12);

  gpu::gles2::GLES2Interface* gl = context_3d.gl;
  DCHECK(gl);
  gfx::Size ya_tex_size = video_frame->coded_size();
  gfx::Size uv_tex_size((ya_tex_size.width() + 1) / 2,
                        (ya_tex_size.height() + 1) / 2);

  GrGLTextureInfo source_textures[] = {{0, 0}, {0, 0}, {0, 0}};
  GLint min_mag_filter[][2] = {{0, 0}, {0, 0}, {0, 0}};
  // TODO(bsalomon): Use GL_RGB8 once Skia supports it.
  // skbug.com/7533
  GrGLenum skia_texture_format =
      video_frame->format() == PIXEL_FORMAT_NV12 ? GL_RGBA8 : GL_R8_EXT;
  for (size_t i = 0; i < video_frame->NumTextures(); ++i) {
    // Get the texture from the mailbox and wrap it in a GrTexture.
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
           mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES ||
           mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB)
        << "Unsupported texture target " << std::hex << std::showbase
        << mailbox_holder.texture_target;
    gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
    source_textures[i].fID =
        gl->CreateAndConsumeTextureCHROMIUM(mailbox_holder.mailbox.name);
    source_textures[i].fTarget = mailbox_holder.texture_target;
    source_textures[i].fFormat = skia_texture_format;

    gl->BindTexture(mailbox_holder.texture_target, source_textures[i].fID);
    gl->GetTexParameteriv(mailbox_holder.texture_target, GL_TEXTURE_MIN_FILTER,
                          &min_mag_filter[i][0]);
    gl->GetTexParameteriv(mailbox_holder.texture_target, GL_TEXTURE_MAG_FILTER,
                          &min_mag_filter[i][1]);
    // TODO(dcastagna): avoid this copy once Skia supports native textures
    // with a GL_TEXTURE_RECTANGLE_ARB texture target.
    // crbug.com/505026
    if (mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB) {
      unsigned texture_copy = 0;
      gl->GenTextures(1, &texture_copy);
      DCHECK(texture_copy);
      gl->BindTexture(GL_TEXTURE_2D, texture_copy);
      gl->CopyTextureCHROMIUM(source_textures[i].fID, 0, GL_TEXTURE_2D,
                              texture_copy, 0, GL_RGB, GL_UNSIGNED_BYTE, false,
                              true, false);

      gl->DeleteTextures(1, &source_textures[i].fID);
      source_textures[i].fID = texture_copy;
      source_textures[i].fTarget = GL_TEXTURE_2D;
    }
  }
  GrBackendTexture textures[3] = {
      GrBackendTexture(ya_tex_size.width(), ya_tex_size.height(),
                       GrMipMapped::kNo, source_textures[0]),
      GrBackendTexture(uv_tex_size.width(), uv_tex_size.height(),
                       GrMipMapped::kNo, source_textures[1]),
      GrBackendTexture(uv_tex_size.width(), uv_tex_size.height(),
                       GrMipMapped::kNo, source_textures[2]),
  };

  sk_sp<SkImage> img = YUVGrBackendTexturesToSkImage(
      context_3d.gr_context, video_frame->ColorSpace(), video_frame->format(),
      textures);

  for (size_t i = 0; i < video_frame->NumTextures(); ++i) {
    gl->BindTexture(source_textures[i].fTarget, source_textures[i].fID);
    gl->TexParameteri(source_textures[i].fTarget, GL_TEXTURE_MIN_FILTER,
                      min_mag_filter[i][0]);
    gl->TexParameteri(source_textures[i].fTarget, GL_TEXTURE_MAG_FILTER,
                      min_mag_filter[i][1]);

    gl->DeleteTextures(1, &source_textures[i].fID);
  }
  return img;
}

// Creates a SkImage from a |video_frame| backed by native resources.
// The SkImage will take ownership of the underlying resource.
sk_sp<SkImage> NewSkImageFromVideoFrameNative(VideoFrame* video_frame,
                                              const Context3D& context_3d) {
  DCHECK(PIXEL_FORMAT_ARGB == video_frame->format() ||
         PIXEL_FORMAT_XRGB == video_frame->format() ||
         PIXEL_FORMAT_RGB24 == video_frame->format() ||
         PIXEL_FORMAT_RGB32 == video_frame->format() ||
         PIXEL_FORMAT_NV12 == video_frame->format() ||
         PIXEL_FORMAT_UYVY == video_frame->format())
      << "Format: " << (int)video_frame->format();

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
         mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
      << "Unsupported texture target " << std::hex << std::showbase
      << mailbox_holder.texture_target;

  gpu::gles2::GLES2Interface* gl = context_3d.gl;
  unsigned source_texture = 0;
  if (mailbox_holder.texture_target != GL_TEXTURE_2D) {
    // TODO(dcastagna): At the moment Skia doesn't support targets different
    // than GL_TEXTURE_2D.  Avoid this copy once
    // https://code.google.com/p/skia/issues/detail?id=3868 is addressed.
    gl->GenTextures(1, &source_texture);
    DCHECK(source_texture);
    gl->BindTexture(GL_TEXTURE_2D, source_texture);
    PaintCanvasVideoRenderer::CopyVideoFrameSingleTextureToGLTexture(
        gl, video_frame, GL_TEXTURE_2D, source_texture, GL_RGBA, GL_RGBA,
        GL_UNSIGNED_BYTE, 0, true, false);
  } else {
    gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
    source_texture =
        gl->CreateAndConsumeTextureCHROMIUM(mailbox_holder.mailbox.name);
  }
  GrGLTextureInfo source_texture_info;
  source_texture_info.fID = source_texture;
  source_texture_info.fTarget = GL_TEXTURE_2D;
  // TODO(bsalomon): GrGLTextureInfo::fFormat and SkColorType passed to SkImage
  // factory should reflect video_frame->format(). Update once Skia supports
  // GL_RGB.
  // skbug.com/7533
  source_texture_info.fFormat = GL_RGBA8_OES;
  GrBackendTexture source_backend_texture(
      video_frame->coded_size().width(), video_frame->coded_size().height(),
      GrMipMapped::kNo, source_texture_info);
  return SkImage::MakeFromAdoptedTexture(
      context_3d.gr_context, source_backend_texture, kTopLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType);
}

void VideoFrameCopyTextureOrSubTexture(gpu::gles2::GLES2Interface* gl,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       GLuint source_texture,
                                       unsigned int target,
                                       unsigned int texture,
                                       unsigned int internal_format,
                                       unsigned int format,
                                       unsigned int type,
                                       int level,
                                       bool premultiply_alpha,
                                       bool flip_y) {
  // The video is stored in a unmultiplied format, so premultiply if necessary.
  // Application itself needs to take care of setting the right |flip_y|
  // value down to get the expected result.
  // "flip_y == true" means to reverse the video orientation while
  // "flip_y == false" means to keep the intrinsic orientation.
  if (visible_rect != gfx::Rect(coded_size)) {
    // Must reallocate the destination texture and copy only a sub-portion.

    // There should always be enough data in the source texture to
    // cover this copy.
    DCHECK_LE(visible_rect.width(), coded_size.width());
    DCHECK_LE(visible_rect.height(), coded_size.height());

    gl->BindTexture(target, texture);
    gl->TexImage2D(target, level, internal_format, visible_rect.width(),
                   visible_rect.height(), 0, format, type, nullptr);
    gl->CopySubTextureCHROMIUM(source_texture, 0, target, texture, level, 0, 0,
                               visible_rect.x(), visible_rect.y(),
                               visible_rect.width(), visible_rect.height(),
                               flip_y, premultiply_alpha, false);

  } else {
    gl->CopyTextureCHROMIUM(source_texture, 0, target, texture, level,
                            internal_format, type, flip_y, premultiply_alpha,
                            false);
  }
}

void OnQueryDone(scoped_refptr<VideoFrame> video_frame,
                 gpu::gles2::GLES2Interface* gl,
                 unsigned query_id) {
  gl->DeleteQueriesEXT(1, &query_id);
  // |video_frame| is dropped here.
}

void SynchronizeVideoFrameRead(scoped_refptr<VideoFrame> video_frame,
                               gpu::gles2::GLES2Interface* gl,
                               gpu::ContextSupport* context_support) {
  DCHECK(gl);
  SyncTokenClientImpl client(gl);
  video_frame->UpdateReleaseSyncToken(&client);

  if (video_frame->metadata()->IsTrue(
          VideoFrameMetadata::READ_LOCK_FENCES_ENABLED)) {
    // |video_frame| must be kept alive during read operations.
    DCHECK(context_support);
    unsigned query_id = 0;
    gl->GenQueriesEXT(1, &query_id);
    DCHECK(query_id);
    gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, query_id);
    gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
    context_support->SignalQuery(
        query_id, base::BindOnce(&OnQueryDone, video_frame, gl, query_id));
  }
}

}  // anonymous namespace

// Generates an RGB image from a VideoFrame. Convert YUV to RGB plain on GPU.
class VideoImageGenerator : public cc::PaintImageGenerator {
 public:
  VideoImageGenerator(const scoped_refptr<VideoFrame>& frame)
      : cc::PaintImageGenerator(
            SkImageInfo::MakeN32Premul(frame->visible_rect().width(),
                                       frame->visible_rect().height())),
        frame_(frame) {
    DCHECK(!frame_->HasTextures());
  }
  ~VideoImageGenerator() override = default;

  sk_sp<SkData> GetEncodedData() const override { return nullptr; }

  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t row_bytes,
                 size_t frame_index,
                 cc::PaintImage::GeneratorClientId client_id,
                 uint32_t lazy_pixel_ref) override {
    DCHECK_EQ(frame_index, 0u);

    // If skia couldn't do the YUV conversion on GPU, we will on CPU.
    PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(frame_.get(), pixels,
                                                           row_bytes);
    return true;
  }

  bool QueryYUVA8(SkYUVASizeInfo* sizeInfo,
                  SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
                  SkYUVColorSpace* color_space) const override {
    if (!media::IsYuvPlanar(frame_->format()) ||
        // TODO(rileya): Skia currently doesn't support YUVA conversion. Remove
        // this case once it does. As-is we will fall back on the pure-software
        // path in this case.
        frame_->format() == PIXEL_FORMAT_I420A) {
      return false;
    }

    if (color_space) {
      if (!frame_->ColorSpace().ToSkYUVColorSpace(color_space)) {
        // TODO(hubbe): This really should default to rec709
        // https://crbug.com/828599
        *color_space = kRec601_SkYUVColorSpace;
      }
    }

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      const gfx::Size size =
          VideoFrame::PlaneSize(frame_->format(), plane,
                                gfx::Size(frame_->visible_rect().width(),
                                          frame_->visible_rect().height()));
      sizeInfo->fSizes[plane].set(size.width(), size.height());
      sizeInfo->fWidthBytes[plane] = size.width();
    }
    sizeInfo->fSizes[VideoFrame::kAPlane] = SkISize::MakeEmpty();
    sizeInfo->fWidthBytes[VideoFrame::kAPlane] = 0;

    indices[SkYUVAIndex::kY_Index] = {VideoFrame::kYPlane, SkColorChannel::kR};
    indices[SkYUVAIndex::kU_Index] = {VideoFrame::kUPlane, SkColorChannel::kR};
    indices[SkYUVAIndex::kV_Index] = {VideoFrame::kVPlane, SkColorChannel::kR};
    indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kR};

    return true;
  }

  bool GetYUVA8Planes(const SkYUVASizeInfo& sizeInfo,
                      const SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
                      void* planes[4],
                      size_t frame_index,
                      uint32_t lazy_pixel_ref) override {
    DCHECK_EQ(frame_index, 0u);

    media::VideoPixelFormat format = frame_->format();
    DCHECK(media::IsYuvPlanar(format) && format != PIXEL_FORMAT_I420A);

    for (int i = 0; i <= VideoFrame::kVPlane; ++i) {
      if (sizeInfo.fSizes[i].isEmpty() || !sizeInfo.fWidthBytes[i]) {
        return false;
      }
    }
    if (!sizeInfo.fSizes[VideoFrame::kAPlane].isEmpty() ||
        sizeInfo.fWidthBytes[VideoFrame::kAPlane]) {
      return false;
    }
    int numPlanes;
    if (!SkYUVAIndex::AreValidIndices(indices, &numPlanes) || numPlanes != 3) {
      return false;
    }

    for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane;
         ++plane) {
      const gfx::Size size =
          VideoFrame::PlaneSize(frame_->format(), plane,
                                gfx::Size(frame_->visible_rect().width(),
                                          frame_->visible_rect().height()));
      if (size.width() != sizeInfo.fSizes[plane].width() ||
          size.height() != sizeInfo.fSizes[plane].height()) {
        return false;
      }

      size_t offset;
      const int y_shift =
          (frame_->format() == media::PIXEL_FORMAT_I422) ? 0 : 1;
      if (plane == VideoFrame::kYPlane) {
        offset =
            (frame_->stride(VideoFrame::kYPlane) * frame_->visible_rect().y()) +
            frame_->visible_rect().x();
      } else {
        offset = (frame_->stride(VideoFrame::kUPlane) *
                  (frame_->visible_rect().y() >> y_shift)) +
                 (frame_->visible_rect().x() >> 1);
      }

      // Copy the frame to the supplied memory.
      // TODO: Find a way (API change?) to avoid this copy.
      char* out_line = static_cast<char*>(planes[plane]);
      int out_line_stride = sizeInfo.fWidthBytes[plane];
      uint8_t* in_line = frame_->data(plane) + offset;
      int in_line_stride = frame_->stride(plane);
      int plane_height = sizeInfo.fSizes[plane].height();
      if (in_line_stride == out_line_stride) {
        memcpy(out_line, in_line, plane_height * in_line_stride);
      } else {
        // Different line padding so need to copy one line at a time.
        int bytes_to_copy_per_line =
            out_line_stride < in_line_stride ? out_line_stride : in_line_stride;
        for (int line_no = 0; line_no < plane_height; line_no++) {
          memcpy(out_line, in_line, bytes_to_copy_per_line);
          in_line += in_line_stride;
          out_line += out_line_stride;
        }
      }
    }
    return true;
  }

 private:
  scoped_refptr<VideoFrame> frame_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoImageGenerator);
};

PaintCanvasVideoRenderer::PaintCanvasVideoRenderer()
    : last_image_deleting_timer_(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kTemporaryResourceDeletionDelay),
          this,
          &PaintCanvasVideoRenderer::ResetCache),
      renderer_stable_id_(cc::PaintImage::GetNextId()) {}

PaintCanvasVideoRenderer::~PaintCanvasVideoRenderer() {
  ResetCache();
}

void PaintCanvasVideoRenderer::Paint(
    const scoped_refptr<VideoFrame>& video_frame,
    cc::PaintCanvas* canvas,
    const gfx::RectF& dest_rect,
    cc::PaintFlags& flags,
    VideoRotation video_rotation,
    const Context3D& context_3d,
    gpu::ContextSupport* context_support) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (flags.getAlpha() == 0) {
    return;
  }

  SkRect dest;
  dest.set(dest_rect.x(), dest_rect.y(), dest_rect.right(), dest_rect.bottom());

  // Paint black rectangle if there isn't a frame available or the
  // frame has an unexpected format.
  if (!video_frame.get() || video_frame->natural_size().IsEmpty() ||
      !(media::IsYuvPlanar(video_frame->format()) ||
        video_frame->format() == media::PIXEL_FORMAT_Y16 ||
        video_frame->HasTextures())) {
    cc::PaintFlags black_with_alpha_flags;
    black_with_alpha_flags.setAlpha(flags.getAlpha());
    canvas->drawRect(dest, black_with_alpha_flags);
    canvas->flush();
    return;
  }

  gpu::gles2::GLES2Interface* gl = context_3d.gl;
  if (!UpdateLastImage(video_frame, context_3d))
    return;

  cc::PaintFlags video_flags;
  video_flags.setAlpha(flags.getAlpha());
  video_flags.setBlendMode(flags.getBlendMode());
  video_flags.setFilterQuality(flags.getFilterQuality());

  const bool need_rotation = video_rotation != VIDEO_ROTATION_0;
  const bool need_scaling =
      dest_rect.size() != gfx::SizeF(last_image_.width(), last_image_.height());
  const bool need_translation = !dest_rect.origin().IsOrigin();
  bool need_transform = need_rotation || need_scaling || need_translation;
  if (need_transform) {
    canvas->save();
    canvas->translate(
        SkFloatToScalar(dest_rect.x() + (dest_rect.width() * 0.5f)),
        SkFloatToScalar(dest_rect.y() + (dest_rect.height() * 0.5f)));
    SkScalar angle = SkFloatToScalar(0.0f);
    switch (video_rotation) {
      case VIDEO_ROTATION_0:
        break;
      case VIDEO_ROTATION_90:
        angle = SkFloatToScalar(90.0f);
        break;
      case VIDEO_ROTATION_180:
        angle = SkFloatToScalar(180.0f);
        break;
      case VIDEO_ROTATION_270:
        angle = SkFloatToScalar(270.0f);
        break;
    }
    canvas->rotate(angle);

    gfx::SizeF rotated_dest_size = dest_rect.size();
    if (video_rotation == VIDEO_ROTATION_90 ||
        video_rotation == VIDEO_ROTATION_270) {
      rotated_dest_size =
          gfx::SizeF(rotated_dest_size.height(), rotated_dest_size.width());
    }
    canvas->scale(
        SkFloatToScalar(rotated_dest_size.width() / last_image_.width()),
        SkFloatToScalar(rotated_dest_size.height() / last_image_.height()));
    canvas->translate(-SkFloatToScalar(last_image_.width() * 0.5f),
                      -SkFloatToScalar(last_image_.height() * 0.5f));
  }

  // This is a workaround for crbug.com/524717. A texture backed image is not
  // safe to access on another thread or GL context. So if we're drawing into a
  // recording canvas we read the texture back into CPU memory and record that
  // sw image into the SkPicture. The long term solution is for Skia to provide
  // a SkPicture filter that makes a picture safe for multiple CPU raster
  // threads. (skbug.com/4321).
  cc::PaintImage image = last_image_;
  if (canvas->imageInfo().colorType() == kUnknown_SkColorType) {
    sk_sp<SkImage> non_texture_image =
        last_image_.GetSkImage()->makeNonTextureImage();
    image =
        cc::PaintImageBuilder::WithProperties(last_image_)
            .set_image(std::move(non_texture_image), last_image_.content_id())
            .TakePaintImage();
  }
  canvas->drawImage(image, 0, 0, &video_flags);

  if (need_transform)
    canvas->restore();
  // Make sure to flush so we can remove the videoframe from the generator.
  canvas->flush();

  if (video_frame->HasTextures()) {
    // Synchronize |video_frame| with the read operations in UpdateLastImage(),
    // which are triggered by canvas->flush().
    SynchronizeVideoFrameRead(video_frame, gl, context_support);
  }
}

void PaintCanvasVideoRenderer::Copy(
    const scoped_refptr<VideoFrame>& video_frame,
    cc::PaintCanvas* canvas,
    const Context3D& context_3d,
    gpu::ContextSupport* context_support) {
  cc::PaintFlags flags;
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setFilterQuality(kLow_SkFilterQuality);
  Paint(video_frame, canvas,
        gfx::RectF(gfx::SizeF(video_frame->visible_rect().size())), flags,
        media::VIDEO_ROTATION_0, context_3d, context_support);
}

namespace {

// libyuv doesn't support all 9-, 10- nor 12-bit pixel formats yet. This
// function creates a regular 8-bit video frame which we can give to libyuv.
scoped_refptr<VideoFrame> DownShiftHighbitVideoFrame(
    const VideoFrame* video_frame) {
  VideoPixelFormat format;
  switch (video_frame->format()) {
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV420P9:
      format = PIXEL_FORMAT_I420;
      break;

    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV422P9:
      format = PIXEL_FORMAT_I422;
      break;

    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV444P9:
      format = PIXEL_FORMAT_I444;
      break;

    default:
      NOTREACHED();
      return nullptr;
  }
  const int shift = video_frame->BitDepth() - 8;
  scoped_refptr<VideoFrame> ret = VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());

  ret->set_color_space(video_frame->ColorSpace());
  // Copy all metadata.
  // (May be enough to copy color space)
  ret->metadata()->MergeMetadataFrom(video_frame->metadata());

  for (int plane = VideoFrame::kYPlane; plane <= VideoFrame::kVPlane; ++plane) {
    int width = ret->row_bytes(plane);
    const uint16_t* src =
        reinterpret_cast<const uint16_t*>(video_frame->data(plane));
    uint8_t* dst = ret->data(plane);
    for (int row = 0; row < video_frame->rows(plane); row++) {
      for (int x = 0; x < width; x++) {
        dst[x] = src[x] >> shift;
      }
      src += video_frame->stride(plane) / 2;
      dst += ret->stride(plane);
    }
  }
  return ret;
}

// Converts 16-bit data to |out| buffer of specified GL |type|.
// When the |format| is RGBA, the converted value is fed as luminance.
void FlipAndConvertY16(const VideoFrame* video_frame,
                       uint8_t* out,
                       unsigned format,
                       unsigned type,
                       bool flip_y,
                       size_t output_row_bytes) {
  const uint8_t* row_head = video_frame->visible_data(0);
  const size_t stride = video_frame->stride(0);
  const int height = video_frame->visible_rect().height();
  for (int i = 0; i < height; ++i, row_head += stride) {
    uint8_t* out_row_head = flip_y ? out + output_row_bytes * (height - i - 1)
                                   : out + output_row_bytes * i;
    const uint16_t* row = reinterpret_cast<const uint16_t*>(row_head);
    const uint16_t* row_end = row + video_frame->visible_rect().width();
    if (type == GL_FLOAT) {
      float* out_row = reinterpret_cast<float*>(out_row_head);
      if (format == GL_RGBA) {
        while (row < row_end) {
          float gray_value = *row++ / 65535.f;
          *out_row++ = gray_value;
          *out_row++ = gray_value;
          *out_row++ = gray_value;
          *out_row++ = 1.0f;
        }
        continue;
      } else if (format == GL_RED) {
        while (row < row_end)
          *out_row++ = *row++ / 65535.f;
        continue;
      }
      // For other formats, hit NOTREACHED below.
    } else if (type == GL_UNSIGNED_BYTE) {
      // We take the upper 8 bits of 16-bit data and convert it as luminance to
      // ARGB.  We loose the precision here, but it is important not to render
      // Y16 as RG_88.  To get the full precision use float textures with WebGL1
      // and e.g. R16UI or R32F textures with WebGL2.
      DCHECK_EQ(static_cast<unsigned>(GL_RGBA), format);
      uint32_t* rgba = reinterpret_cast<uint32_t*>(out_row_head);
      while (row < row_end) {
        uint32_t gray_value = *row++ >> 8;
        *rgba++ = SkColorSetRGB(gray_value, gray_value, gray_value);
      }
      continue;
    }
    NOTREACHED() << "Unsupported Y16 conversion for format: 0x" << std::hex
                 << format << " and type: 0x" << std::hex << type;
  }
}

// Common functionality of PaintCanvasVideoRenderer's TexImage2D and
// TexSubImage2D. Allocates a buffer required for conversion and converts
// |frame| content to desired |format|. Returns true if calling glTex(Sub)Image
// is supported for provided |frame| format and parameters.
bool TexImageHelper(VideoFrame* frame,
                    unsigned format,
                    unsigned type,
                    bool flip_y,
                    scoped_refptr<DataBuffer>* temp_buffer) {
  unsigned output_bytes_per_pixel = 0;
  switch (frame->format()) {
    case PIXEL_FORMAT_Y16:
      // Converting single component unsigned short here to FLOAT luminance.
      switch (format) {
        case GL_RGBA:
          if (type == GL_FLOAT) {
            output_bytes_per_pixel = 4 * sizeof(GLfloat);
            break;
          }
          return false;
        case GL_RED:
          if (type == GL_FLOAT) {
            output_bytes_per_pixel = sizeof(GLfloat);
            break;
          }
          return false;
        default:
          return false;
      }
      break;
    default:
      return false;
  }

  size_t output_row_bytes =
      frame->visible_rect().width() * output_bytes_per_pixel;
  *temp_buffer =
      new DataBuffer(output_row_bytes * frame->visible_rect().height());
  FlipAndConvertY16(frame, (*temp_buffer)->writable_data(), format, type,
                    flip_y, output_row_bytes);
  return true;
}

// Upload the |frame| data to temporary texture of |temp_format|,
// |temp_internalformat| and |temp_type| and then copy intermediate texture
// subimage to destination |texture|. The destination |texture| is bound to the
// |target| before the call.
void TextureSubImageUsingIntermediate(unsigned target,
                                      unsigned texture,
                                      gpu::gles2::GLES2Interface* gl,
                                      VideoFrame* frame,
                                      int temp_internalformat,
                                      unsigned temp_format,
                                      unsigned temp_type,
                                      int level,
                                      int xoffset,
                                      int yoffset,
                                      bool flip_y,
                                      bool premultiply_alpha) {
  unsigned temp_texture = 0;
  gl->GenTextures(1, &temp_texture);
  gl->BindTexture(target, temp_texture);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl->TexImage2D(target, 0, temp_internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, temp_format, temp_type,
                 frame->visible_data(0));
  gl->BindTexture(target, texture);
  gl->CopySubTextureCHROMIUM(temp_texture, 0, target, texture, level, 0, 0,
                             xoffset, yoffset, frame->visible_rect().width(),
                             frame->visible_rect().height(), flip_y,
                             premultiply_alpha, false);
  gl->DeleteTextures(1, &temp_texture);
}

}  // anonymous namespace

// static
void PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
    const VideoFrame* video_frame,
    void* rgb_pixels,
    size_t row_bytes) {
  if (!video_frame->IsMappable()) {
    NOTREACHED() << "Cannot extract pixels from non-CPU frame formats.";
    return;
  }

  // TODO(hubbe): This should really default to the rec709 colorspace.
  // https://crbug.com/828599
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_frame->ColorSpace().ToSkYUVColorSpace(&color_space);

  switch (video_frame->format()) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      switch (color_space) {
        case kJPEG_SkYUVColorSpace:
          LIBYUV_J420_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                              video_frame->stride(VideoFrame::kYPlane),
                              video_frame->visible_data(VideoFrame::kUPlane),
                              video_frame->stride(VideoFrame::kUPlane),
                              video_frame->visible_data(VideoFrame::kVPlane),
                              video_frame->stride(VideoFrame::kVPlane),
                              static_cast<uint8_t*>(rgb_pixels), row_bytes,
                              video_frame->visible_rect().width(),
                              video_frame->visible_rect().height());
          break;
        case kRec709_SkYUVColorSpace:
          LIBYUV_H420_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                              video_frame->stride(VideoFrame::kYPlane),
                              video_frame->visible_data(VideoFrame::kUPlane),
                              video_frame->stride(VideoFrame::kUPlane),
                              video_frame->visible_data(VideoFrame::kVPlane),
                              video_frame->stride(VideoFrame::kVPlane),
                              static_cast<uint8_t*>(rgb_pixels), row_bytes,
                              video_frame->visible_rect().width(),
                              video_frame->visible_rect().height());
          break;
        case kRec601_SkYUVColorSpace:
          LIBYUV_I420_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                              video_frame->stride(VideoFrame::kYPlane),
                              video_frame->visible_data(VideoFrame::kUPlane),
                              video_frame->stride(VideoFrame::kUPlane),
                              video_frame->visible_data(VideoFrame::kVPlane),
                              video_frame->stride(VideoFrame::kVPlane),
                              static_cast<uint8_t*>(rgb_pixels), row_bytes,
                              video_frame->visible_rect().width(),
                              video_frame->visible_rect().height());
          break;
      }
      break;
    case PIXEL_FORMAT_I422:
      LIBYUV_I422_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                          video_frame->stride(VideoFrame::kYPlane),
                          video_frame->visible_data(VideoFrame::kUPlane),
                          video_frame->stride(VideoFrame::kUPlane),
                          video_frame->visible_data(VideoFrame::kVPlane),
                          video_frame->stride(VideoFrame::kVPlane),
                          static_cast<uint8_t*>(rgb_pixels), row_bytes,
                          video_frame->visible_rect().width(),
                          video_frame->visible_rect().height());
      break;

    case PIXEL_FORMAT_I420A:
      LIBYUV_I420ALPHA_TO_ARGB(
          video_frame->visible_data(VideoFrame::kYPlane),
          video_frame->stride(VideoFrame::kYPlane),
          video_frame->visible_data(VideoFrame::kUPlane),
          video_frame->stride(VideoFrame::kUPlane),
          video_frame->visible_data(VideoFrame::kVPlane),
          video_frame->stride(VideoFrame::kVPlane),
          video_frame->visible_data(VideoFrame::kAPlane),
          video_frame->stride(VideoFrame::kAPlane),
          static_cast<uint8_t*>(rgb_pixels), row_bytes,
          video_frame->visible_rect().width(),
          video_frame->visible_rect().height(),
          1);  // 1 = enable RGB premultiplication by Alpha.
      break;

    case PIXEL_FORMAT_I444:
      LIBYUV_I444_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                          video_frame->stride(VideoFrame::kYPlane),
                          video_frame->visible_data(VideoFrame::kUPlane),
                          video_frame->stride(VideoFrame::kUPlane),
                          video_frame->visible_data(VideoFrame::kVPlane),
                          video_frame->stride(VideoFrame::kVPlane),
                          static_cast<uint8_t*>(rgb_pixels), row_bytes,
                          video_frame->visible_rect().width(),
                          video_frame->visible_rect().height());
      break;

    case PIXEL_FORMAT_YUV420P10:
      if (color_space == kRec709_SkYUVColorSpace) {
        LIBYUV_H010_TO_ARGB(reinterpret_cast<const uint16_t*>(
                                video_frame->visible_data(VideoFrame::kYPlane)),
                            video_frame->stride(VideoFrame::kYPlane) / 2,
                            reinterpret_cast<const uint16_t*>(
                                video_frame->visible_data(VideoFrame::kUPlane)),
                            video_frame->stride(VideoFrame::kUPlane) / 2,
                            reinterpret_cast<const uint16_t*>(
                                video_frame->visible_data(VideoFrame::kVPlane)),
                            video_frame->stride(VideoFrame::kVPlane) / 2,
                            static_cast<uint8_t*>(rgb_pixels), row_bytes,
                            video_frame->visible_rect().width(),
                            video_frame->visible_rect().height());
      } else {
        LIBYUV_I010_TO_ARGB(reinterpret_cast<const uint16_t*>(
                                video_frame->visible_data(VideoFrame::kYPlane)),
                            video_frame->stride(VideoFrame::kYPlane) / 2,
                            reinterpret_cast<const uint16_t*>(
                                video_frame->visible_data(VideoFrame::kUPlane)),
                            video_frame->stride(VideoFrame::kUPlane) / 2,
                            reinterpret_cast<const uint16_t*>(
                                video_frame->visible_data(VideoFrame::kVPlane)),
                            video_frame->stride(VideoFrame::kVPlane) / 2,
                            static_cast<uint8_t*>(rgb_pixels), row_bytes,
                            video_frame->visible_rect().width(),
                            video_frame->visible_rect().height());
      }
      break;

    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12: {
      scoped_refptr<VideoFrame> temporary_frame =
          DownShiftHighbitVideoFrame(video_frame);
      ConvertVideoFrameToRGBPixels(temporary_frame.get(), rgb_pixels,
                                   row_bytes);
      break;
    }

    case PIXEL_FORMAT_Y16:
      // Since it is grayscale conversion, we disregard SK_PMCOLOR_BYTE_ORDER
      // and always use GL_RGBA.
      FlipAndConvertY16(video_frame, static_cast<uint8_t*>(rgb_pixels), GL_RGBA,
                        GL_UNSIGNED_BYTE, false /*flip_y*/, row_bytes);
      break;

    case PIXEL_FORMAT_NV12:
      LIBYUV_NV12_TO_ARGB(video_frame->visible_data(VideoFrame::kYPlane),
                          video_frame->stride(VideoFrame::kYPlane),
                          video_frame->visible_data(VideoFrame::kUVPlane),
                          video_frame->stride(VideoFrame::kUVPlane),
                          static_cast<uint8_t*>(rgb_pixels), row_bytes,
                          video_frame->visible_rect().width(),
                          video_frame->visible_rect().height());
      break;

    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_MT21:
    case PIXEL_FORMAT_ABGR:
    case PIXEL_FORMAT_XBGR:
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED() << "Only YUV formats and Y16 are supported, got: "
                   << media::VideoPixelFormatToString(video_frame->format());
  }
}

// static
void PaintCanvasVideoRenderer::CopyVideoFrameSingleTextureToGLTexture(
    gpu::gles2::GLES2Interface* gl,
    VideoFrame* video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());

  const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(0);
  DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
         mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB ||
         mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES)
      << mailbox_holder.texture_target;

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  uint32_t source_texture =
      gl->CreateAndConsumeTextureCHROMIUM(mailbox_holder.mailbox.name);
  VideoFrameCopyTextureOrSubTexture(gl, video_frame->coded_size(),
                                    video_frame->visible_rect(), source_texture,
                                    target, texture, internal_format, format,
                                    type, level, premultiply_alpha, flip_y);
  gl->DeleteTextures(1, &source_texture);
  gl->ShallowFlushCHROMIUM();
  // The caller must call SynchronizeVideoFrameRead() after this operation, but
  // we can't do that because we don't have the ContextSupport.
}

bool PaintCanvasVideoRenderer::CopyVideoFrameTexturesToGLTexture(
    const Context3D& context_3d,
    gpu::ContextSupport* context_support,
    gpu::gles2::GLES2Interface* destination_gl,
    const scoped_refptr<VideoFrame>& video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    bool premultiply_alpha,
    bool flip_y) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(video_frame);
  DCHECK(video_frame->HasTextures());
  if (video_frame->NumTextures() > 1 ||
      video_frame->metadata()->IsTrue(
          VideoFrameMetadata::READ_LOCK_FENCES_ENABLED)) {
    if (!context_3d.gr_context)
      return false;
    if (!UpdateLastImage(video_frame, context_3d))
      return false;

    GrBackendTexture backend_texture =
        last_image_.GetSkImage()->getBackendTexture(true);
    if (!backend_texture.isValid())
      return false;
    GrGLTextureInfo texture_info;
    if (!backend_texture.getGLTextureInfo(&texture_info))
      return false;

    // Synchronize |video_frame| with the read operations in UpdateLastImage(),
    // which are triggered by getBackendTexture().
    gpu::gles2::GLES2Interface* canvas_gl = context_3d.gl;
    SynchronizeVideoFrameRead(video_frame, canvas_gl, context_support);

    gpu::MailboxHolder mailbox_holder;
    mailbox_holder.texture_target = texture_info.fTarget;
    canvas_gl->ProduceTextureDirectCHROMIUM(texture_info.fID,
                                            mailbox_holder.mailbox.name);

    // Wait for mailbox creation on canvas context before consuming it and
    // copying from it on the consumer context.
    canvas_gl->GenUnverifiedSyncTokenCHROMIUM(
        mailbox_holder.sync_token.GetData());

    destination_gl->WaitSyncTokenCHROMIUM(
        mailbox_holder.sync_token.GetConstData());
    uint32_t intermediate_texture =
        destination_gl->CreateAndConsumeTextureCHROMIUM(
            mailbox_holder.mailbox.name);

    destination_gl->CopyTextureCHROMIUM(intermediate_texture, 0, target,
                                        texture, level, internal_format, type,
                                        flip_y, premultiply_alpha, false);

    destination_gl->DeleteTextures(1, &intermediate_texture);

    // Wait for destination context to consume mailbox before deleting it in
    // canvas context.
    gpu::SyncToken dest_sync_token;
    destination_gl->GenUnverifiedSyncTokenCHROMIUM(dest_sync_token.GetData());
    canvas_gl->WaitSyncTokenCHROMIUM(dest_sync_token.GetConstData());
  } else {
    CopyVideoFrameSingleTextureToGLTexture(
        destination_gl, video_frame.get(), target, texture, internal_format,
        format, type, level, premultiply_alpha, flip_y);
    SynchronizeVideoFrameRead(video_frame, destination_gl, nullptr);
  }

  return true;
}

bool PaintCanvasVideoRenderer::CopyVideoFrameYUVDataToGLTexture(
    const Context3D& context_3d,
    gpu::gles2::GLES2Interface* destination_gl,
    const scoped_refptr<VideoFrame>& video_frame,
    unsigned int target,
    unsigned int texture,
    unsigned int internal_format,
    unsigned int format,
    unsigned int type,
    int level,
    bool premultiply_alpha,
    bool flip_y) {
  if (!context_3d.gr_context) {
    return false;
  }

  if (!video_frame || !video_frame->IsMappable()) {
    return false;
  }

  if (video_frame->format() != media::PIXEL_FORMAT_I420) {
    return false;
  }
  // Could handle NV12 here as well. See NewSkImageFromVideoFrameYUVTextures.

  static constexpr size_t kNumPlanes = 3;
  DCHECK_EQ(video_frame->NumPlanes(video_frame->format()), kNumPlanes);
  // Y,U,V GPU-side SkImages. (These must outlive the yuv_textures).
  sk_sp<SkImage> yuv_images[kNumPlanes]{};
  // Y,U,V GPU textures from those SkImages.
  // (A GrBackendTexture is a non-owned reference to the SkImage's texture.)
  GrBackendTexture yuv_textures[kNumPlanes]{};

  // Upload the whole coded image area (not visible rect).
  gfx::Size y_tex_size = video_frame->coded_size();
  gfx::Size uv_tex_size((y_tex_size.width() + 1) / 2,
                        (y_tex_size.height() + 1) / 2);

  for (size_t plane = 0; plane < kNumPlanes; ++plane) {
    const uint8_t* data = video_frame->data(plane);
    int plane_stride = video_frame->stride(plane);

    bool is_y_plane = plane == media::VideoFrame::kYPlane;
    gfx::Size tex_size = is_y_plane ? y_tex_size : uv_tex_size;
    int data_size = plane_stride * (tex_size.height() - 1) + tex_size.width();

    // Create a CPU-side SkImage from the channel.
    sk_sp<SkData> sk_data = SkData::MakeWithoutCopy(data, data_size);
    DCHECK(sk_data);
    SkImageInfo image_info =
        SkImageInfo::Make(tex_size.width(), tex_size.height(),
                          kGray_8_SkColorType, kUnknown_SkAlphaType);
    sk_sp<SkImage> plane_image_cpu =
        SkImage::MakeRasterData(image_info, sk_data, plane_stride);
    DCHECK(plane_image_cpu);

    // Upload the CPU-side SkImage into a GPU-side SkImage.
    // (Note the original video_frame data is no longer used after this point.)
    yuv_images[plane] =
        plane_image_cpu->makeTextureImage(context_3d.gr_context, nullptr);
    DCHECK(yuv_images[plane]);

    // Extract the backend texture from the GPU-side image.
    yuv_textures[plane] = yuv_images[plane]->getBackendTexture(false);
  }

  // Decode 3 GPU-side Y,U,V SkImages into a GPU-side RGB SkImage.
  sk_sp<SkImage> yuv_image = YUVGrBackendTexturesToSkImage(
      context_3d.gr_context, video_frame->ColorSpace(), video_frame->format(),
      yuv_textures);
  if (!yuv_image) {
    return false;
  }

  GrGLTextureInfo src_texture_info{};
  yuv_image->getBackendTexture(false).getGLTextureInfo(&src_texture_info);

  gpu::gles2::GLES2Interface* source_gl = context_3d.gl;
  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.texture_target = src_texture_info.fTarget;
  source_gl->ProduceTextureDirectCHROMIUM(src_texture_info.fID,
                                          mailbox_holder.mailbox.name);

  // Wait for mailbox creation on source context before consuming it and
  // copying from it on the consumer context.
  source_gl->GenUnverifiedSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetData());

  destination_gl->WaitSyncTokenCHROMIUM(
      mailbox_holder.sync_token.GetConstData());
  uint32_t intermediate_texture =
      destination_gl->CreateAndConsumeTextureCHROMIUM(
          mailbox_holder.mailbox.name);
  VideoFrameCopyTextureOrSubTexture(
      destination_gl, video_frame->coded_size(), video_frame->visible_rect(),
      intermediate_texture, target, texture, internal_format, format, type,
      level, premultiply_alpha, flip_y);
  destination_gl->DeleteTextures(1, &intermediate_texture);

  // Wait for destination context to consume mailbox before deleting it in
  // source context.
  gpu::SyncToken dest_sync_token;
  destination_gl->GenUnverifiedSyncTokenCHROMIUM(dest_sync_token.GetData());
  source_gl->WaitSyncTokenCHROMIUM(dest_sync_token.GetConstData());

  // video_frame->UpdateReleaseSyncToken is not necessary since the video frame
  // data we used was CPU-side (IsMappable) to begin with. If there were any
  // textures, we didn't use them.

  // The temporary SkImages should be automatically cleaned up here.
  return true;
}

bool PaintCanvasVideoRenderer::TexImage2D(
    unsigned target,
    unsigned texture,
    gpu::gles2::GLES2Interface* gl,
    const gpu::Capabilities& gpu_capabilities,
    VideoFrame* frame,
    int level,
    int internalformat,
    unsigned format,
    unsigned type,
    bool flip_y,
    bool premultiply_alpha) {
  DCHECK(frame);
  DCHECK(!frame->HasTextures());

  // Note: CopyTextureCHROMIUM uses mediump for color computation. Don't use
  // it if the precision would lead to data loss when converting 16-bit
  // normalized to float. medium_float.precision > 15 means that the approach
  // below is not used on Android, where the extension EXT_texture_norm16 is
  // not widely supported. It is used on Windows, Linux and OSX.
  // Android support is not required for now because Tango depth camera already
  // provides floating point data (projected point cloud). See crbug.com/674440.
  if (gpu_capabilities.texture_norm16 &&
      gpu_capabilities.fragment_shader_precisions.medium_float.precision > 15 &&
      target == GL_TEXTURE_2D &&
      (type == GL_FLOAT || type == GL_UNSIGNED_BYTE)) {
    // TODO(aleksandar.stojiljkovic): Extend the approach to TexSubImage2D
    // implementation and other types. See https://crbug.com/624436.

    // Allocate the destination texture.
    gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                   frame->visible_rect().height(), 0, format, type, nullptr);
    // We use sized internal format GL_R16_EXT instead of unsized GL_RED.
    // See angleproject:1952
    TextureSubImageUsingIntermediate(target, texture, gl, frame, GL_R16_EXT,
                                     GL_RED, GL_UNSIGNED_SHORT, level, 0, 0,
                                     flip_y, premultiply_alpha);
    return true;
  }
  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, flip_y, &temp_buffer))
    return false;

  gl->TexImage2D(target, level, internalformat, frame->visible_rect().width(),
                 frame->visible_rect().height(), 0, format, type,
                 temp_buffer->data());
  return true;
}

bool PaintCanvasVideoRenderer::TexSubImage2D(unsigned target,
                                             gpu::gles2::GLES2Interface* gl,
                                             VideoFrame* frame,
                                             int level,
                                             unsigned format,
                                             unsigned type,
                                             int xoffset,
                                             int yoffset,
                                             bool flip_y,
                                             bool premultiply_alpha) {
  DCHECK(frame);
  DCHECK(!frame->HasTextures());

  scoped_refptr<DataBuffer> temp_buffer;
  if (!TexImageHelper(frame, format, type, flip_y, &temp_buffer))
    return false;

  gl->TexSubImage2D(
      target, level, xoffset, yoffset, frame->visible_rect().width(),
      frame->visible_rect().height(), format, type, temp_buffer->data());
  return true;
}

void PaintCanvasVideoRenderer::ResetCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Clear cached values.
  last_image_ = cc::PaintImage();
  last_id_.reset();
}

bool PaintCanvasVideoRenderer::UpdateLastImage(
    const scoped_refptr<VideoFrame>& video_frame,
    const Context3D& context_3d) {
  if (!last_image_ || video_frame->unique_id() != last_id_ ||
      !last_image_.GetSkImage()->getBackendTexture(true).isValid()) {
    ResetCache();

    auto paint_image_builder =
        cc::PaintImageBuilder::WithDefault()
            .set_id(renderer_stable_id_)
            .set_animation_type(cc::PaintImage::AnimationType::VIDEO)
            .set_completion_state(cc::PaintImage::CompletionState::DONE);

    // Generate a new image.
    // Note: Skia will hold onto |video_frame| via |video_generator| only when
    // |video_frame| is software.
    // Holding |video_frame| longer than this call when using GPUVideoDecoder
    // could cause problems since the pool of VideoFrames has a fixed size.
    if (video_frame->HasTextures()) {
      DCHECK(context_3d.gr_context);
      DCHECK(context_3d.gl);
      if (video_frame->NumTextures() > 1) {
        paint_image_builder.set_image(
            NewSkImageFromVideoFrameYUVTextures(video_frame.get(), context_3d),
            cc::PaintImage::GetNextContentId());
      } else {
        paint_image_builder.set_image(
            NewSkImageFromVideoFrameNative(video_frame.get(), context_3d),
            cc::PaintImage::GetNextContentId());
      }
    } else {
      paint_image_builder.set_paint_image_generator(
          sk_make_sp<VideoImageGenerator>(video_frame));
    }
    last_image_ = paint_image_builder.TakePaintImage();
    CorrectLastImageDimensions(gfx::RectToSkIRect(video_frame->visible_rect()));
    if (!last_image_)  // Couldn't create the SkImage.
      return false;
    last_id_ = video_frame->unique_id();
  }

  DCHECK(last_image_);
  last_image_deleting_timer_.Reset();
  return true;
}

void PaintCanvasVideoRenderer::CorrectLastImageDimensions(
    const SkIRect& visible_rect) {
  last_image_dimensions_for_testing_ = visible_rect.size();
  if (!last_image_)
    return;
  SkIRect bounds = SkIRect::MakeWH(last_image_.width(), last_image_.height());
  if (bounds.size() != visible_rect.size() && bounds.contains(visible_rect)) {
    last_image_ = cc::PaintImageBuilder::WithCopy(std::move(last_image_))
                      .make_subset(gfx::SkIRectToRect(visible_rect))
                      .TakePaintImage();
  }
}

SkISize PaintCanvasVideoRenderer::LastImageDimensionsForTesting() {
  return last_image_dimensions_for_testing_;
}

}  // namespace media
