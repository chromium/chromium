// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/mediastream/media_constraints_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_error_state.h"
#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"

namespace blink {

namespace {

class GetOpenDeviceRequestCallbacks final : public UserMediaRequest::Callbacks {
 public:
  ~GetOpenDeviceRequestCallbacks() override = default;

  void OnSuccess(const MediaStreamVector& streams) override {}
  void OnError(ScriptWrappable* callback_this_value,
               const V8MediaStreamError* error) override {}
};

}  // namespace

String ContentHintToString(
    const WebMediaStreamTrack::ContentHintType& content_hint) {
  switch (content_hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      return kContentHintStringNone;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
      return kContentHintStringAudioSpeech;
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      return kContentHintStringAudioMusic;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
      return kContentHintStringVideoMotion;
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
      return kContentHintStringVideoDetail;
    case WebMediaStreamTrack::ContentHintType::kVideoText:
      return kContentHintStringVideoText;
  }
  NOTREACHED();
  return kContentHintStringNone;
}

String ReadyStateToString(const MediaStreamSource::ReadyState& ready_state) {
  // Although muted is tracked as a ReadyState, only "live" and "ended" are
  // visible externally.
  switch (ready_state) {
    case MediaStreamSource::kReadyStateLive:
    case MediaStreamSource::kReadyStateMuted:
      return "live";
    case MediaStreamSource::kReadyStateEnded:
      return "ended";
  }
  NOTREACHED();
  return String();
}

// static
MediaStreamTrack* MediaStreamTrack::FromTransferredState(
    ScriptState* script_state,
    const TransferredValues& data) {
  DCHECK(data.track_impl_subtype);

  // Allow injecting a mock.
  if (GetFromTransferredStateImplForTesting()) {
    return GetFromTransferredStateImplForTesting().Run(data);
  }

  auto* window =
      DynamicTo<LocalDOMWindow>(ExecutionContext::From(script_state));
  if (!window)
    return nullptr;

  UserMediaClient* user_media_client = UserMediaClient::From(window);
  if (!user_media_client) {
    return nullptr;
  }

  MediaErrorState error_state;
  // TODO(1288839): Set media_type, options, callbacks, surface appropriately
  MediaConstraints audio = (data.kind == "audio")
                               ? media_constraints_impl::Create()
                               : MediaConstraints();
  MediaConstraints video = (data.kind == "video")
                               ? media_constraints_impl::Create()
                               : MediaConstraints();
  UserMediaRequest* const request = MakeGarbageCollected<UserMediaRequest>(
      window, user_media_client, UserMediaRequestType::kDisplayMedia, audio,
      video, /*should_prefer_current_tab=*/false,
      /*auto_select_all_screens=*/false,
      MakeGarbageCollected<GetOpenDeviceRequestCallbacks>(),
      IdentifiableSurface());
  if (!request) {
      return nullptr;
  }

  // TODO(1288839): Create a TransferredMediaStreamTrack implementing interfaces
  // supporting BrowserCaptureMediaStreamTrack or FocusableMediaStreamTrack
  // operations when needed (or support these behaviors in some other way).
  TransferredMediaStreamTrack* transferred_media_stream_track =
      MakeGarbageCollected<TransferredMediaStreamTrack>(
          ExecutionContext::From(script_state), data);

  request->SetTransferData(data.session_id, data.transfer_id,
                           transferred_media_stream_track);
  request->Start();

  // TODO(1288839): get rid of TransferredMediaStreamTrack, since it's just a
  // container for the impl track
  auto* track = transferred_media_stream_track->track();
  // TODO(1288839): What happens if GetOpenDevice fails?
  DCHECK(track);
  if (track->GetWrapperTypeInfo() != data.track_impl_subtype) {
    NOTREACHED() << "transferred track should be "
                 << data.track_impl_subtype->interface_name
                 << " but instead it is "
                 << track->GetWrapperTypeInfo()->interface_name;
    return nullptr;
  }
  return track;
}

<<<<<<< HEAD
void MediaStreamTrack::applyConstraintsImageCapture(
    ScriptPromiseResolver* resolver,
    const MediaTrackConstraints* constraints) {
  // |constraints| empty means "remove/clear all current constraints".
  if (!constraints->hasAdvanced() || constraints->advanced().IsEmpty()) {
    image_capture_->ClearMediaTrackConstraints();
    resolver->Resolve();
  } else {
    image_capture_->SetMediaTrackConstraints(resolver, constraints->advanced());
  }
}

bool MediaStreamTrack::Ended() const {
  return (execution_context_ && execution_context_->IsContextDestroyed()) ||
         (ready_state_ == MediaStreamSource::kReadyStateEnded);
}

void MediaStreamTrack::SourceChangedState() {
  if (Ended())
    return;

  // Note that both 'live' and 'muted' correspond to a 'live' ready state in the
  // web API, hence the following logic around |feature_handle_for_scheduler_|.

  setReadyState(component_->Source()->GetReadyState());
  switch (ready_state_) {
    case MediaStreamSource::kReadyStateLive:
      component_->SetMuted(false);
      DispatchEvent(*Event::Create(event_type_names::kUnmute), "MediaStreamTrack::SourceChangedState #1");
      EnsureFeatureHandleForScheduler();
      break;
    case MediaStreamSource::kReadyStateMuted:
      component_->SetMuted(true);
      DispatchEvent(*Event::Create(event_type_names::kMute), "MediaStreamTrack::SourceChangedState #2");
      EnsureFeatureHandleForScheduler();
      break;
    case MediaStreamSource::kReadyStateEnded:
      DispatchEvent(*Event::Create(event_type_names::kEnded), "MediaStreamTrack::SourceChangedState #3");
      PropagateTrackEnded();
      feature_handle_for_scheduler_.reset();
      break;
  }
  SendLogMessage(
      base::StringPrintf("SourceChangedState([id=%s] {readyState=%s})",
                         id().Utf8().c_str(), readyState().Utf8().c_str()));
}

void MediaStreamTrack::PropagateTrackEnded() {
  CHECK(!is_iterating_registered_media_streams_);
  is_iterating_registered_media_streams_ = true;
  for (HeapHashSet<Member<MediaStream>>::iterator iter =
           registered_media_streams_.begin();
       iter != registered_media_streams_.end(); ++iter)
    (*iter)->TrackEnded();
  is_iterating_registered_media_streams_ = false;
}

bool MediaStreamTrack::HasPendingActivity() const {
  // If 'ended' listeners exist and the object hasn't yet reached
  // that state, keep the object alive.
  //
  // An otherwise unreachable MediaStreamTrack object in an non-ended
  // state will otherwise indirectly be transitioned to the 'ended' state
  // while finalizing m_component. Which dispatches an 'ended' event,
  // referring to this object as the target. If this object is then GCed
  // at the same time, v8 objects will retain (wrapper) references to
  // this dead MediaStreamTrack object. Bad.
  //
  // Hence insisting on keeping this object alive until the 'ended'
  // state has been reached & handled.
  return !Ended() && HasEventListeners(event_type_names::kEnded);
}

std::unique_ptr<AudioSourceProvider> MediaStreamTrack::CreateWebAudioSource(
    int context_sample_rate) {
  return std::make_unique<MediaStreamWebAudioSource>(
      CreateWebAudioSourceFromMediaStreamTrack(Component(),
                                               context_sample_rate));
}

void MediaStreamTrack::RegisterMediaStream(MediaStream* media_stream) {
  CHECK(!is_iterating_registered_media_streams_);
  CHECK(!registered_media_streams_.Contains(media_stream));
  registered_media_streams_.insert(media_stream);
}

void MediaStreamTrack::UnregisterMediaStream(MediaStream* media_stream) {
  CHECK(!is_iterating_registered_media_streams_);
  HeapHashSet<Member<MediaStream>>::iterator iter =
      registered_media_streams_.find(media_stream);
  CHECK(iter != registered_media_streams_.end());
  registered_media_streams_.erase(iter);
}

const AtomicString& MediaStreamTrack::InterfaceName() const {
  return event_target_names::kMediaStreamTrack;
}

ExecutionContext* MediaStreamTrack::GetExecutionContext() const {
  return execution_context_.Get();
}

void MediaStreamTrack::Trace(Visitor* visitor) const {
  visitor->Trace(registered_media_streams_);
  visitor->Trace(component_);
  visitor->Trace(image_capture_);
  visitor->Trace(execution_context_);
  visitor->Trace(observers_);
  EventTargetWithInlineData::Trace(visitor);
}

void MediaStreamTrack::EnsureFeatureHandleForScheduler() {
  if (feature_handle_for_scheduler_)
    return;
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  // Ideally we'd use To<LocalDOMWindow>, but in unittests the ExecutionContext
  // may not be a LocalDOMWindow.
  if (!window)
    return;
  // This can happen for detached frames.
  if (!window->GetFrame())
    return;
  feature_handle_for_scheduler_ =
      window->GetFrame()->GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebRTC,
          SchedulingPolicy::DisableAggressiveThrottling());
}

