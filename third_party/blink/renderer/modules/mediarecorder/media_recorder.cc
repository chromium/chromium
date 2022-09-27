// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/media_recorder.h"

#include <algorithm>
#include <limits>
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/mediarecorder/blob_event.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/privacy_budget/identifiability_digest_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// Boundaries of Opus bitrate from https://www.opus-codec.org/.
const int kSmallestPossibleOpusBitRate = 6000;
const int kLargestAutoAllocatedOpusBitRate = 128000;

// Smallest Vpx bitrate that can be requested.
const int kSmallestPossibleVpxBitRate = 100000;

String StateToString(MediaRecorder::State state) {
  switch (state) {
    case MediaRecorder::State::kInactive:
      return "inactive";
    case MediaRecorder::State::kRecording:
      return "recording";
    case MediaRecorder::State::kPaused:
      return "paused";
  }

  NOTREACHED();
  return String();
}

String BitrateModeToString(AudioTrackRecorder::BitrateMode bitrateMode) {
  switch (bitrateMode) {
    case AudioTrackRecorder::BitrateMode::kConstant:
      return "constant";
    case AudioTrackRecorder::BitrateMode::kVariable:
      return "variable";
  }

  NOTREACHED();
  return String();
}

AudioTrackRecorder::BitrateMode GetBitrateModeFromOptions(
    const MediaRecorderOptions* const options) {
  if (options->hasAudioBitrateMode()) {
    if (!WTF::CodeUnitCompareIgnoringASCIICase(options->audioBitrateMode(),
                                               "constant"))
      return AudioTrackRecorder::BitrateMode::kConstant;
  }

  return AudioTrackRecorder::BitrateMode::kVariable;
}

// Allocates the requested bit rates from |bitrateOptions| into the respective
// |{audio,video}BitsPerSecond| (where a value of zero indicates Platform to use
// whatever it sees fit). If |options.bitsPerSecond()| is specified, it
// overrides any specific bitrate, and the UA is free to allocate as desired:
// here a 90%/10% video/audio is used. In all cases where a value is explicited
// or calculated, values are clamped in sane ranges.
// This method throws NotSupportedError.
void AllocateVideoAndAudioBitrates(ExceptionState& exception_state,
                                   ExecutionContext* context,
                                   const MediaRecorderOptions* options,
                                   MediaStream* stream,
                                   uint32_t* audio_bits_per_second,
                                   uint32_t* video_bits_per_second) {
  const bool use_video = !stream->getVideoTracks().empty();
  const bool use_audio = !stream->getAudioTracks().empty();

  // Clamp incoming values into a signed integer's range.
  // TODO(mcasas): This section would no be needed if the bit rates are signed
  // or double, see https://github.com/w3c/mediacapture-record/issues/48.
  constexpr uint32_t kMaxIntAsUnsigned = std::numeric_limits<int>::max();

  uint32_t overall_bps = 0;
  if (options->hasBitsPerSecond())
    overall_bps = std::min(options->bitsPerSecond(), kMaxIntAsUnsigned);
  uint32_t video_bps = 0;
  if (options->hasVideoBitsPerSecond() && use_video)
    video_bps = std::min(options->videoBitsPerSecond(), kMaxIntAsUnsigned);
  uint32_t audio_bps = 0;
  if (options->hasAudioBitsPerSecond() && use_audio)
    audio_bps = std::min(options->audioBitsPerSecond(), kMaxIntAsUnsigned);

  if (use_audio) {
    // |overallBps| overrides the specific audio and video bit rates.
    if (options->hasBitsPerSecond()) {
      if (use_video)
        audio_bps = overall_bps / 10;
      else
        audio_bps = overall_bps;
    }
    // Limit audio bitrate values if set explicitly or calculated.
    if (options->hasAudioBitsPerSecond() || options->hasBitsPerSecond()) {
      if (audio_bps > kLargestAutoAllocatedOpusBitRate) {
        context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Clamping calculated audio bitrate (" + String::Number(audio_bps) +
                "bps) to the maximum (" +
                String::Number(kLargestAutoAllocatedOpusBitRate) + "bps)"));
        audio_bps = kLargestAutoAllocatedOpusBitRate;
      }

      if (audio_bps < kSmallestPossibleOpusBitRate) {
        context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Clamping calculated audio bitrate (" + String::Number(audio_bps) +
                "bps) to the minimum (" +
                String::Number(kSmallestPossibleOpusBitRate) + "bps)"));
        audio_bps = kSmallestPossibleOpusBitRate;
      }
    } else {
      DCHECK(!audio_bps);
    }
  }

  if (use_video) {
    // Allocate the remaining |overallBps|, if any, to video.
    if (options->hasBitsPerSecond())
      video_bps = overall_bps >= audio_bps ? overall_bps - audio_bps : 0u;

    // Clamp the video bit rate. Avoid clamping if the user has not set it
    // explicitly.
    if (options->hasVideoBitsPerSecond() || options->hasBitsPerSecond()) {
      if (video_bps < kSmallestPossibleVpxBitRate) {
        context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Clamping calculated video bitrate (" + String::Number(video_bps) +
                "bps) to the minimum (" +
                String::Number(kSmallestPossibleVpxBitRate) + "bps)"));
        video_bps = kSmallestPossibleVpxBitRate;
      }
    } else {
      DCHECK(!video_bps);
    }
  }

  *video_bits_per_second = video_bps;
  *audio_bits_per_second = audio_bps;
  return;
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
    : ExecutionContextLifecycleObserver(context),
      stream_(stream),
      mime_type_(options->mimeType()),
      audio_bits_per_second_(0),
      video_bits_per_second_(0),
      state_(State::kInactive) {
  if (context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Execution context is detached.");
    return;
  }
  recorder_handler_ = MakeGarbageCollected<MediaRecorderHandler>(
      context->GetTaskRunner(TaskType::kInternalMediaRealTime));
  if (!recorder_handler_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "No MediaRecorder handler can be created.");
    return;
  }

  AllocateVideoAndAudioBitrates(exception_state, context, options, stream,
                                &audio_bits_per_second_,
                                &video_bits_per_second_);

  const ContentType content_type(mime_type_);
  if (!recorder_handler_->Initialize(
          this, stream->Descriptor(), content_type.GetType(),
          content_type.Parameter("codecs"), audio_bits_per_second_,
          video_bits_per_second_, GetBitrateModeFromOptions(options))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Failed to initialize native MediaRecorder the type provided (" +
            mime_type_ + ") is not supported.");
  }
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
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
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
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "The MediaRecorder cannot start because"
                                      "there are no audio or video tracks "
                                      "available.");
    return;
  }

  state_ = State::kRecording;

  if (!recorder_handler_->Start(time_slice)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kUnknownError,
        "There was an error starting the MediaRecorder.");
  }
}

