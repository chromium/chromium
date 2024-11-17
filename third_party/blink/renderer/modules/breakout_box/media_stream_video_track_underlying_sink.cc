// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_sink.h"

#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "media/base/video_types.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/modules/breakout_box/metrics.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_video_frame_pool.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// Cannot be in the anonymous namespace because it is friended by
// MainThreadTaskRunnerRestricted.
MainThreadTaskRunnerRestricted AccessMainThreadForGpuMemoryBufferManager() {
  return {};
}

namespace {
// Enables conversion of input frames in RGB format to NV12 GMB-backed format
// if GMB readback from texture is supported.
BASE_FEATURE(kBreakoutBoxEagerConversion,
             "BreakoutBoxEagerConversion",
             base::FEATURE_ENABLED_BY_DEFAULT
);

// If BreakoutBoxEagerConversion is enabled, this feature enables frame
// conversion even if the sinks connected to the track backed by the
// MediaStreamVideoTrackUnderlyingSink have not sent the RequireMappedFrame
// signal.
// This feature has no effect if BreakoutBoxEagerConversion is disabled.
BASE_FEATURE(kBreakoutBoxConversionWithoutSinkSignal,
             "BreakoutBoxConversionWithoutSinkSignal",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If BreakoutBoxWriteVideoFrameCaptureTimestamp is enabled, the timestamp from
// a blink::VideoFrame written to a MediaStreamVideoTrackUnderlyingSink is also
// set as the capture timestamp for its underlying media::VideoFrame.
// TODO(crbug.com/343870500): Remove this feature once WebCodec VideoFrames
// expose the capture time as metadata.
BASE_FEATURE(kBreakoutBoxWriteVideoFrameCaptureTimestamp,
             "BreakoutBoxWriteVideoFrameCaptureTimestamp",
             base::FEATURE_ENABLED_BY_DEFAULT);

class TransferringOptimizer : public WritableStreamTransferringOptimizer {
 public:
  explicit TransferringOptimizer(
      scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker,
      gpu::GpuMemoryBufferManager* gmb_manager)
      : source_broker_(std::move(source_broker)), gmb_manager_(gmb_manager) {}
  UnderlyingSinkBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableVideoWorker);
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(
        source_broker_, gmb_manager_);
  }

 private:
  const scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker_;
  const raw_ptr<gpu::GpuMemoryBufferManager> gmb_manager_ = nullptr;
};

gpu::GpuMemoryBufferManager* GetGmbManager() {
  if (!WebGraphicsContext3DVideoFramePool::
          IsGpuMemoryBufferReadbackFromTextureEnabled()) {
    return nullptr;
  }
  gpu::GpuMemoryBufferManager* gmb_manager = nullptr;
  if (IsMainThread()) {
    gmb_manager = Platform::Current()->GetGpuMemoryBufferManager();
  } else {
    // Get the GPU Buffer Manager by jumping to the main thread and blocking.
    // The purpose of blocking is to have the manager available by the time
    // the first frame arrives. This ensures all frames can be converted to
    // the appropriate format, which helps prevent a WebRTC sink from falling
    // back to software encoding due to frames in formats the hardware encoder
    // cannot handle.
    base::WaitableEvent waitable_event;
    PostCrossThreadTask(
        *Thread::MainThread()->GetTaskRunner(
            AccessMainThreadForGpuMemoryBufferManager()),
        FROM_HERE,
        CrossThreadBindOnce(
            [](base::WaitableEvent* event,
               gpu::GpuMemoryBufferManager** gmb_manager_ptr) {
              *gmb_manager_ptr =
                  Platform::Current()->GetGpuMemoryBufferManager();
              event->Signal();
            },
            CrossThreadUnretained(&waitable_event),
            CrossThreadUnretained(&gmb_manager)));
    waitable_event.Wait();
  }
  return gmb_manager;
}

}  // namespace

MediaStreamVideoTrackUnderlyingSink::MediaStreamVideoTrackUnderlyingSink(
    scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker,
    gpu::GpuMemoryBufferManager* gmb_manager)
    : source_broker_(std::move(source_broker)), gmb_manager_(gmb_manager) {
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableVideo);
}

MediaStreamVideoTrackUnderlyingSink::MediaStreamVideoTrackUnderlyingSink(
    scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker)
    : MediaStreamVideoTrackUnderlyingSink(std::move(source_broker),
                                          GetGmbManager()) {}

MediaStreamVideoTrackUnderlyingSink::~MediaStreamVideoTrackUnderlyingSink() =
    default;

