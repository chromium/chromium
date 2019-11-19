// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_PAINT_CANVAS_VIDEO_RENDERER_H_
#define MEDIA_RENDERERS_PAINT_CANVAS_VIDEO_RENDERER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"

namespace gfx {
class RectF;
}

namespace gpu {
struct Capabilities;

namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
}

namespace media {

// Handles rendering of VideoFrames to PaintCanvases.
class MEDIA_EXPORT PaintCanvasVideoRenderer {
 public:
  PaintCanvasVideoRenderer();
  ~PaintCanvasVideoRenderer();

  // Paints |video_frame| translated and scaled to |dest_rect| on |canvas|.
  //
  // If the format of |video_frame| is PIXEL_FORMAT_NATIVE_TEXTURE, |context_3d|
  // and |context_support| must be provided.
  //
  // If |video_frame| is nullptr or an unsupported format, |dest_rect| will be
  // painted black.
  void Paint(scoped_refptr<VideoFrame> video_frame,
             cc::PaintCanvas* canvas,
             const gfx::RectF& dest_rect,
             cc::PaintFlags& flags,
             VideoTransformation video_transformation,
             viz::ContextProvider* context_provider);

  // Paints |video_frame| scaled to its visible size on |canvas|.
  //
  // If the format of |video_frame| is PIXEL_FORMAT_NATIVE_TEXTURE, |context_3d|
  // and |context_support| must be provided.
  void Copy(scoped_refptr<VideoFrame> video_frame,
            cc::PaintCanvas* canvas,
            viz::ContextProvider* context_provider);

  // Convert the contents of |video_frame| to raw RGB pixels. |rgb_pixels|
  // should point into a buffer large enough to hold as many 32 bit RGBA pixels
  // as are in the visible_rect() area of the frame.
  static void ConvertVideoFrameToRGBPixels(const media::VideoFrame* video_frame,
                                           void* rgb_pixels,
                                           size_t row_bytes);

  // Copy the visible rect size contents of texture of |video_frame| to
  // texture |texture|. |level|, |internal_format|, |type| specify target
  // texture |texture|. The format of |video_frame| must be
  // VideoFrame::NATIVE_TEXTURE.
  static void CopyVideoFrameSingleTextureToGLTexture(
      gpu::gles2::GLES2Interface* gl,
      VideoFrame* video_frame,
      unsigned int target,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int format,
      unsigned int type,
      int level,
      bool premultiply_alpha,
      bool flip_y);

  // Copy the contents of |video_frame| to |texture| of |destination_gl|.
  //
  // The format of |video_frame| must be VideoFrame::NATIVE_TEXTURE.
  bool CopyVideoFrameTexturesToGLTexture(
      viz::ContextProvider* context_provider,
      gpu::gles2::GLES2Interface* destination_gl,
      scoped_refptr<VideoFrame> video_frame,
      unsigned int target,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int format,
      unsigned int type,
      int level,
      bool premultiply_alpha,
      bool flip_y);

  bool PrepareVideoFrameForWebGL(viz::ContextProvider* context_provider,
                                 gpu::gles2::GLES2Interface* gl,
                                 scoped_refptr<VideoFrame> video_frame,
                                 unsigned int target,
                                 unsigned int texture);

  // Copy the CPU-side YUV contents of |video_frame| to texture |texture| in
  // context |destination_gl|.
  // |level|, |internal_format|, |type| specify target texture |texture|.
  // The format of |video_frame| must be mappable.
  // |context_3d| has a GrContext that may be used during the copy.
  // CorrectLastImageDimensions() ensures that the source texture will be
  // cropped to |visible_rect|. Returns true on success.
  bool CopyVideoFrameYUVDataToGLTexture(
      viz::ContextProvider* context_provider,
      gpu::gles2::GLES2Interface* destination_gl,
      const VideoFrame& video_frame,
      unsigned int target,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int format,
      unsigned int type,
      int level,
      bool premultiply_alpha,
      bool flip_y);

  // Calls texImage2D where the texture image data source is the contents of
  // |video_frame|. Texture |texture| needs to be created and bound to |target|
  // before this call and the binding is active upon return.
  // This is an optimization of WebGL |video_frame| TexImage2D implementation
  // for specific combinations of |video_frame| and |texture| formats; e.g. if
  // |frame format| is Y16, optimizes conversion of normalized 16-bit content
  // and calls texImage2D to |texture|. |level|, |internal_format|, |format| and
  // |type| are WebGL texImage2D parameters.
  // Returns false if there is no implementation for given parameters.
  static bool TexImage2D(unsigned target,
                         unsigned texture,
                         gpu::gles2::GLES2Interface* gl,
                         const gpu::Capabilities& gpu_capabilities,
                         VideoFrame* video_frame,
                         int level,
                         int internalformat,
                         unsigned format,
                         unsigned type,
                         bool flip_y,
                         bool premultiply_alpha);

