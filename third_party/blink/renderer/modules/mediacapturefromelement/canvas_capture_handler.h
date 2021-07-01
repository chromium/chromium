// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_CANVAS_CAPTURE_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_CANVAS_CAPTURE_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/video_frame_pool.h"
#include "media/capture/video_capturer_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"

class SkImage;

namespace gfx {
class Size;
}  // namespace gfx

namespace blink {

class LocalFrame;
class MediaStreamComponent;
class StaticBitmapImage;
class WebGraphicsContext3DProvider;
class WebGraphicsContext3DProviderWrapper;
class WebGraphicsContext3DVideoFramePool;

// CanvasCaptureHandler acts as the link between Blink side HTMLCanvasElement
// and Chrome side VideoCapturerSource. It is responsible for handling
// SkImage instances sent from the Blink side, convert them to
// media::VideoFrame and plug them to the MediaStreamTrack.
// CanvasCaptureHandler instance is owned by a blink::CanvasDrawListener which
// is owned by a CanvasCaptureMediaStreamTrack.
// All methods are called on the same thread as construction and destruction,
// i.e. the Main Render thread. Note that a CanvasCaptureHandlerDelegate is
// used to send back frames to |io_task_runner_|, i.e. IO thread.
class MODULES_EXPORT CanvasCaptureHandler {
 public:
  ~CanvasCaptureHandler();

  // Creates a CanvasCaptureHandler instance and updates UMA histogram.
  static std::unique_ptr<CanvasCaptureHandler> CreateCanvasCaptureHandler(
      LocalFrame* frame,
      const gfx::Size& size,
      double frame_rate,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      MediaStreamComponent** component);

  void SendNewFrame(scoped_refptr<StaticBitmapImage> image,
                    base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
                        context_provider);
  bool NeedsNewFrame() const;

  // Functions called by media::VideoCapturerSource implementation.
  void StartVideoCapture(
      const media::VideoCaptureParams& params,
      const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
          new_frame_callback,
      const media::VideoCapturerSource::RunningCallback& running_callback);
  void RequestRefreshFrame();
  void StopVideoCapture();
  void SetCanDiscardAlpha(bool can_discard_alpha) {
    can_discard_alpha_ = can_discard_alpha;
  }

 private:
  // A VideoCapturerSource instance is created, which is responsible for handing
  // stop&start callbacks back to CanvasCaptureHandler. That VideoCapturerSource
  // is then plugged into a MediaStreamTrack passed as |track|, and it is owned
  // by the Blink side MediaStreamSource.
  CanvasCaptureHandler(
      LocalFrame* frame,
      const gfx::Size& size,
      double frame_rate,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      MediaStreamComponent** component);

  // Helper functions to read pixel content.
  void ReadARGBPixelsSync(scoped_refptr<StaticBitmapImage> image);
  void ReadARGBPixelsAsync(
      scoped_refptr<StaticBitmapImage> image,
      blink::WebGraphicsContext3DProvider* context_provider);
  void ReadYUVPixelsAsync(
      scoped_refptr<StaticBitmapImage> image,
      base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
          context_provider);
  void OnARGBPixelsReadAsync(scoped_refptr<StaticBitmapImage> image,
                             scoped_refptr<media::VideoFrame> temp_argb_frame,
                             base::TimeTicks this_frame_ticks,
                             GrSurfaceOrigin result_origin,
                             bool success);
  void OnYUVPixelsReadAsync(scoped_refptr<media::VideoFrame> yuv_frame,
                            base::TimeTicks this_frame_ticks,
                            bool success);
  void OnReleaseMailbox(scoped_refptr<StaticBitmapImage> image);

  scoped_refptr<media::VideoFrame> ConvertToYUVFrame(
      scoped_refptr<media::VideoFrame> argb_video_frame,
      bool flip);
  void SendFrame(scoped_refptr<media::VideoFrame> video_frame,
                 base::TimeTicks this_frame_ticks,
                 const gfx::ColorSpace& color_space);

  void AddVideoCapturerSourceToVideoTrack(
      LocalFrame* frame,
      std::unique_ptr<media::VideoCapturerSource> source,
      MediaStreamComponent** component);

  // Helper methods to increment/decrement the number of ongoing async pixel
  // readouts currently happening.
  void IncrementOngoingAsyncPixelReadouts();
  void DecrementOngoingAsyncPixelReadouts();

  // Send a refresh frame.
  void SendRefreshFrame();

  // Object that does all the work of running |new_frame_callback_|.
  // Destroyed on |frame_callback_task_runner_| after the class is destroyed.
  class CanvasCaptureHandlerDelegate;

  media::VideoCaptureFormat capture_format_;
  bool can_discard_alpha_ = false;
  bool ask_for_new_frame_;
  media::VideoFramePool frame_pool_;
  std::unique_ptr<WebGraphicsContext3DVideoFramePool> accelerated_frame_pool_;
  absl::optional<base::TimeTicks> first_frame_ticks_;
  scoped_refptr<media::VideoFrame> last_frame_;

  // The following attributes ensure that CanvasCaptureHandler emits
  // frames with monotonically increasing timestamps.
  bool deferred_request_refresh_frame_ = false;
  int num_ongoing_async_pixel_readouts_ = 0;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<CanvasCaptureHandlerDelegate> delegate_;

  // Bound to Main Render thread.
  THREAD_CHECKER(main_render_thread_checker_);
  base::WeakPtrFactory<CanvasCaptureHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIACAPTUREFROMELEMENT_CANVAS_CAPTURE_HANDLER_H_