void MediaStreamTrack::AddObserver(MediaStreamTrack::Observer* observer) {
  observers_.insert(observer);
||||||| 80c960997e61f
void MediaStreamTrack::applyConstraintsImageCapture(
    ScriptPromiseResolver* resolver,
    const MediaTrackConstraints* constraints) {
  // |constraints| empty means "remove/clear all current constraints".
  if (!constraints->hasAdvanced() || constraints->advanced().IsEmpty()) {
    image_capture_->ClearMediaTrackConstraints();
    resolver->Resolve();
  } else {
    image_capture_->SetMediaTrackConstraints(resolver, constraints->advanced());
  }
}

bool MediaStreamTrack::Ended() const {
  return (execution_context_ && execution_context_->IsContextDestroyed()) ||
         (ready_state_ == MediaStreamSource::kReadyStateEnded);
}

void MediaStreamTrack::SourceChangedState() {
  if (Ended())
    return;

  // Note that both 'live' and 'muted' correspond to a 'live' ready state in the
  // web API, hence the following logic around |feature_handle_for_scheduler_|.

  setReadyState(component_->Source()->GetReadyState());
  switch (ready_state_) {
    case MediaStreamSource::kReadyStateLive:
      component_->SetMuted(false);
      DispatchEvent(*Event::Create(event_type_names::kUnmute));
      EnsureFeatureHandleForScheduler();
      break;
    case MediaStreamSource::kReadyStateMuted:
      component_->SetMuted(true);
      DispatchEvent(*Event::Create(event_type_names::kMute));
      EnsureFeatureHandleForScheduler();
      break;
    case MediaStreamSource::kReadyStateEnded:
      DispatchEvent(*Event::Create(event_type_names::kEnded));
      PropagateTrackEnded();
      feature_handle_for_scheduler_.reset();
      break;
  }
  SendLogMessage(
      base::StringPrintf("SourceChangedState([id=%s] {readyState=%s})",
                         id().Utf8().c_str(), readyState().Utf8().c_str()));
}

void MediaStreamTrack::PropagateTrackEnded() {
  CHECK(!is_iterating_registered_media_streams_);
  is_iterating_registered_media_streams_ = true;
  for (HeapHashSet<Member<MediaStream>>::iterator iter =
           registered_media_streams_.begin();
       iter != registered_media_streams_.end(); ++iter)
    (*iter)->TrackEnded();
  is_iterating_registered_media_streams_ = false;
}

bool MediaStreamTrack::HasPendingActivity() const {
  // If 'ended' listeners exist and the object hasn't yet reached
  // that state, keep the object alive.
  //
  // An otherwise unreachable MediaStreamTrack object in an non-ended
  // state will otherwise indirectly be transitioned to the 'ended' state
  // while finalizing m_component. Which dispatches an 'ended' event,
  // referring to this object as the target. If this object is then GCed
  // at the same time, v8 objects will retain (wrapper) references to
  // this dead MediaStreamTrack object. Bad.
  //
  // Hence insisting on keeping this object alive until the 'ended'
  // state has been reached & handled.
  return !Ended() && HasEventListeners(event_type_names::kEnded);
}

std::unique_ptr<AudioSourceProvider> MediaStreamTrack::CreateWebAudioSource(
    int context_sample_rate) {
  return std::make_unique<MediaStreamWebAudioSource>(
      CreateWebAudioSourceFromMediaStreamTrack(Component(),
                                               context_sample_rate));
}

void MediaStreamTrack::RegisterMediaStream(MediaStream* media_stream) {
  CHECK(!is_iterating_registered_media_streams_);
  CHECK(!registered_media_streams_.Contains(media_stream));
  registered_media_streams_.insert(media_stream);
}

void MediaStreamTrack::UnregisterMediaStream(MediaStream* media_stream) {
  CHECK(!is_iterating_registered_media_streams_);
  HeapHashSet<Member<MediaStream>>::iterator iter =
      registered_media_streams_.find(media_stream);
  CHECK(iter != registered_media_streams_.end());
  registered_media_streams_.erase(iter);
}

const AtomicString& MediaStreamTrack::InterfaceName() const {
  return event_target_names::kMediaStreamTrack;
}

ExecutionContext* MediaStreamTrack::GetExecutionContext() const {
  return execution_context_.Get();
}

void MediaStreamTrack::Trace(Visitor* visitor) const {
  visitor->Trace(registered_media_streams_);
  visitor->Trace(component_);
  visitor->Trace(image_capture_);
  visitor->Trace(execution_context_);
  visitor->Trace(observers_);
  EventTargetWithInlineData::Trace(visitor);
}

void MediaStreamTrack::EnsureFeatureHandleForScheduler() {
  if (feature_handle_for_scheduler_)
    return;
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  // Ideally we'd use To<LocalDOMWindow>, but in unittests the ExecutionContext
  // may not be a LocalDOMWindow.
  if (!window)
    return;
  // This can happen for detached frames.
  if (!window->GetFrame())
    return;
  feature_handle_for_scheduler_ =
      window->GetFrame()->GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebRTC,
          SchedulingPolicy::DisableAggressiveThrottling());
}

void MediaStreamTrack::AddObserver(MediaStreamTrack::Observer* observer) {
  observers_.insert(observer);
=======
// static
MediaStreamTrack::FromTransferredStateImplForTesting&
MediaStreamTrack::GetFromTransferredStateImplForTesting() {
  static base::NoDestructor<
      MediaStreamTrack::FromTransferredStateImplForTesting>
      impl;
  return *impl;
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
}

}  // namespace blink
