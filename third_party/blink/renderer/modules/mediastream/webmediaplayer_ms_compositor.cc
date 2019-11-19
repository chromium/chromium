// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/webmediaplayer_ms_compositor.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "cc/paint/skia_paint_canvas.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/filters/video_renderer_algorithm.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
#include "third_party/blink/public/web/modules/mediastream/webmediaplayer_ms.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/libyuv/include/libyuv/video_common.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace WTF {

template <typename T>
struct CrossThreadCopier<base::Optional<T>>
    : public CrossThreadCopierPassThrough<base::Optional<T>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace blink {

namespace {

// This function copies |frame| to a new I420 or YV12A media::VideoFrame.
scoped_refptr<media::VideoFrame> CopyFrame(
    scoped_refptr<media::VideoFrame> frame,
    media::PaintCanvasVideoRenderer* video_renderer) {
  scoped_refptr<media::VideoFrame> new_frame;
  if (frame->HasTextures()) {
    DCHECK(frame->format() == media::PIXEL_FORMAT_ARGB ||
           frame->format() == media::PIXEL_FORMAT_XRGB ||
           frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_NV12);
    new_frame = media::VideoFrame::CreateFrame(
        media::PIXEL_FORMAT_I420, frame->coded_size(), frame->visible_rect(),
        frame->natural_size(), frame->timestamp());

    auto* const provider =
        Platform::Current()->SharedMainThreadContextProvider();
    if (!provider) {
      // Return a black frame (yuv = {0, 0x80, 0x80}).
      return media::VideoFrame::CreateColorFrame(
          frame->visible_rect().size(), 0u, 0x80, 0x80, frame->timestamp());
    }

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(
        frame->visible_rect().width(), frame->visible_rect().height()));
    cc::SkiaPaintCanvas paint_canvas(bitmap);

    DCHECK(provider->ContextGL());
    video_renderer->Copy(frame.get(), &paint_canvas, provider);

    SkPixmap pixmap;
    const bool result = bitmap.peekPixels(&pixmap);
    DCHECK(result) << "Error trying to access SkBitmap's pixels";

    const uint32_t source_pixel_format =
        (kN32_SkColorType == kRGBA_8888_SkColorType) ? libyuv::FOURCC_ABGR
                                                     : libyuv::FOURCC_ARGB;
    libyuv::ConvertToI420(static_cast<const uint8_t*>(pixmap.addr(0, 0)),
                          pixmap.computeByteSize(),
                          new_frame->visible_data(media::VideoFrame::kYPlane),
                          new_frame->stride(media::VideoFrame::kYPlane),
                          new_frame->visible_data(media::VideoFrame::kUPlane),
                          new_frame->stride(media::VideoFrame::kUPlane),
                          new_frame->visible_data(media::VideoFrame::kVPlane),
                          new_frame->stride(media::VideoFrame::kVPlane),
                          0 /* crop_x */, 0 /* crop_y */, pixmap.width(),
                          pixmap.height(), new_frame->visible_rect().width(),
                          new_frame->visible_rect().height(), libyuv::kRotate0,
                          source_pixel_format);
  } else {
    DCHECK(frame->IsMappable());
    DCHECK(frame->format() == media::PIXEL_FORMAT_I420A ||
           frame->format() == media::PIXEL_FORMAT_I420);
    const gfx::Size& coded_size = frame->coded_size();
    new_frame = media::VideoFrame::CreateFrame(
        media::IsOpaque(frame->format()) ? media::PIXEL_FORMAT_I420
                                         : media::PIXEL_FORMAT_I420A,
        coded_size, frame->visible_rect(), frame->natural_size(),
        frame->timestamp());
    libyuv::I420Copy(frame->data(media::VideoFrame::kYPlane),
                     frame->stride(media::VideoFrame::kYPlane),
                     frame->data(media::VideoFrame::kUPlane),
                     frame->stride(media::VideoFrame::kUPlane),
                     frame->data(media::VideoFrame::kVPlane),
                     frame->stride(media::VideoFrame::kVPlane),
                     new_frame->data(media::VideoFrame::kYPlane),
                     new_frame->stride(media::VideoFrame::kYPlane),
                     new_frame->data(media::VideoFrame::kUPlane),
                     new_frame->stride(media::VideoFrame::kUPlane),
                     new_frame->data(media::VideoFrame::kVPlane),
                     new_frame->stride(media::VideoFrame::kVPlane),
                     coded_size.width(), coded_size.height());
    if (frame->format() == media::PIXEL_FORMAT_I420A) {
      libyuv::CopyPlane(frame->data(media::VideoFrame::kAPlane),
                        frame->stride(media::VideoFrame::kAPlane),
                        new_frame->data(media::VideoFrame::kAPlane),
                        new_frame->stride(media::VideoFrame::kAPlane),
                        coded_size.width(), coded_size.height());
    }
  }

  // Transfer metadata keys.
  new_frame->metadata()->MergeMetadataFrom(frame->metadata());
  return new_frame;
}

}  // anonymous namespace

