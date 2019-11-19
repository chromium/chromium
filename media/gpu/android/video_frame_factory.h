// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_
#define MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_decoder.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class CodecOutputBuffer;
class CodecSurfaceBundle;
class VideoFrame;

// VideoFrameFactory creates CodecOutputBuffer backed VideoFrames. Not thread
// safe. Virtual for testing; see VideoFrameFactoryImpl.
class MEDIA_GPU_EXPORT VideoFrameFactory {
 public:
  using InitCb =
      base::RepeatingCallback<void(scoped_refptr<gpu::TextureOwner>)>;
  using OnceOutputCb = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;

  VideoFrameFactory() = default;
  virtual ~VideoFrameFactory() = default;

  // Initializes the factory and runs |init_cb| on the current thread when it's
  // complete. If initialization fails, the returned texture owner will be
  // null.
  enum class OverlayMode {
    // When using java overlays, the compositor can provide hints to the media
    // pipeline to indicate whether the video can be promoted to an overlay.
    // This indicates whether promotion hints are needed, if framework support
    // for overlay promotion is available (requires MediaCodec.setOutputSurface
    // support).
    kDontRequestPromotionHints,
    kRequestPromotionHints,

    // When using surface control, the factory should always use a TextureOwner
    // since it can directly be promoted to an overlay on a frame-by-frame
    // basis. The bits below indicate whether the media uses a secure codec.
    kSurfaceControlSecure,
    kSurfaceControlInsecure
  };
  virtual void Initialize(OverlayMode overlay_mode, InitCb init_cb) = 0;

  // Notify us about the current surface bundle that subsequent video frames
  // should use.
  virtual void SetSurfaceBundle(
      scoped_refptr<CodecSurfaceBundle> surface_bundle) = 0;

  // Creates a new VideoFrame backed by |output_buffer|.  Runs |output_cb| on
  // the calling sequence to return the frame.
  virtual void CreateVideoFrame(
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      base::TimeDelta timestamp,
      gfx::Size natural_size,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb,
      OnceOutputCb output_cb) = 0;

  // Runs |closure| on the calling sequence after all previous
  // CreateVideoFrame() calls have completed.
  virtual void RunAfterPendingVideoFrames(base::OnceClosure closure) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_VIDEO_FRAME_FACTORY_
