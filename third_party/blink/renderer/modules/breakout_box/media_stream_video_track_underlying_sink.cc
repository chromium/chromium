// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_sink.h"

#include "base/feature_list.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
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

namespace {

BASE_FEATURE(kBreakoutBoxEagerConversion,
             "BreakoutBoxEagerConversion",
// This feature has the same restrictions as TwoCopyCanvasCapture; see
// comments there.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(IS_CHROMEOS) && defined(ARCH_CPU_X86_FAMILY))
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If BreakoutBoxEagerConversion is enabled, this feature enables frame
// conversion even if the sinks connected to the track backed by the
// MediaStreamVideoTrackUnderlyingSink have not sent the RequireMappedFrame
// signal.
// This feature has no effect if BreakoutBoxEagerConversion is disabled.
BASE_FEATURE(kBreakoutBoxConversionWithoutSinkSignal,
             "BreakoutBoxConversionWithoutSinkSignal",
             base::FEATURE_ENABLED_BY_DEFAULT);

class TransferringOptimizer : public WritableStreamTransferringOptimizer {
 public:
  explicit TransferringOptimizer(
      scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker)
      : source_broker_(std::move(source_broker)) {}
  UnderlyingSinkBase* PerformInProcessOptimization(
      ScriptState* script_state) override {
    RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableVideoWorker);
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSink>(
        source_broker_);
  }

 private:
  const scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker_;
};

}  // namespace

MainThreadTaskRunnerRestricted AccessMainThreadForGpuMemoryBufferManager() {
  return {};
}

MediaStreamVideoTrackUnderlyingSink::MediaStreamVideoTrackUnderlyingSink(
    scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker)
    : source_broker_(std::move(source_broker)) {
  RecordBreakoutBoxUsage(BreakoutBoxUsage::kWritableVideo);
}

MediaStreamVideoTrackUnderlyingSink::~MediaStreamVideoTrackUnderlyingSink() =
    default;

ScriptPromise MediaStreamVideoTrackUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  source_broker_->OnClientStarted();
  is_connected_ = true;
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VideoFrame* video_frame =
      V8VideoFrame::ToWrappable(script_state->GetIsolate(), chunk.V8Value());
  if (!video_frame) {
    exception_state.ThrowTypeError("Null video frame.");
    return ScriptPromise();
  }

  auto media_frame = video_frame->frame();
  if (!media_frame) {
    exception_state.ThrowTypeError("Empty video frame.");
    return ScriptPromise();
  }
  // Invalidate the JS |video_frame|. Otherwise, the media frames might not be
  // released, which would leak resources and also cause some MediaStream
  // sources such as cameras to drop frames.
  video_frame->close();

  if (!source_broker_->IsRunning()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
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

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise MediaStreamVideoTrackUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect();
  return ScriptPromise::CastUndefined(script_state);
}

std::unique_ptr<WritableStreamTransferringOptimizer>
MediaStreamVideoTrackUnderlyingSink::GetTransferringOptimizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<TransferringOptimizer>(source_broker_);
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

absl::optional<ScriptPromise>
MediaStreamVideoTrackUnderlyingSink::MaybeConvertToNV12GMBVideoFrame(
    ScriptState* script_state,
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks estimated_capture_time) {
  static constexpr int kMaxFailures = 5;
  if (convert_to_nv12_gmb_failure_count_ > kMaxFailures) {
    return absl::nullopt;
  }
  DCHECK(video_frame);
  auto format = video_frame->format();
  bool frame_is_rgb = (format == media::PIXEL_FORMAT_XBGR ||
                       format == media::PIXEL_FORMAT_ABGR ||
                       format == media::PIXEL_FORMAT_XRGB ||
                       format == media::PIXEL_FORMAT_ARGB);
  bool frame_can_be_converted =
      video_frame->NumTextures() == 1 &&
      (media::IsOpaque(format) || source_broker_->CanDiscardAlpha());
  bool sink_wants_mapped_frame =
      base::FeatureList::IsEnabled(kBreakoutBoxConversionWithoutSinkSignal) ||
      source_broker_->RequireMappedFrame();

  bool should_eagerly_convert =
      base::FeatureList::IsEnabled(kBreakoutBoxEagerConversion) &&
      frame_is_rgb && frame_can_be_converted && sink_wants_mapped_frame;
  if (!should_eagerly_convert) {
    return absl::nullopt;
  }

  if (!accelerated_frame_pool_) {
    gpu::GpuMemoryBufferManager* gmb_manager = nullptr;
    if (IsMainThread()) {
      gmb_manager = Platform::Current()->GetGpuMemoryBufferManager();
    } else {
      // Get the GPU Buffer Manager by jumping to the main thread and blocking.
      // This normally occurs for the first frame only. In case of failures
      // only kMaxFailures attempts are made.
      // The purpose of blocking until the value is read is to avoid forwarding
      // unconverted frames, which cause the WebRTC sink to fall back to
      // software encoding.
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
    if (!gmb_manager) {
      convert_to_nv12_gmb_failure_count_++;
      return absl::nullopt;
    }

    CreateAcceleratedFramePool(gmb_manager);
    if (!accelerated_frame_pool_) {
      convert_to_nv12_gmb_failure_count_++;
      return absl::nullopt;
    }
  }
  DCHECK(accelerated_frame_pool_);

  auto resolver =
      WrapPersistent(MakeGarbageCollected<ScriptPromiseResolver>(script_state));
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
    Persistent<ScriptPromiseResolver> resolver,
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