WebMediaPlayerMSCompositor::WebMediaPlayerMSCompositor(
    scoped_refptr<base::SingleThreadTaskRunner>
        video_frame_compositor_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const WebMediaStream& web_stream,
    std::unique_ptr<WebVideoFrameSubmitter> submitter,
    WebMediaPlayer::SurfaceLayerMode surface_layer_mode,
    const base::WeakPtr<WebMediaPlayerMS>& player)
    : video_frame_compositor_task_runner_(video_frame_compositor_task_runner),
      io_task_runner_(io_task_runner),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      player_(player),
      video_frame_provider_client_(nullptr),
      current_frame_rendered_(false),
      last_render_length_(base::TimeDelta::FromSecondsD(1.0 / 60.0)),
      total_frame_count_(0),
      dropped_frame_count_(0),
      stopped_(true),
      render_started_(!stopped_) {
  if (surface_layer_mode != WebMediaPlayer::SurfaceLayerMode::kNever) {
    submitter_ = std::move(submitter);

    PostCrossThreadTask(
        *video_frame_compositor_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&WebMediaPlayerMSCompositor::InitializeSubmitter,
                            weak_ptr_factory_.GetWeakPtr()));
    update_submission_state_callback_ =
        media::BindToLoop(video_frame_compositor_task_runner_,
                          ConvertToBaseCallback(CrossThreadBindRepeating(
                              &WebMediaPlayerMSCompositor::SetIsSurfaceVisible,
                              weak_ptr_factory_.GetWeakPtr())));
  }

  WebVector<WebMediaStreamTrack> video_tracks;
  if (!web_stream.IsNull())
    video_tracks = web_stream.VideoTracks();

  const bool remote_video =
      video_tracks.size() && video_tracks[0].Source().Remote();

  if (remote_video && Platform::Current()->RTCSmoothnessAlgorithmEnabled()) {
    base::AutoLock auto_lock(current_frame_lock_);
    rendering_frame_buffer_.reset(new media::VideoRendererAlgorithm(
        ConvertToBaseCallback(CrossThreadBindRepeating(
            &WebMediaPlayerMSCompositor::MapTimestampsToRenderTimeTicks,
            CrossThreadUnretained(this))),
        &media_log_));
  }

  // Just for logging purpose.
  std::string stream_id =
      web_stream.IsNull() ? std::string() : web_stream.Id().Utf8();
  const uint32_t hash_value = base::Hash(stream_id);
  serial_ = (hash_value << 1) | (remote_video ? 1 : 0);
}

WebMediaPlayerMSCompositor::~WebMediaPlayerMSCompositor() {
  // Ensured by destructor traits.
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());

  if (submitter_) {
    video_frame_compositor_task_runner_->DeleteSoon(FROM_HERE,
                                                    std::move(submitter_));
  } else {
    DCHECK(!video_frame_provider_client_)
        << "Must call StopUsingProvider() before dtor!";
  }
}

// static
void WebMediaPlayerMSCompositorTraits::Destruct(
    const WebMediaPlayerMSCompositor* compositor) {
  if (!compositor->video_frame_compositor_task_runner_
           ->BelongsToCurrentThread()) {
    PostCrossThreadTask(
        *compositor->video_frame_compositor_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&WebMediaPlayerMSCompositorTraits::Destruct,
                            CrossThreadUnretained(compositor)));
    return;
  }
  delete compositor;
}