void MediaRecorder::stop(ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ == State::kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The MediaRecorder's state is '" + StateToString(state_) + "'.");
    return;
  }

  StopRecording();
}

void MediaRecorder::pause(ExceptionState& exception_state) {
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
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
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
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
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "Execution context is detached.");
    return;
  }
  if (state_ == State::kInactive) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The MediaRecorder's state is '" + StateToString(state_) + "'.");
    return;
  }
  WriteData(nullptr /* data */, 0 /* length */, true /* lastInSlice */,
            base::Time::Now().ToDoubleT() * 1000.0);
}

bool MediaRecorder::isTypeSupported(ExecutionContext* context,
                                    const String& type) {
  MediaRecorderHandler* handler = MakeGarbageCollected<MediaRecorderHandler>(
      context->GetTaskRunner(TaskType::kInternalMediaRealTime));
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
    CreateBlobEvent(MakeGarbageCollected<Blob>(BlobDataHandle::Create(
                        std::move(blob_data_), blob_data_length)),
                    base::Time::Now().ToDoubleT() * 1000.0);
  }

  state_ = State::kInactive;
  stream_.Clear();
  recorder_handler_->Stop();
  recorder_handler_ = nullptr;
}

void MediaRecorder::WriteData(const char* data,
                              size_t length,
                              bool last_in_slice,
                              double timecode) {
  // Update mime_type_ when "onstart" is sent by the MediaRecorder. This method
  // is used also from StopRecording, with a zero length. If we never wrote
  // anything we don't want to send start or associated actions (update the mime
  // type in that case).
  if (!first_write_received_ && length) {
    mime_type_ = recorder_handler_->ActualMimeType();
    ScheduleDispatchEvent(Event::Create(event_type_names::kStart));
    first_write_received_ = true;
  }

  if (!blob_data_) {
    blob_data_ = std::make_unique<BlobData>();
    blob_data_->SetContentType(mime_type_);
  }
  if (data)
    blob_data_->AppendBytes(data, length);

  if (!last_in_slice)
    return;

  // Cache |blob_data_->length()| because of std::move in argument list.
  const uint64_t blob_data_length = blob_data_->length();
  CreateBlobEvent(MakeGarbageCollected<Blob>(BlobDataHandle::Create(
                      std::move(blob_data_), blob_data_length)),
                  timecode);
}

void MediaRecorder::OnError(const String& message) {
  DLOG(ERROR) << message.Ascii();
  StopRecording();
  ScheduleDispatchEvent(Event::Create(event_type_names::kError));
}

void MediaRecorder::OnAllTracksEnded() {
  StopRecording();
}

void MediaRecorder::CreateBlobEvent(Blob* blob, double timecode) {
  ScheduleDispatchEvent(MakeGarbageCollected<BlobEvent>(
      event_type_names::kDataavailable, blob, timecode));
}

void MediaRecorder::StopRecording() {
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
  first_write_received_ = false;
  state_ = State::kInactive;

  recorder_handler_->Stop();

  WriteData(nullptr /* data */, 0 /* length */, true /* lastInSlice */,
            base::Time::Now().ToDoubleT() * 1000.0);
  ScheduleDispatchEvent(Event::Create(event_type_names::kStop));
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
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
