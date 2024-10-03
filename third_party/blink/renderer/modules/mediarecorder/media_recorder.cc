// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"

#include <algorithm>
#include <limits>

#include "base/time/time.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_error_event_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediarecorder/blob_event.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

struct MediaRecorderBitrates {
  const std::optional<uint32_t> audio_bps;
  const std::optional<uint32_t> video_bps;
  const std::optional<uint32_t> overall_bps;
};

// Boundaries of Opus SILK bitrate from https://www.opus-codec.org/.
const int kSmallestPossibleOpusBitRate = 5000;
const int kLargestPossibleOpusBitRate = 510000;

// Smallest Vpx bitrate that can be requested.
// 75kbps is the min bitrate recommended by VP9 VOD settings for 320x240 videos.
const int kSmallestPossibleVpxBitRate = 75000;

// Both values come from YouTube recommended upload encoding settings and are
// used by other browser vendors. See
// https://support.google.com/youtube/answer/1722171?hl=en#zippy=%2Cbitrate
const int kDefaultVideoBitRate = 2500e3;  // 2.5Mbps
const int kDefaultAudioBitRate = 128e3;   // 128kbps

String StateToString(MediaRecorder::State state) {
  switch (state) {
    case MediaRecorder::State::kInactive:
      return "inactive";
    case MediaRecorder::State::kRecording:
      return "recording";
    case MediaRecorder::State::kPaused:
      return "paused";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

String BitrateModeToString(AudioTrackRecorder::BitrateMode bitrateMode) {
  switch (bitrateMode) {
    case AudioTrackRecorder::BitrateMode::kConstant:
      return "constant";
    case AudioTrackRecorder::BitrateMode::kVariable:
      return "variable";
  }

  NOTREACHED_IN_MIGRATION();
  return String();
}

AudioTrackRecorder::BitrateMode GetBitrateModeFromOptions(
    const MediaRecorderOptions* const options) {
  if (options->hasAudioBitrateMode()) {
    if (options->audioBitrateMode() == V8BitrateMode::Enum::kConstant) {
      return AudioTrackRecorder::BitrateMode::kConstant;
    }
  }

  return AudioTrackRecorder::BitrateMode::kVariable;
}

void LogConsoleMessage(ExecutionContext* context, const String& message) {
  context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning, message));
}

uint32_t ClampAudioBitRate(ExecutionContext* context, uint32_t audio_bps) {
  if (audio_bps > kLargestPossibleOpusBitRate) {
    LogConsoleMessage(
        context,
        String::Format(
            "Clamping calculated audio bitrate (%dbps) to the maximum (%dbps)",
            audio_bps, kLargestPossibleOpusBitRate));
    return kLargestPossibleOpusBitRate;
  }
  if (audio_bps < kSmallestPossibleOpusBitRate) {
    LogConsoleMessage(
        context,
        String::Format(
            "Clamping calculated audio bitrate (%dbps) to the minimum (%dbps)",
            audio_bps, kSmallestPossibleOpusBitRate));
    return kSmallestPossibleOpusBitRate;
  }
  return audio_bps;
}

uint32_t ClampVideoBitRate(ExecutionContext* context, uint32_t video_bps) {
  if (video_bps < kSmallestPossibleVpxBitRate) {
    LogConsoleMessage(
        context,
        String::Format(
            "Clamping calculated video bitrate (%dbps) to the minimum (%dbps)",
            video_bps, kSmallestPossibleVpxBitRate));
    return kSmallestPossibleVpxBitRate;
  }
  return video_bps;
}