void WebMediaPlayerMSCompositor::InitializeSubmitter() {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  submitter_->Initialize(this);
}

void WebMediaPlayerMSCompositor::SetIsSurfaceVisible(bool state) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  submitter_->SetIsSurfaceVisible(state);
}

// TODO(https://crbug/879424): Rename, since it really doesn't enable
// submission. Do this along with the VideoFrameSubmitter refactor.
void WebMediaPlayerMSCompositor::EnableSubmission(
    const viz::SurfaceId& id,
    base::TimeTicks local_surface_id_allocation_time,
    media::VideoTransformation transformation,
    bool force_submit) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());

  // If we're switching to |submitter_| from some other client, then tell it.
  if (video_frame_provider_client_ &&
      video_frame_provider_client_ != submitter_.get()) {
    video_frame_provider_client_->StopUsingProvider();
  }

  submitter_->SetRotation(transformation.rotation);
  submitter_->SetForceSubmit(force_submit);
  submitter_->EnableSubmission(id, local_surface_id_allocation_time);
  video_frame_provider_client_ = submitter_.get();

  if (!stopped_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMSCompositor::SetForceSubmit(bool force_submit) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  submitter_->SetForceSubmit(force_submit);
}

void WebMediaPlayerMSCompositor::SetIsPageVisible(bool is_visible) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  if (submitter_)
    submitter_->SetIsPageVisible(is_visible);
}

gfx::Size WebMediaPlayerMSCompositor::GetCurrentSize() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_ ? current_frame_->natural_size() : gfx::Size();
}

base::TimeDelta WebMediaPlayerMSCompositor::GetCurrentTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoLock auto_lock(current_frame_lock_);
  return current_frame_.get() ? current_frame_->timestamp() : base::TimeDelta();
}

size_t WebMediaPlayerMSCompositor::total_frame_count() {
  base::AutoLock auto_lock(current_frame_lock_);
  DVLOG(1) << __func__ << ", " << total_frame_count_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return total_frame_count_;
}

size_t WebMediaPlayerMSCompositor::dropped_frame_count() {
  base::AutoLock auto_lock(current_frame_lock_);
  DVLOG(1) << __func__ << ", " << dropped_frame_count_;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return dropped_frame_count_;
}

