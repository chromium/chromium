// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/canvas_capture_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/limits.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints_util.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_capturer_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_to_video_frame_copier.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// Implementation VideoCapturerSource that is owned by
// MediaStreamVideoCapturerSource and delegates the Start/Stop calls to
// CanvasCaptureHandler.
// This class is single threaded and pinned to main render thread.
class CanvasVideoCapturerSource : public VideoCapturerSource {
 public:
  CanvasVideoCapturerSource(base::WeakPtr<CanvasCaptureHandler> canvas_handler,
                            const gfx::Size& size,
                            double frame_rate)
      : size_(size),
        frame_rate_(static_cast<float>(
            std::min(static_cast<double>(media::limits::kMaxFramesPerSecond),
                     frame_rate))),
        canvas_handler_(std::move(canvas_handler)) {
    DCHECK_LE(0, frame_rate_);
  }

 protected:
  media::VideoCaptureFormats GetPreferredFormats() override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    media::VideoCaptureFormats formats;
    formats.push_back(media::VideoCaptureFormat(gfx::Size(size_), frame_rate_,
                                                media::PIXEL_FORMAT_I420));
    formats.push_back(media::VideoCaptureFormat(gfx::Size(size_), frame_rate_,
                                                media::PIXEL_FORMAT_I420A));
    return formats;
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const blink::VideoCaptureDeliverFrameCB& frame_callback,
                    const VideoCaptureSubCaptureTargetVersionCB&
                        sub_capture_target_version_callback,
                    // Canvas capture does not report frame drops.
                    const VideoCaptureNotifyFrameDroppedCB&,
                    const RunningCallback& running_callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get()) {
      canvas_handler_->StartVideoCapture(params, frame_callback,
                                         running_callback);
    }
  }
  void RequestRefreshFrame() override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get())
      canvas_handler_->RequestRefreshFrame();
  }
  void StopCapture() override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get())
      canvas_handler_->StopVideoCapture();
  }
  void SetCanDiscardAlpha(bool can_discard_alpha) override {
    DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
    if (canvas_handler_.get())
      canvas_handler_->SetCanDiscardAlpha(can_discard_alpha);
  }

 private:
  const gfx::Size size_;
  const float frame_rate_;
  // Bound to Main Render thread.
  THREAD_CHECKER(main_render_thread_checker_);
  // CanvasCaptureHandler is owned by CanvasDrawListener in blink. It is
  // guaranteed to be destroyed on Main Render thread and it would happen
  // independently of this class. Therefore, WeakPtr should always be checked
  // before use.
  base::WeakPtr<CanvasCaptureHandler> canvas_handler_;
};

class CanvasCaptureHandler::CanvasCaptureHandlerDelegate {
 public:
  explicit CanvasCaptureHandlerDelegate(
      VideoCaptureDeliverFrameCB new_frame_callback)
      : new_frame_callback_(new_frame_callback) {
    DETACH_FROM_THREAD(io_thread_checker_);
  }

  CanvasCaptureHandlerDelegate(const CanvasCaptureHandlerDelegate&) = delete;
  CanvasCaptureHandlerDelegate& operator=(const CanvasCaptureHandlerDelegate&) =
      delete;

  ~CanvasCaptureHandlerDelegate() {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
  }

  void SendNewFrameOnIOThread(scoped_refptr<media::VideoFrame> video_frame,
                              base::TimeTicks current_time) {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    new_frame_callback_.Run(std::move(video_frame), current_time);
  }

  base::WeakPtr<CanvasCaptureHandlerDelegate> GetWeakPtrForIOThread() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const VideoCaptureDeliverFrameCB new_frame_callback_;
  // Bound to IO thread.
  THREAD_CHECKER(io_thread_checker_);
  base::WeakPtrFactory<CanvasCaptureHandlerDelegate> weak_ptr_factory_{this};
};

CanvasCaptureHandler::CanvasCaptureHandler(
    LocalFrame* frame,
    const gfx::Size& size,
    double frame_rate,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    MediaStreamComponent** component)
    : io_task_runner_(std::move(io_task_runner)) {
  std::unique_ptr<VideoCapturerSource> video_source(
      new CanvasVideoCapturerSource(weak_ptr_factory_.GetWeakPtr(), size,
                                    frame_rate));
  AddVideoCapturerSourceToVideoTrack(std::move(main_task_runner), frame,
                                     std::move(video_source), component);
}

CanvasCaptureHandler::~CanvasCaptureHandler() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

// static
std::unique_ptr<CanvasCaptureHandler>
CanvasCaptureHandler::CreateCanvasCaptureHandler(
    LocalFrame* frame,
    const gfx::Size& size,
    double frame_rate,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    MediaStreamComponent** component) {
  // Save histogram data so we can see how much CanvasCapture is used.
  // The histogram counts the number of calls to the JS API.
  UpdateWebRTCMethodCount(RTCAPIName::kCanvasCaptureStream);

  return std::unique_ptr<CanvasCaptureHandler>(new CanvasCaptureHandler(
      frame, size, frame_rate, std::move(main_task_runner),
      std::move(io_task_runner), component));
}

