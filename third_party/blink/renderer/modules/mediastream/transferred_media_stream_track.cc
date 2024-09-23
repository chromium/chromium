// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_double_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_point_2d.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/imagecapture/image_capture.h"
#include "third_party/blink/renderer/modules/mediastream/apply_constraints_request.h"
#include "third_party/blink/renderer/modules/mediastream/browser_capture_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_video_stats.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/mediastream/processed_local_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/webaudio_media_stream_audio_sink.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_web_audio_source.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

TransferredMediaStreamTrack::TransferredMediaStreamTrack(
    ExecutionContext* execution_context,
    const TransferredValues& data)
    : transferred_component_(
          MakeGarbageCollected<TransferredMediaStreamComponent>(
              TransferredMediaStreamComponent::TransferredValues{.id =
                                                                     data.id})),
      execution_context_(execution_context),
      data_(data) {}

String TransferredMediaStreamTrack::kind() const {
  if (track_) {
    return track_->kind();
  }
  return data_.kind;
}

String TransferredMediaStreamTrack::id() const {
  if (track_) {
    return track_->id();
  }
  return data_.id;
}

String TransferredMediaStreamTrack::label() const {
  if (track_) {
    return track_->label();
  }
  return data_.label;
}

bool TransferredMediaStreamTrack::enabled() const {
  if (track_) {
    return track_->enabled();
  }
  return data_.enabled;
}

void TransferredMediaStreamTrack::setEnabled(bool enabled) {
  if (track_) {
    track_->setEnabled(enabled);
    return;
  }
  setter_call_order_.push_back(SET_ENABLED);
  enabled_state_list_.push_back(enabled);
}

bool TransferredMediaStreamTrack::muted() const {
  if (track_) {
    return track_->muted();
  }
  return data_.muted;
}

String TransferredMediaStreamTrack::ContentHint() const {
  if (track_) {
    return track_->ContentHint();
  }
  return ContentHintToString(data_.content_hint);
}

void TransferredMediaStreamTrack::SetContentHint(const String& content_hint) {
  if (track_) {
    track_->SetContentHint(content_hint);
    return;
  }
  setter_call_order_.push_back(SET_CONTENT_HINT);
  content_hint_list_.push_back(content_hint);
}

String TransferredMediaStreamTrack::readyState() const {
  if (track_) {
    return track_->readyState();
  }
  return ReadyStateToString(data_.ready_state);
}

MediaStreamTrack* TransferredMediaStreamTrack::clone(
    ExecutionContext* execution_context) {
  if (track_) {
    return track_->clone(execution_context);
  }

  auto* cloned_tmst = MakeGarbageCollected<TransferredMediaStreamTrack>(
      execution_context, data_);

  setter_call_order_.push_back(CLONE);
  clone_list_.push_back(cloned_tmst);
  return cloned_tmst;
}