void WebMediaPlayerMSCompositor::SetVideoFrameProviderClient(
    cc::VideoFrameProvider::Client* client) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();

  video_frame_provider_client_ = client;
  if (video_frame_provider_client_ && !stopped_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMSCompositor::EnqueueFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  base::AutoLock auto_lock(current_frame_lock_);
  TRACE_EVENT_INSTANT1("media", "WebMediaPlayerMSCompositor::EnqueueFrame",
                       TRACE_EVENT_SCOPE_THREAD, "Timestamp",
                       frame->timestamp().InMicroseconds());
  ++total_frame_count_;

  // With algorithm off, just let |current_frame_| hold the incoming |frame|.
  if (!rendering_frame_buffer_) {
    RenderWithoutAlgorithm(std::move(frame));
    return;
  }

  // This is a signal frame saying that the stream is stopped.
  bool end_of_stream = false;
  if (frame->metadata()->GetBoolean(media::VideoFrameMetadata::END_OF_STREAM,
                                    &end_of_stream) &&
      end_of_stream) {
    rendering_frame_buffer_.reset();
    RenderWithoutAlgorithm(std::move(frame));
    return;
  }

  // If we detect a bad frame without |render_time|, we switch off algorithm,
  // because without |render_time|, algorithm cannot work.
  // In general, this should not happen.
  base::TimeTicks render_time;
  if (!frame->metadata()->GetTimeTicks(
          media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
    DLOG(WARNING)
        << "Incoming VideoFrames have no REFERENCE_TIME, switching off super "
           "sophisticated rendering algorithm";
    rendering_frame_buffer_.reset();
    RenderWithoutAlgorithm(std::move(frame));
    return;
  }

  // The code below handles the case where UpdateCurrentFrame() callbacks stop.
  // These callbacks can stop when the tab is hidden or the page area containing
  // the video frame is scrolled out of view.
  // Since some hardware decoders only have a limited number of output frames,
  // we must aggressively release frames in this case.
  const base::TimeTicks now = base::TimeTicks::Now();
  if (now > last_deadline_max_) {
    // Note: the frame in |rendering_frame_buffer_| with lowest index is the
    // same as |current_frame_|. Function SetCurrentFrame() handles whether
    // to increase |dropped_frame_count_| for that frame, so here we should
    // increase |dropped_frame_count_| by the count of all other frames.
    dropped_frame_count_ += rendering_frame_buffer_->frames_queued() - 1;
    rendering_frame_buffer_->Reset();
    timestamps_to_clock_times_.clear();
    RenderWithoutAlgorithm(frame);
  }

  timestamps_to_clock_times_[frame->timestamp()] = render_time;
  rendering_frame_buffer_->EnqueueFrame(std::move(frame));
}

bool WebMediaPlayerMSCompositor::UpdateCurrentFrame(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT_BEGIN2("media", "UpdateCurrentFrame", "Actual Render Begin",
                     deadline_min.ToInternalValue(), "Actual Render End",
                     deadline_max.ToInternalValue());
  if (stopped_)
    return false;

  base::AutoLock auto_lock(current_frame_lock_);

  if (rendering_frame_buffer_)
    RenderUsingAlgorithm(deadline_min, deadline_max);

  {
    bool tracing_or_dcheck_enabled = false;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED("media", &tracing_or_dcheck_enabled);
#if DCHECK_IS_ON()
    tracing_or_dcheck_enabled = true;
#endif  // DCHECK_IS_ON()
    if (tracing_or_dcheck_enabled) {
      base::TimeTicks render_time;
      if (!current_frame_->metadata()->GetTimeTicks(
              media::VideoFrameMetadata::REFERENCE_TIME, &render_time)) {
        DCHECK(!rendering_frame_buffer_)
            << "VideoFrames need REFERENCE_TIME to use "
               "sophisticated video rendering algorithm.";
      }
      TRACE_EVENT_END2("media", "UpdateCurrentFrame", "Ideal Render Instant",
                       render_time.ToInternalValue(), "Serial", serial_);
    }
  }

  return !current_frame_rendered_;
}

bool WebMediaPlayerMSCompositor::HasCurrentFrame() {
  base::AutoLock auto_lock(current_frame_lock_);
  return !!current_frame_;
}

scoped_refptr<media::VideoFrame> WebMediaPlayerMSCompositor::GetCurrentFrame() {
  DVLOG(3) << __func__;
  base::AutoLock auto_lock(current_frame_lock_);
  TRACE_EVENT_INSTANT1("media", "WebMediaPlayerMSCompositor::GetCurrentFrame",
                       TRACE_EVENT_SCOPE_THREAD, "Timestamp",
                       current_frame_->timestamp().InMicroseconds());
  if (!render_started_)
    return nullptr;

  return current_frame_;
}

void WebMediaPlayerMSCompositor::PutCurrentFrame() {
  DVLOG(3) << __func__;
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  current_frame_rendered_ = true;
}

base::TimeDelta WebMediaPlayerMSCompositor::GetPreferredRenderInterval() {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  return viz::BeginFrameArgs::MinInterval();
}

void WebMediaPlayerMSCompositor::StartRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    base::AutoLock auto_lock(current_frame_lock_);
    render_started_ = true;
  }
  PostCrossThreadTask(
      *video_frame_compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMSCompositor::StartRenderingInternal,
                          WrapRefCounted(this)));
}

void WebMediaPlayerMSCompositor::StopRendering() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *video_frame_compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMSCompositor::StopRenderingInternal,
                          WrapRefCounted(this)));
}

void WebMediaPlayerMSCompositor::ReplaceCurrentFrameWithACopy() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Bounce this call off of IO thread to since there might still be frames
  // passed on IO thread.
  io_task_runner_->PostTask(
      FROM_HERE,
      media::BindToCurrentLoop(WTF::Bind(
          &WebMediaPlayerMSCompositor::ReplaceCurrentFrameWithACopyInternal,
          WrapRefCounted(this))));
}

void WebMediaPlayerMSCompositor::StopUsingProvider() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PostCrossThreadTask(
      *video_frame_compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &WebMediaPlayerMSCompositor::StopUsingProviderInternal,
          WrapRefCounted(this)));
}