// Allocates the requested bit rates from |options| into the respective
// |{audio,video}_bps| (where a value of zero indicates Platform to use
// whatever it sees fit). If |options.bitsPerSecond()| is specified, it
// overrides any specific bitrate, and the UA is free to allocate as desired:
// here a 90%/10% video/audio is used. In all cases where a value is explicited
// or calculated, values are clamped in sane ranges.
// This method throws NotSupportedError.
MediaRecorderBitrates GetBitratesFromOptions(
    ExceptionState& exception_state,
    ExecutionContext* context,
    const MediaRecorderOptions* options) {
  // Clamp incoming values into a signed integer's range.
  // TODO(mcasas): This section would no be needed if the bit rates are signed
  // or double, see https://github.com/w3c/mediacapture-record/issues/48.
  constexpr uint32_t kMaxIntAsUnsigned = std::numeric_limits<int>::max();

  std::optional<uint32_t> audio_bps;
  if (options->hasAudioBitsPerSecond()) {
    audio_bps = std::min(options->audioBitsPerSecond(), kMaxIntAsUnsigned);
  }
  std::optional<uint32_t> video_bps;
  if (options->hasVideoBitsPerSecond()) {
    video_bps = std::min(options->videoBitsPerSecond(), kMaxIntAsUnsigned);
  }
  std::optional<uint32_t> overall_bps;
  if (options->hasBitsPerSecond()) {
    overall_bps = std::min(options->bitsPerSecond(), kMaxIntAsUnsigned);
    audio_bps = ClampAudioBitRate(context, overall_bps.value() / 10);
    video_bps = overall_bps.value() >= audio_bps.value()
                    ? overall_bps.value() - audio_bps.value()
                    : 0u;
    video_bps = ClampVideoBitRate(context, video_bps.value());
  }

  return {audio_bps, video_bps, overall_bps};
}

}  // namespace

MediaRecorder* MediaRecorder::Create(ExecutionContext* context,
                                     MediaStream* stream,
                                     ExceptionState& exception_state) {
  return MakeGarbageCollected<MediaRecorder>(
      context, stream, MediaRecorderOptions::Create(), exception_state);
}

MediaRecorder* MediaRecorder::Create(ExecutionContext* context,
                                     MediaStream* stream,
                                     const MediaRecorderOptions* options,
                                     ExceptionState& exception_state) {
  return MakeGarbageCollected<MediaRecorder>(context, stream, options,
                                             exception_state);
}

MediaRecorder::MediaRecorder(ExecutionContext* context,
                             MediaStream* stream,
                             const MediaRecorderOptions* options,
                             ExceptionState& exception_state)
    : ActiveScriptWrappable<MediaRecorder>({}),
      ExecutionContextLifecycleObserver(context),
      stream_(stream),
      mime_type_(options->mimeType()) {
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return;
  }
  if (options->hasVideoKeyFrameIntervalDuration() &&
      options->hasVideoKeyFrameIntervalCount()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Both videoKeyFrameIntervalDuration and videoKeyFrameIntervalCount "
        "can't be specified.");
    return;
  }
  KeyFrameRequestProcessor::Configuration key_frame_config;
  if (options->hasVideoKeyFrameIntervalDuration()) {
    key_frame_config =
        base::Milliseconds(options->videoKeyFrameIntervalDuration());
  } else if (options->hasVideoKeyFrameIntervalCount()) {
    key_frame_config = options->videoKeyFrameIntervalCount();
  }
  recorder_handler_ = MakeGarbageCollected<MediaRecorderHandler>(
      context->GetTaskRunner(TaskType::kInternalMediaRealTime),
      key_frame_config);
  if (!recorder_handler_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "No MediaRecorder handler can be created.");
    return;
  }

  const MediaRecorderBitrates bitrates =
      GetBitratesFromOptions(exception_state, context, options);
  const ContentType content_type(mime_type_);
  if (!recorder_handler_->Initialize(this, stream->Descriptor(),
                                     content_type.GetType(),
                                     content_type.Parameter("codecs"),
                                     GetBitrateModeFromOptions(options))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Failed to initialize native MediaRecorder the type provided (" +
            mime_type_ + ") is not supported.");
  }

  audio_bits_per_second_ = bitrates.audio_bps.value_or(kDefaultAudioBitRate);
  video_bits_per_second_ = bitrates.video_bps.value_or(kDefaultVideoBitRate);
  overall_bits_per_second_ = bitrates.overall_bps;
}

MediaRecorder::~MediaRecorder() = default;

String MediaRecorder::state() const {
  return StateToString(state_);
}

String MediaRecorder::audioBitrateMode() const {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    // Return a valid enum value; variable is the default.
    return BitrateModeToString(AudioTrackRecorder::BitrateMode::kVariable);
  }
  DCHECK(recorder_handler_);
  return BitrateModeToString(recorder_handler_->AudioBitrateMode());
}

void MediaRecorder::start(ExceptionState& exception_state) {
  start(std::numeric_limits<int>::max() /* timeSlice */, exception_state);
}