void TransferredMediaStreamTrack::stopTrack(
    ExecutionContext* execution_context) {
  if (track_) {
    track_->stopTrack(execution_context);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

MediaTrackCapabilities* TransferredMediaStreamTrack::getCapabilities() const {
  if (track_) {
    return track_->getCapabilities();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return MediaTrackCapabilities::Create();
}

MediaTrackConstraints* TransferredMediaStreamTrack::getConstraints() const {
  if (track_) {
    return track_->getConstraints();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return MediaTrackConstraints::Create();
}

MediaTrackSettings* TransferredMediaStreamTrack::getSettings() const {
  if (track_) {
    return track_->getSettings();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return MediaTrackSettings::Create();
}

V8UnionMediaStreamTrackAudioStatsOrMediaStreamTrackVideoStats*
TransferredMediaStreamTrack::stats() {
  if (track_) {
    return track_->stats();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return nullptr;
}

CaptureHandle* TransferredMediaStreamTrack::getCaptureHandle() const {
  if (track_) {
    return track_->getCaptureHandle();
  }
  // TODO(https://crbug.com/1288839): return the transferred value.
  return CaptureHandle::Create();
}

ScriptPromise<IDLUndefined> TransferredMediaStreamTrack::applyConstraints(
    ScriptState* script_state,
    const MediaTrackConstraints* constraints) {
  if (track_) {
    return track_->applyConstraints(script_state, constraints);
  }
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  auto promise = resolver->Promise();
  applyConstraints(resolver, constraints);
  return promise;
}

void TransferredMediaStreamTrack::applyConstraints(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const MediaTrackConstraints* constraints) {
  setter_call_order_.push_back(APPLY_CONSTRAINTS);
  constraints_list_.push_back(
      MakeGarbageCollected<ConstraintsPair>(resolver, constraints));
}

void TransferredMediaStreamTrack::SetImplementation(MediaStreamTrack* track) {
  track_ = track;
  transferred_component_.Clear();

  // Replaying mutations which happened before this point.
  for (const auto& setter_function : setter_call_order_) {
    switch (setter_function) {
      case APPLY_CONSTRAINTS: {
        const auto& entry = constraints_list_.front();
        track->applyConstraints(entry->resolver, entry->constraints);
        constraints_list_.pop_front();
        break;
      }
      case SET_CONTENT_HINT: {
        track->SetContentHint(content_hint_list_.front());
        content_hint_list_.pop_front();
        break;
      }
      case SET_ENABLED: {
        track->setEnabled(enabled_state_list_.front());
        enabled_state_list_.pop_front();
        break;
      }
      case CLONE: {
        MediaStreamTrack* real_track_clone = track->clone(execution_context_);
        clone_list_.front()->SetImplementation(real_track_clone);
        clone_list_.pop_front();
        break;
      }
    }
  }

  // Set up an EventPropagator helper to forward any events fired on track so
  // that they're re-dispatched to anything that's listening on this.
  event_propagator_ = MakeGarbageCollected<EventPropagator>(track, this);

  // Observers may dispatch events which create and add new Observers. Such
  // observers are added directly to the implementation track since track_ is
  // now set.
  for (auto observer : observers_) {
    observer->TrackChangedState();
    track_->AddObserver(observer);
  }
  observers_.clear();
}

void TransferredMediaStreamTrack::SetComponentImplementation(
    MediaStreamComponent* component) {
  transferred_component_->SetImplementation(component);
}

void TransferredMediaStreamTrack::SetInitialConstraints(
    const MediaConstraints& constraints) {
  if (track_) {
    track_->SetInitialConstraints(constraints);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

void TransferredMediaStreamTrack::SetConstraints(
    const MediaConstraints& constraints) {
  if (track_) {
    track_->SetConstraints(constraints);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

MediaStreamSource::ReadyState TransferredMediaStreamTrack::GetReadyState() {
  if (track_) {
    return track_->GetReadyState();
  }
  return data_.ready_state;
}

MediaStreamComponent* TransferredMediaStreamTrack::Component() const {
  if (track_) {
    return track_->Component();
  }
  return transferred_component_.Get();
}

bool TransferredMediaStreamTrack::Ended() const {
  if (track_) {
    return track_->Ended();
  }
  return (data_.ready_state == MediaStreamSource::kReadyStateEnded);
}

void TransferredMediaStreamTrack::RegisterMediaStream(MediaStream* stream) {
  if (track_) {
    track_->RegisterMediaStream(stream);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

void TransferredMediaStreamTrack::UnregisterMediaStream(MediaStream* stream) {
  if (track_) {
    track_->UnregisterMediaStream(stream);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

// EventTarget
const AtomicString& TransferredMediaStreamTrack::InterfaceName() const {
  // TODO(https://crbug.com/1288839): Should TMST have its own interface name?
  return event_target_names::kMediaStreamTrack;
}

ExecutionContext* TransferredMediaStreamTrack::GetExecutionContext() const {
  return execution_context_.Get();
}

void TransferredMediaStreamTrack::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (track_) {
    return track_->AddedEventListener(event_type, registered_listener);
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

bool TransferredMediaStreamTrack::HasPendingActivity() const {
  if (track_) {
    return track_->HasPendingActivity();
  }
  return false;
}

std::unique_ptr<AudioSourceProvider>
TransferredMediaStreamTrack::CreateWebAudioSource(
    int context_sample_rate,
    base::TimeDelta platform_buffer_duration) {
  if (track_) {
    return track_->CreateWebAudioSource(context_sample_rate,
                                        platform_buffer_duration);
  }
  // TODO(https://crbug.com/1288839): Create one based on transferred data?
  return nullptr;
}

ImageCapture* TransferredMediaStreamTrack::GetImageCapture() {
  if (track_) {
    return track_->GetImageCapture();
  }
  // TODO(https://crbug.com/1288839): Create one based on transferred data?
  return nullptr;
}

std::optional<const MediaStreamDevice> TransferredMediaStreamTrack::device()
    const {
  if (track_) {
    return track_->device();
  }
  // TODO(https://crbug.com/1288839): Return transferred data
  return std::nullopt;
}

void TransferredMediaStreamTrack::BeingTransferred(
    const base::UnguessableToken& transfer_id) {
  if (track_) {
    track_->BeingTransferred(transfer_id);
    stopTrack(GetExecutionContext());
    return;
  }
  // TODO(https://crbug.com/1288839): Save and forward to track_ once it's
  // initialized.
}

bool TransferredMediaStreamTrack::TransferAllowed(String& message) const {
  if (track_) {
    return track_->TransferAllowed(message);
  }
  return clone_list_.empty();
}

void TransferredMediaStreamTrack::AddObserver(Observer* observer) {
  if (track_) {
    track_->AddObserver(observer);
  } else {
    observers_.insert(observer);
  }
}

TransferredMediaStreamTrack::EventPropagator::EventPropagator(
    MediaStreamTrack* underlying_track,
    TransferredMediaStreamTrack* transferred_track)
    : transferred_track_(transferred_track) {
  DCHECK(underlying_track);
  DCHECK(transferred_track);
  underlying_track->addEventListener(event_type_names::kMute, this);
  underlying_track->addEventListener(event_type_names::kUnmute, this);
  underlying_track->addEventListener(event_type_names::kEnded, this);
  underlying_track->addEventListener(event_type_names::kCapturehandlechange,
                                     this);
}

void TransferredMediaStreamTrack::EventPropagator::Invoke(ExecutionContext*,
                                                          Event* event) {
  transferred_track_->DispatchEvent(*event);
}

void TransferredMediaStreamTrack::EventPropagator::Trace(
    Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  visitor->Trace(transferred_track_);
}

void TransferredMediaStreamTrack::Trace(Visitor* visitor) const {
  MediaStreamTrack::Trace(visitor);
  visitor->Trace(transferred_component_);
  visitor->Trace(track_);
  visitor->Trace(execution_context_);
  visitor->Trace(event_propagator_);
  visitor->Trace(observers_);
  visitor->Trace(constraints_list_);
  visitor->Trace(clone_list_);
}

TransferredMediaStreamTrack::ConstraintsPair::ConstraintsPair(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    const MediaTrackConstraints* constraints)
    : resolver(resolver), constraints(constraints) {}

void TransferredMediaStreamTrack::ConstraintsPair::Trace(
    Visitor* visitor) const {
  visitor->Trace(resolver);
  visitor->Trace(constraints);
}

}  // namespace blink