bool WebMediaPlayerMSCompositor::MapTimestampsToRenderTimeTicks(
    const std::vector<base::TimeDelta>& timestamps,
    std::vector<base::TimeTicks>* wall_clock_times) {
#if DCHECK_IS_ON()
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread() ||
         thread_checker_.CalledOnValidThread() ||
         io_task_runner_->BelongsToCurrentThread());
#endif
  for (const base::TimeDelta& timestamp : timestamps) {
    DCHECK(timestamps_to_clock_times_.count(timestamp));
    wall_clock_times->push_back(timestamps_to_clock_times_[timestamp]);
  }
  return true;
}

void WebMediaPlayerMSCompositor::RenderUsingAlgorithm(
    base::TimeTicks deadline_min,
    base::TimeTicks deadline_max) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  current_frame_lock_.AssertAcquired();
  last_deadline_max_ = deadline_max;
  last_render_length_ = deadline_max - deadline_min;

  size_t frames_dropped = 0;
  scoped_refptr<media::VideoFrame> frame = rendering_frame_buffer_->Render(
      deadline_min, deadline_max, &frames_dropped);
  dropped_frame_count_ += frames_dropped;

  // There is a chance we get a null |frame| here:
  // When the player gets paused, we reset |rendering_frame_buffer_|;
  // When the player gets resumed, it is possible that render gets called before
  // we get a new frame. In that case continue to render the |current_frame_|.
  if (!frame || frame == current_frame_)
    return;

  const base::TimeDelta timestamp = frame->timestamp();
  SetCurrentFrame(std::move(frame));

  const auto& end = timestamps_to_clock_times_.end();
  const auto& begin = timestamps_to_clock_times_.begin();
  auto iterator = begin;
  while (iterator != end && iterator->first < timestamp)
    ++iterator;
  timestamps_to_clock_times_.erase(begin, iterator);
}

void WebMediaPlayerMSCompositor::RenderWithoutAlgorithm(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  PostCrossThreadTask(
      *video_frame_compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &WebMediaPlayerMSCompositor::RenderWithoutAlgorithmOnCompositor,
          WrapRefCounted(this), std::move(frame)));
}

void WebMediaPlayerMSCompositor::RenderWithoutAlgorithmOnCompositor(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock auto_lock(current_frame_lock_);
    SetCurrentFrame(std::move(frame));
  }
  if (video_frame_provider_client_)
    video_frame_provider_client_->DidReceiveFrame();
}

void WebMediaPlayerMSCompositor::SetCurrentFrame(
    scoped_refptr<media::VideoFrame> frame) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  current_frame_lock_.AssertAcquired();
  TRACE_EVENT_INSTANT1("media", "WebMediaPlayerMSCompositor::SetCurrentFrame",
                       TRACE_EVENT_SCOPE_THREAD, "Timestamp",
                       frame->timestamp().InMicroseconds());

  if (!current_frame_rendered_)
    ++dropped_frame_count_;
  current_frame_rendered_ = false;

  // Compare current frame with |frame|. Initialize values as if there is no
  // current frame.
  bool is_first_frame = true;
  bool has_frame_size_changed = false;
  base::Optional<media::VideoRotation> new_rotation = media::VIDEO_ROTATION_0;
  base::Optional<bool> new_opacity;

  new_opacity = media::IsOpaque(frame->format());
  media::VideoRotation current_video_rotation;
  if (frame->metadata()->GetRotation(media::VideoFrameMetadata::ROTATION,
                                     &current_video_rotation)) {
    new_rotation = current_video_rotation;
  }

  if (current_frame_) {
    // We have a current frame, so determine what has changed.
    is_first_frame = false;

    if (!current_frame_->metadata()->GetRotation(
            media::VideoFrameMetadata::ROTATION, &current_video_rotation)) {
      // Assume VIDEO_ROTATION_0 for current frame without video rotation.
      current_video_rotation = media::VIDEO_ROTATION_0;
    }
    if (current_video_rotation == *new_rotation) {
      new_rotation.reset();
    }

    if (*new_opacity == media::IsOpaque(current_frame_->format()))
      new_opacity.reset();

    has_frame_size_changed =
        frame->natural_size() != current_frame_->natural_size();
  }

  current_frame_ = std::move(frame);

  // Complete the checks after |current_frame_| is accessible to avoid
  // deadlocks, see https://crbug.com/901744.
  PostCrossThreadTask(
      *video_frame_compositor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMSCompositor::CheckForFrameChanges,
                          WrapRefCounted(this), is_first_frame,
                          has_frame_size_changed, std::move(new_rotation),
                          std::move(new_opacity)));
}