void MediaRecorder::start(int time_slice, ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ != State::kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The MediaRecorder's state is '" + StateToString(state_) + "'.");
    return;
  }

  if (stream_->getTracks().size() == 0) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The MediaRecorder cannot start because"
                                      "there are no audio or video tracks "
                                      "available.");
    return;
  }

  state_ = State::kRecording;

  if (stream_->getAudioTracks().size() == 0) {
    audio_bits_per_second_ = 0;
    if (overall_bits_per_second_.has_value()) {
      video_bits_per_second_ = ClampVideoBitRate(
          GetExecutionContext(), overall_bits_per_second_.value());
    }
  }

  if (stream_->getVideoTracks().size() == 0) {
    video_bits_per_second_ = 0;
    if (overall_bits_per_second_.has_value()) {
      audio_bits_per_second_ = ClampAudioBitRate(
          GetExecutionContext(), overall_bits_per_second_.value());
    }
  }

  const ContentType content_type(mime_type_);
  if (!recorder_handler_->Start(time_slice, content_type.GetType(),
                                audio_bits_per_second_,
                                video_bits_per_second_)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "There was an error starting the MediaRecorder.");
  }
}

void MediaRecorder::stop(ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ == State::kInactive) {
    return;
  }

  StopRecording(/*error_event=*/nullptr);
}

void MediaRecorder::pause(ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ == State::kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The MediaRecorder's state is '" + StateToString(state_) + "'.");
    return;
  }
  if (state_ == State::kPaused)
    return;

  state_ = State::kPaused;

  recorder_handler_->Pause();

  ScheduleDispatchEvent(Event::Create(event_type_names::kPause));
}

void MediaRecorder::resume(ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ == State::kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The MediaRecorder's state is '" + StateToString(state_) + "'.");
    return;
  }
  if (state_ == State::kRecording)
    return;

  state_ = State::kRecording;

  recorder_handler_->Resume();
  ScheduleDispatchEvent(Event::Create(event_type_names::kResume));
}

void MediaRecorder::requestData(ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ == State::kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The MediaRecorder's state is '" + StateToString(state_) + "'.");
    return;
  }

  WriteData(/*data=*/{}, /*last_in_slice=*/true, /*error_event=*/nullptr);
}

bool MediaRecorder::isTypeSupported(ExecutionContext* context,
                                    const String& type) {
  MediaRecorderHandler* handler = MakeGarbageCollected<MediaRecorderHandler>(
      context->GetTaskRunner(TaskType::kInternalMediaRealTime),
      KeyFrameRequestProcessor::Configuration());
  if (!handler)
    return false;

  // If true is returned from this method, it only indicates that the
  // MediaRecorder implementation is capable of recording Blob objects for the
  // specified MIME type. Recording may still fail if sufficient resources are
  // not available to support the concrete media encoding.
  // https://w3c.github.io/mediacapture-record/#dom-mediarecorder-istypesupported
  ContentType content_type(type);
  bool result = handler->CanSupportMimeType(content_type.GetType(),
                                            content_type.Parameter("codecs"));
  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kMediaRecorder_IsTypeSupported)) {
    blink::IdentifiabilityMetricBuilder(context->UkmSourceID())
        .Add(blink::IdentifiableSurface::FromTypeAndToken(
                 blink::IdentifiableSurface::Type::
                     kMediaRecorder_IsTypeSupported,
                 IdentifiabilityBenignStringToken(type)),
             result)
        .Record(context->UkmRecorder());
  }

  return result;
}

const AtomicString& MediaRecorder::InterfaceName() const {
  return event_target_names::kMediaRecorder;
}

ExecutionContext* MediaRecorder::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void MediaRecorder::ContextDestroyed() {
  if (blob_data_) {
    // Cache |blob_data_->length()| because of std::move in argument list.
    const uint64_t blob_data_length = blob_data_->length();
    CreateBlobEvent(MakeGarbageCollected<Blob>(
        BlobDataHandle::Create(std::move(blob_data_), blob_data_length)));
  }

  state_ = State::kInactive;
  stream_.Clear();
  if (recorder_handler_) {
    recorder_handler_->Stop();
    recorder_handler_ = nullptr;
  }
}

