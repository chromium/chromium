// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_html_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_media_capture_element_base.h"
#include "third_party/blink/renderer/core/html/html_media_track_element_base.h"
#include "third_party/blink/renderer/modules/mediastream/html_media_track_element_media_track.h"
#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/overconstrained_error.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"

namespace blink {

UserMediaRequestProviderCallbacks::UserMediaRequestProviderCallbacks(
    HTMLMediaCaptureElementBase* element)
    : element_(element) {}

void UserMediaRequestProviderCallbacks::OnSuccess(
    const MediaStreamVector& streams,
    CaptureController* capture_controller) {
  if (!element_) {
    return;
  }

  element_->ResetMediaStreamRequestTime();
  if (streams.empty()) {
    return;
  }
  MediaStream* stream = streams[0];
  if (auto* track_element =
          DynamicTo<HTMLMediaTrackElementBase>(element_.Get())) {
    MediaStreamTrack* track = nullptr;
    if (track_element->IsHTMLCameraElement()) {
      MediaStreamTrackVector video_tracks = stream->getVideoTracks();
      track = video_tracks.empty() ? nullptr : video_tracks[0];
    } else if (track_element->IsHTMLMicrophoneElement()) {
      MediaStreamTrackVector audio_tracks = stream->getAudioTracks();
      track = audio_tracks.empty() ? nullptr : audio_tracks[0];
    }
    if (track) {
      HTMLMediaTrackElementMediaTrack::From(*track_element).SetMediaTrack(track);
      track_element->EnqueueEvent(*Event::Create(event_type_names::kTrack),
                                  TaskType::kDOMManipulation);
    } else {
      element_->SetError(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, "No matching media track found"));
      element_->EnqueueEvent(*Event::Create(event_type_names::kError),
                             TaskType::kDOMManipulation);
    }
  } else if (auto* user_media =
                 DynamicTo<HTMLUserMediaElement>(element_.Get())) {
    HTMLUserMediaElementMediaStream::From(*user_media).SetMediaStream(stream);
    element_->EnqueueEvent(*Event::Create(event_type_names::kStream),
                           TaskType::kDOMManipulation);
  }
}

void UserMediaRequestProviderCallbacks::OnError(
    ScriptWrappable* callback_this_value,
    const V8MediaStreamError* error,
    CaptureController* capture_controller,
    UserMediaRequestResult result) {
  if (element_ && element_->GetExecutionContext()) {
    element_->ResetMediaStreamRequestTime();
    DOMException* dom_exception = nullptr;
    if (error) {
      if (error->IsDOMException()) {
        dom_exception = error->GetAsDOMException();
      } else if (error->IsOverconstrainedError()) {
        dom_exception = error->GetAsOverconstrainedError();
      }
    }
    element_->SetError(dom_exception);
    if (result == UserMediaRequestResult::kNotAllowedByUserError) {
      element_->EnqueueEvent(*Event::Create(event_type_names::kCancel),
                             TaskType::kDOMManipulation);
    } else {
      base::UmaHistogramBoolean(
          "Blink.CapabilityElement.UserMedia.GumApi.OverconstrainedError",
          error->IsOverconstrainedError());
      element_->EnqueueEvent(*Event::Create(event_type_names::kError),
                             TaskType::kDOMManipulation);
    }
  }
}

void UserMediaRequestProviderCallbacks::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  UserMediaRequest::Callbacks::Trace(visitor);
}

// static
void UserMediaRequestProviderImpl::ProvideTo(LocalDOMWindow& window) {
  if (!UserMediaRequestProvider::From(window)) {
    UserMediaRequestProvider::ProvideTo(
        window, MakeGarbageCollected<UserMediaRequestProviderImpl>(window));
  }
}

UserMediaRequestProviderImpl::UserMediaRequestProviderImpl(
    LocalDOMWindow& window)
    : UserMediaRequestProvider(window) {}