CanvasCaptureHandler::NewFrameCallback
CanvasCaptureHandler::GetNewFrameCallback() {
  // Increment the number of pending calls, and create a ScopedClosureRunner
  // to ensure that it be decremented even if the returned callback is dropped
  // instead of being run.
  pending_send_new_frame_calls_ += 1;
  auto decrement_closure = WTF::BindOnce(
      [](base::WeakPtr<CanvasCaptureHandler> handler) {
        if (handler)
          handler->pending_send_new_frame_calls_ -= 1;
      },
      weak_ptr_factory_.GetWeakPtr());

  return WTF::BindOnce(&CanvasCaptureHandler::OnNewFrameCallback,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::ScopedClosureRunner(std::move(decrement_closure)),
                       base::TimeTicks::Now(), gfx::ColorSpace());
}

void CanvasCaptureHandler::OnNewFrameCallback(
    base::ScopedClosureRunner decrement_runner,
    base::TimeTicks this_frame_ticks,
    const gfx::ColorSpace& color_space,
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK_GT(pending_send_new_frame_calls_, 0u);
  decrement_runner.RunAndReset();

  if (video_frame)
    SendFrame(this_frame_ticks, color_space, video_frame);

  if (!pending_send_new_frame_calls_ && deferred_request_refresh_frame_)
    SendRefreshFrame();
}

bool CanvasCaptureHandler::NeedsNewFrame() const {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  return ask_for_new_frame_;
}

void CanvasCaptureHandler::StartVideoCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const VideoCapturerSource::RunningCallback& running_callback) {
  DVLOG(3) << __func__ << " requested "
           << media::VideoCaptureFormat::ToString(params.requested_format);
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK(params.requested_format.IsValid());
  capture_format_ = params.requested_format;
  delegate_ =
      std::make_unique<CanvasCaptureHandlerDelegate>(new_frame_callback);
  DCHECK(delegate_);
  ask_for_new_frame_ = true;
  running_callback.Run(RunState::kRunning);
}

void CanvasCaptureHandler::RequestRefreshFrame() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  if (last_frame_ && delegate_) {
    // If we're currently reading out pixels from GL memory, we risk
    // emitting frames with non-incrementally increasing timestamps.
    // Defer sending the refresh frame until we have completed those async
    // reads.
    if (pending_send_new_frame_calls_) {
      deferred_request_refresh_frame_ = true;
      return;
    }
    SendRefreshFrame();
  }
}

void CanvasCaptureHandler::StopVideoCapture() {
  DVLOG(3) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  ask_for_new_frame_ = false;
  io_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());
}

void CanvasCaptureHandler::SendFrame(
    base::TimeTicks this_frame_ticks,
    const gfx::ColorSpace& color_space,
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);

  // If this function is called asynchronously, |delegate_| might have been
  // released already in StopVideoCapture().
  if (!delegate_ || !video_frame)
    return;

  if (!first_frame_ticks_)
    first_frame_ticks_ = this_frame_ticks;
  video_frame->set_timestamp(this_frame_ticks - *first_frame_ticks_);
  if (color_space.IsValid())
    video_frame->set_color_space(color_space);

  last_frame_ = video_frame;
  PostCrossThreadTask(*io_task_runner_, FROM_HERE,
                      WTF::CrossThreadBindOnce(
                          &CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                              SendNewFrameOnIOThread,
                          delegate_->GetWeakPtrForIOThread(),
                          std::move(video_frame), this_frame_ticks));
}

void CanvasCaptureHandler::AddVideoCapturerSourceToVideoTrack(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    LocalFrame* frame,
    std::unique_ptr<VideoCapturerSource> source,
    MediaStreamComponent** component) {
  uint8_t track_id_bytes[64];
  base::RandBytes(track_id_bytes);
  String track_id = Base64Encode(track_id_bytes);
  media::VideoCaptureFormats preferred_formats = source->GetPreferredFormats();
  auto stream_video_source = std::make_unique<MediaStreamVideoCapturerSource>(
      main_task_runner, frame,
      WebPlatformMediaStreamSource::SourceStoppedCallback(), std::move(source));
  auto* stream_video_source_ptr = stream_video_source.get();
  auto* stream_source = MakeGarbageCollected<MediaStreamSource>(
      track_id, MediaStreamSource::kTypeVideo, track_id, false,
      std::move(stream_video_source));
  stream_source->SetCapabilities(ComputeCapabilitiesForVideoSource(
      track_id, preferred_formats, mojom::blink::FacingMode::kNone,
      false /* is_device_capture */));

  *component = MakeGarbageCollected<MediaStreamComponentImpl>(
      stream_source,
      std::make_unique<MediaStreamVideoTrack>(
          stream_video_source_ptr,
          MediaStreamVideoSource::ConstraintsOnceCallback(), true));
}

void CanvasCaptureHandler::SendRefreshFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(main_render_thread_checker_);
  DCHECK_EQ(pending_send_new_frame_calls_, 0u);
  if (last_frame_ && delegate_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CanvasCaptureHandler::CanvasCaptureHandlerDelegate::
                           SendNewFrameOnIOThread,
                       delegate_->GetWeakPtrForIOThread(), last_frame_,
                       base::TimeTicks::Now()));
  }
  deferred_request_refresh_frame_ = false;
}

}  // namespace blink