ScriptPromise<IDLUndefined> MediaStreamVideoTrackUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_broker_->OnClientStarted();
  is_connected_ = true;
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> MediaStreamVideoTrackUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VideoFrame* video_frame =
      V8VideoFrame::ToWrappable(script_state->GetIsolate(), chunk.V8Value());
  if (!video_frame) {
    exception_state.ThrowTypeError("Null video frame.");
    return EmptyPromise();
  }

  auto media_frame = video_frame->frame();
  if (!media_frame) {
    exception_state.ThrowTypeError("Empty video frame.");
    return EmptyPromise();
  }

  static const base::TimeDelta kLongDelta = base::Minutes(1);
  base::TimeDelta now = base::TimeTicks::Now() - base::TimeTicks();
  if (base::FeatureList::IsEnabled(
          kBreakoutBoxWriteVideoFrameCaptureTimestamp) &&
      should_try_to_write_capture_time_ &&
      !media_frame->metadata().capture_begin_time && (now > kLongDelta)) {
    // If the difference between now and the frame's timestamp is large,
    // assume the stream is not using capture times as timestamps.
    if ((media_frame->timestamp() - now).magnitude() > kLongDelta) {
      should_try_to_write_capture_time_ = false;
    }

    if (should_try_to_write_capture_time_) {
      media_frame->metadata().capture_begin_time =
          base::TimeTicks() + video_frame->handle()->timestamp();
    }
  }

  // Invalidate the JS |video_frame|. Otherwise, the media frames might not be
  // released, which would leak resources and also cause some MediaStream
  // sources such as cameras to drop frames.
  video_frame->close();

  if (!source_broker_->IsRunning()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return EmptyPromise();
  }

  base::TimeTicks estimated_capture_time = base::TimeTicks::Now();

  // Try to convert to an NV12 GpuMemoryBuffer-backed frame if the encoder
  // prefers that format. Unfortunately, for the first few frames, we may not
  // receive feedback from the sink (CanDiscardAlpha and RequireMappedFrame), so
  // those frames will instead be converted immediately before encoding (by
  // WebRtcVideoFrameAdapter).
  auto opt_convert_promise = MaybeConvertToNV12GMBVideoFrame(
      script_state, media_frame, estimated_capture_time);
  if (opt_convert_promise) {
    return *opt_convert_promise;
  }

  source_broker_->PushFrame(std::move(media_frame), estimated_capture_time);

  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> MediaStreamVideoTrackUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> MediaStreamVideoTrackUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ToResolvedUndefinedPromise(script_state);
}

std::unique_ptr<WritableStreamTransferringOptimizer>
MediaStreamVideoTrackUnderlyingSink::GetTransferringOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<TransferringOptimizer>(source_broker_, gmb_manager_);
}

void MediaStreamVideoTrackUnderlyingSink::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_connected_)
    return;

  source_broker_->OnClientStopped();
  is_connected_ = false;
}

void MediaStreamVideoTrackUnderlyingSink::CreateAcceleratedFramePool(
    gpu::GpuMemoryBufferManager* gmb_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Here we need to use the SharedGpuContext as some of the images may have
  // been originated with other contextProvider, but we internally need a
  // context_provider that has a RasterInterface available.
  auto context_provider = SharedGpuContext::ContextProviderWrapper();
  if (context_provider && gmb_manager) {
    accelerated_frame_pool_ =
        std::make_unique<WebGraphicsContext3DVideoFramePool>(context_provider,
                                                             gmb_manager);
  } else {
    convert_to_nv12_gmb_failure_count_++;
  }
}

std::optional<ScriptPromise<IDLUndefined>>
MediaStreamVideoTrackUnderlyingSink::MaybeConvertToNV12GMBVideoFrame(
    ScriptState* script_state,
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  static constexpr int kMaxFailures = 5;
  if (convert_to_nv12_gmb_failure_count_ > kMaxFailures) {
    return std::nullopt;
  }
  DCHECK(video_frame);
  auto format = video_frame->format();
  bool frame_is_rgb = (format == media::PIXEL_FORMAT_XBGR ||
                       format == media::PIXEL_FORMAT_ABGR ||
                       format == media::PIXEL_FORMAT_XRGB ||
                       format == media::PIXEL_FORMAT_ARGB);
  bool frame_can_be_converted =
      video_frame->HasSharedImage() &&
      (media::IsOpaque(format) || source_broker_->CanDiscardAlpha());
  bool sink_wants_mapped_frame =
      base::FeatureList::IsEnabled(kBreakoutBoxConversionWithoutSinkSignal) ||
      source_broker_->RequireMappedFrame();

  bool should_eagerly_convert =
      base::FeatureList::IsEnabled(kBreakoutBoxEagerConversion) &&
      WebGraphicsContext3DVideoFramePool::
          IsGpuMemoryBufferReadbackFromTextureEnabled() &&
      frame_is_rgb && frame_can_be_converted && sink_wants_mapped_frame;
  if (!should_eagerly_convert) {
    return std::nullopt;
  }

  if (!accelerated_frame_pool_) {
    gpu::GpuMemoryBufferManager* gmb_manager = GetGmbManager();
    if (!gmb_manager) {
      convert_to_nv12_gmb_failure_count_++;
      return std::nullopt;
    }

    CreateAcceleratedFramePool(gmb_manager);
    if (!accelerated_frame_pool_) {
      convert_to_nv12_gmb_failure_count_++;
      return std::nullopt;
    }
  }
  DCHECK(accelerated_frame_pool_);

  auto resolver = WrapPersistent(
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state));
  auto convert_done_callback = WTF::BindOnce(
      &MediaStreamVideoTrackUnderlyingSink::ConvertDone, WrapPersistent(this),
      resolver, video_frame, estimated_capture_time);
  const bool success = accelerated_frame_pool_->ConvertVideoFrame(
      video_frame, gfx::ColorSpace::CreateREC709(),
      std::move(convert_done_callback));
  if (success) {
    convert_to_nv12_gmb_failure_count_ = 0;
  } else {
    ConvertDone(resolver, video_frame, estimated_capture_time,
                /*converted_video_frame=*/nullptr);
    convert_to_nv12_gmb_failure_count_++;
  }
  return resolver->Promise();
}

void MediaStreamVideoTrackUnderlyingSink::ConvertDone(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    scoped_refptr<media::VideoFrame> orig_video_frame,
    base::TimeTicks estimated_capture_time,
    scoped_refptr<media::VideoFrame> converted_video_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!source_broker_->IsRunning()) {
    // The MediaStreamTrack was stopped while write was pending.
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError, "Stream closed"));
    return;
  }
  if (converted_video_frame) {
    source_broker_->PushFrame(std::move(converted_video_frame),
                              estimated_capture_time);
  } else {
    source_broker_->PushFrame(std::move(orig_video_frame),
                              estimated_capture_time);
  }
  resolver->Resolve();
}

}  // namespace blink