void UserMediaRequestProviderImpl::StartRequest(
    HTMLMediaCaptureElementBase* element,
    const Vector<mojom::blink::PermissionDescriptorPtr>&
        permission_descriptors) {
  if (permission_descriptors.empty()) {
    return;
  }

  LocalDOMWindow* window = element->GetDocument().domWindow();
  if (!window) {
    return;
  }

  ScriptState* script_state = ToScriptStateForMainWorld(window->GetFrame());
  if (!script_state) {
    return;
  }
  ScriptState::Scope scope(script_state);

  UserMediaClient* client = UserMediaClient::From(window);
  if (!client) {
    return;
  }

  if (auto* user_media = DynamicTo<HTMLUserMediaElement>(element)) {
    MediaStream* existing_stream =
        HTMLUserMediaElementMediaStream::stream(*user_media);
    if (existing_stream && existing_stream->active()) {
      return;
    }
  } else if (auto* track_element =
                 DynamicTo<HTMLMediaTrackElementBase>(element)) {
    MediaStreamTrack* existing_track =
        HTMLMediaTrackElementMediaTrack::From(*track_element).MediaTrack();
    if (existing_track && !existing_track->Ended()) {
      return;
    }
  }

  // Constraints that are set on the media capture element.
  const HTMLMediaStreamConstraints* constraints =
      UserMediaElementConstraints::From(*element).Constraints();

  if (!constraints) {
    HTMLMediaStreamConstraints* default_constraints =
        HTMLMediaStreamConstraints::Create();
    default_constraints->setVideo(MediaTrackConstraints::Create());
    default_constraints->setAudio(MediaTrackConstraints::Create());
    constraints = default_constraints;
  }

  // Constraints that will be used for the UserMediaRequest.
  MediaStreamConstraints* request_constraints = nullptr;

  if (permission_descriptors.size() == 2) {
    // Camera and Microphone element.
    if (!constraints->hasAudio() && !constraints->hasVideo()) {
      element->SetError(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "No constraints set"));
      element->EnqueueEvent(*Event::Create(event_type_names::kError),
                            TaskType::kDOMManipulation);
      return;
    }
    request_constraints = MediaStreamConstraints::Create();
    if (constraints->hasAudio()) {
      request_constraints->setAudio(
          MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(
              static_cast<const MediaTrackConstraints*>(constraints->audio())));
    }
    if (constraints->hasVideo()) {
      request_constraints->setVideo(
          MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(
              static_cast<const MediaTrackConstraints*>(constraints->video())));
    }
  } else if (permission_descriptors[0]->name ==
             mojom::blink::PermissionName::AUDIO_CAPTURE) {
    // Audio only element.
    if (!constraints->hasAudio()) {
      element->SetError(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "No audio constraints set"));
      element->EnqueueEvent(*Event::Create(event_type_names::kError),
                            TaskType::kDOMManipulation);
      return;
    }
    request_constraints = MediaStreamConstraints::Create();
    request_constraints->setAudio(
        MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(
            static_cast<const MediaTrackConstraints*>(constraints->audio())));
  } else {
    // Video only element.
    CHECK_EQ(permission_descriptors[0]->name,
             mojom::blink::PermissionName::VIDEO_CAPTURE);
    if (!constraints->hasVideo()) {
      element->SetError(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotSupportedError, "No video constraints set"));
      element->EnqueueEvent(*Event::Create(event_type_names::kError),
                            TaskType::kDOMManipulation);
      return;
    }
    request_constraints = MediaStreamConstraints::Create();
    request_constraints->setVideo(
        MakeGarbageCollected<V8UnionBooleanOrMediaTrackConstraints>(
            static_cast<const MediaTrackConstraints*>(constraints->video())));
  }

  ExceptionState exception_state(window->GetIsolate());
  UserMediaRequest* request = UserMediaRequest::Create(
      window, client, UserMediaRequestType::kUserMedia, request_constraints,
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element),
      exception_state);

  if (exception_state.HadException()) {
    element->SetError(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kOperationError, "Stream creation failed"));
    element->EnqueueEvent(*Event::Create(event_type_names::kError),
                          TaskType::kDOMManipulation);
    return;
  }

  if (request) {
    request->Start();
  }
}

}  // namespace blink