  // Calls texSubImage2D where the texture image data source is the contents of
  // |video_frame|.
  // This is an optimization of WebGL |video_frame| TexSubImage2D implementation
  // for specific combinations of |video_frame| and texture |format| and |type|;
  // e.g. if |frame format| is Y16, converts unsigned 16-bit value to target
  // |format| and calls WebGL texSubImage2D. |level|, |format|, |type|,
  // |xoffset| and |yoffset| are texSubImage2D parameters.
  // Returns false if there is no implementation for given parameters.
  static bool TexSubImage2D(unsigned target,
                            gpu::gles2::GLES2Interface* gl,
                            VideoFrame* video_frame,
                            int level,
                            unsigned format,
                            unsigned type,
                            int xoffset,
                            int yoffset,
                            bool flip_y,
                            bool premultiply_alpha);

  // In general, We hold the most recently painted frame to increase the
  // performance for the case that the same frame needs to be painted
  // repeatedly. Call this function if you are sure the most recent frame will
  // never be painted again, so we can release the resource.
  void ResetCache();

  // Used for unit test.
  gfx::Size LastImageDimensionsForTesting();

 private:
  // This structure wraps information extracted out of a VideoFrame and/or
  // constructed out of it. The various calls in PaintCanvasVideoRenderer must
  // not keep a reference to the VideoFrame so necessary data is extracted out
  // of it.
  struct Cache {
    explicit Cache(int frame_id);
    ~Cache();

    // VideoFrame::unique_id() of the videoframe used to generate the cache.
    int frame_id;

    // A PaintImage that can be used to draw into a PaintCanvas. This is sized
    // to the visible size of the VideoFrame. Its contents are generated lazily.
    cc::PaintImage paint_image;

    // The context provider used to generate |source_mailbox| and
    // |source_texture|. This is only set if the VideoFrame was texture-backed.
    scoped_refptr<viz::ContextProvider> context_provider;

    // The mailbox for the source texture. This can be either the source
    // VideoFrame's texture (if |wraps_video_frame_texture| is true) or a newly
    // allocated shared image (if |wraps_video_frame_texture| is false) if a
    // copy or conversion was necessary.
    // This is only set if the VideoFrame was texture-backed.
    gpu::Mailbox source_mailbox;

    // The texture ID created when importing |source_mailbox|.
    // This is only set if the VideoFrame was texture-backed.
    uint32_t source_texture = 0;

    // The allocated size of |source_mailbox|.
    // This is only set if the VideoFrame was texture-backed.
    gfx::Size coded_size;

    // The visible subrect of |coded_size| that represents the logical contents
    // of the frame after cropping.
    // This is only set if the VideoFrame was texture-backed.
    gfx::Rect visible_rect;

    // Whether |source_mailbox| directly points to a texture of the VideoFrame
    // (if true), or to an allocated shared image (if false).
    bool wraps_video_frame_texture = false;

    // Whether the texture pointed by |paint_image| is owned by skia or not.
    bool texture_ownership_in_skia = false;

    // Used to allow recycling of the previous shared image. This requires that
    // no external users have access to this resource via SkImage. Returns true
    // if the existing resource can be recycled.
    bool Recycle();
  };

  // Update the cache holding the most-recently-painted frame. Returns false
  // if the image couldn't be updated.
  bool UpdateLastImage(scoped_refptr<VideoFrame> video_frame,
                       viz::ContextProvider* context_provider,
                       bool allow_wrap_texture);

  bool PrepareVideoFrame(scoped_refptr<VideoFrame> video_frame,
                         viz::ContextProvider* context_provider,
                         unsigned int textureTarget,
                         unsigned int texture);

  base::Optional<Cache> cache_;

  // If |cache_| is not used for a while, it's deleted to save memory.
  base::DelayTimer cache_deleting_timer_;
  // Stable paint image id to provide to draw image calls.
  cc::PaintImage::Id renderer_stable_id_;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  struct YUVTextureCache {
    YUVTextureCache();
    ~YUVTextureCache();
    void Reset();

    // The ContextProvider that holds the texture.
    scoped_refptr<viz::ContextProvider> context_provider;

    // The size of the texture.
    gfx::Size size;

    // The shared image backing the texture.
    gpu::Mailbox mailbox;

    // The GL texture.
    uint32_t texture = 0;

    // A SyncToken after last usage, used for reusing or destroying texture and
    // shared image.
    gpu::SyncToken sync_token;
  };
  YUVTextureCache yuv_cache_;

  DISALLOW_COPY_AND_ASSIGN(PaintCanvasVideoRenderer);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_PAINT_CANVAS_VIDEO_RENDERER_H_