void MediaRecorder::WriteData(base::span<const uint8_t> data,
                              bool last_in_slice,
                              ErrorEvent* error_event) {
  if (!first_write_received_) {
    mime_type_ = recorder_handler_->ActualMimeType();
    ScheduleDispatchEvent(Event::Create(event_type_names::kStart));
    first_write_received_ = true;
  }

  if (error_event) {
    ScheduleDispatchEvent(error_event);
  }

  if (!blob_data_) {
    blob_data_ = std::make_unique<BlobData>();
    blob_data_->SetContentType(mime_type_);
  }
  if (!data.empty()) {
    blob_data_->AppendBytes(data);
  }

  if (!last_in_slice)
    return;

  // Cache |blob_data_->length()| because of std::move in argument list.
  const uint64_t blob_data_length = blob_data_->length();
  CreateBlobEvent(MakeGarbageCollected<Blob>(
      BlobDataHandle::Create(std::move(blob_data_), blob_data_length)));
}

void MediaRecorder::OnError(DOMExceptionCode code, const String& message) {
  DVLOG(1) << __func__ << " message=" << message.Ascii();

  ScriptState* script_state =
      ToScriptStateForMainWorld(DomWindow()->GetFrame());
  ScriptState::Scope scope(script_state);
  ScriptValue error_value = ScriptValue::From(
      script_state, MakeGarbageCollected<DOMException>(code, message));
  ErrorEventInit* event_init = ErrorEventInit::Create();
  event_init->setError(error_value);
  StopRecording(
      ErrorEvent::Create(script_state, event_type_names::kError, event_init));
}

void MediaRecorder::OnAllTracksEnded() {
  DVLOG(1) << __func__;
  StopRecording(/*error_event=*/nullptr);
}

void MediaRecorder::OnStreamChanged(const String& message) {
  DVLOG(1) << __func__ << " message=" << message.Ascii()
           << " state_=" << static_cast<int>(state_);
  if (state_ != State::kInactive) {
    OnError(DOMExceptionCode::kInvalidModificationError, message);
  }
}

void MediaRecorder::CreateBlobEvent(Blob* blob) {
  const base::TimeTicks now = base::TimeTicks::Now();
  double timecode = 0;
  if (!blob_event_first_chunk_timecode_.has_value()) {
    blob_event_first_chunk_timecode_ = now;
  } else {
    timecode =
        (now - blob_event_first_chunk_timecode_.value()).InMillisecondsF();
  }

  ScheduleDispatchEvent(MakeGarbageCollected<BlobEvent>(
      event_type_names::kDataavailable, blob, timecode));
}

void MediaRecorder::StopRecording(ErrorEvent* error_event) {
  if (state_ == State::kInactive) {
    // This may happen if all tracks have ended and recording has stopped or
    // never started.
    return;
  }
  if (!recorder_handler_) {
    // This may happen when ContextDestroyed has executed, but the
    // MediaRecorderHandler still exists and all tracks
    // have ended leading to a call to OnAllTracksEnded.
    return;
  }
  // Make sure that starting the recorder again yields an onstart event.
  state_ = State::kInactive;

  recorder_handler_->Stop();
  WriteData(/*data=*/{}, /*last_in_slice=*/true, error_event);
  ScheduleDispatchEvent(Event::Create(event_type_names::kStop));
  first_write_received_ = false;
}

void MediaRecorder::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);
  // Only schedule a post if we are placing the first item in the queue.
  if (scheduled_events_.size() == 1) {
    if (auto* context = GetExecutionContext()) {
      // MediaStream recording should use DOM manipulation task source.
      // https://www.w3.org/TR/mediastream-recording/
      context->GetTaskRunner(TaskType::kDOMManipulation)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(&MediaRecorder::DispatchScheduledEvent,
                                   WrapPersistent(this)));
    }
  }
}

void MediaRecorder::DispatchScheduledEvent() {
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  for (const auto& event : events)
    DispatchEvent(*event);
}

void MediaRecorder::Trace(Visitor* visitor) const {
  visitor->Trace(stream_);
  visitor->Trace(recorder_handler_);
  visitor->Trace(scheduled_events_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void MediaRecorder::UpdateAudioBitrate(uint32_t bits_per_second) {
  audio_bits_per_second_ = bits_per_second;
}

}  // namespace blink