void WebMediaPlayerMSCompositor::CheckForFrameChanges(
    bool is_first_frame,
    bool has_frame_size_changed,
    base::Optional<media::VideoRotation> new_frame_rotation,
    base::Optional<bool> new_frame_opacity) {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());

  if (is_first_frame) {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&WebMediaPlayerMS::OnFirstFrameReceived, player_,
                            *new_frame_rotation, *new_frame_opacity));
    return;
  }
  if (new_frame_rotation.has_value()) {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&WebMediaPlayerMS::OnRotationChanged, player_,
                            *new_frame_rotation));
    if (submitter_)
      submitter_->SetRotation(*new_frame_rotation);
  }
  if (new_frame_opacity.has_value()) {
    PostCrossThreadTask(*main_task_runner_, FROM_HERE,
                        CrossThreadBindOnce(&WebMediaPlayerMS::OnOpacityChanged,
                                            player_, *new_frame_opacity));
  }
  if (has_frame_size_changed) {
    PostCrossThreadTask(
        *main_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&WebMediaPlayerMS::TriggerResize, player_));
  }
  PostCrossThreadTask(
      *main_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&WebMediaPlayerMS::ResetCanvasCache, player_));
}

void WebMediaPlayerMSCompositor::StartRenderingInternal() {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  stopped_ = false;

  if (video_frame_provider_client_)
    video_frame_provider_client_->StartRendering();
}

void WebMediaPlayerMSCompositor::StopRenderingInternal() {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  stopped_ = true;

  // It is possible that the video gets paused and then resumed. We need to
  // reset VideoRendererAlgorithm, otherwise, VideoRendererAlgorithm will think
  // there is a very long frame in the queue and then make totally wrong
  // frame selection.
  {
    base::AutoLock auto_lock(current_frame_lock_);
    if (rendering_frame_buffer_)
      rendering_frame_buffer_->Reset();
  }

  if (video_frame_provider_client_)
    video_frame_provider_client_->StopRendering();
}

void WebMediaPlayerMSCompositor::StopUsingProviderInternal() {
  DCHECK(video_frame_compositor_task_runner_->BelongsToCurrentThread());
  if (video_frame_provider_client_)
    video_frame_provider_client_->StopUsingProvider();
  video_frame_provider_client_ = nullptr;
}

void WebMediaPlayerMSCompositor::ReplaceCurrentFrameWithACopyInternal() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<media::VideoFrame> current_frame_ref;
  {
    base::AutoLock auto_lock(current_frame_lock_);
    if (!current_frame_ || !player_)
      return;
    current_frame_ref = current_frame_;
  }
  // Copy the frame so that rendering can show the last received frame.
  // The original frame must not be referenced when the player is paused since
  // there might be a finite number of available buffers. E.g, video that
  // originates from a video camera, HW decoded frames.
  scoped_refptr<media::VideoFrame> copied_frame =
      CopyFrame(current_frame_ref, player_->GetPaintCanvasVideoRenderer());
  // Copying frame can take time, so only set the copied frame if
  // |current_frame_| hasn't been changed.
  {
    base::AutoLock auto_lock(current_frame_lock_);
    if (current_frame_ == current_frame_ref)
      current_frame_ = std::move(copied_frame);
  }
}

void WebMediaPlayerMSCompositor::SetAlgorithmEnabledForTesting(
    bool algorithm_enabled) {
  if (!algorithm_enabled) {
    rendering_frame_buffer_.reset();
    return;
  }

  if (!rendering_frame_buffer_) {
    rendering_frame_buffer_.reset(new media::VideoRendererAlgorithm(
        WTF::BindRepeating(
            &WebMediaPlayerMSCompositor::MapTimestampsToRenderTimeTicks,
            WTF::Unretained(this)),
        &media_log_));
  }
}

}  // namespace blink
