// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_union_boolean_mediatrackconstraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_domexception_overconstrainederror.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"

namespace blink {

namespace {
bool IsConstraintEnabled(
    const V8UnionBooleanOrMediaTrackConstraints* constraint) {
  if (!constraint) {
    return false;
  }
  if (constraint->IsBoolean()) {
    return constraint->GetAsBoolean();
  }
  return constraint->IsMediaTrackConstraints();
}
}  // namespace

UserMediaRequestProviderCallbacks::UserMediaRequestProviderCallbacks(
    HTMLUserMediaElement* element)
    : element_(element) {}

void UserMediaRequestProviderCallbacks::OnSuccess(
    const MediaStreamVector& streams,
    CaptureController* capture_controller) {
  if (streams.empty()) {
    return;
  }
  MediaStream* stream = streams[0];
  if (element_) {
    HTMLUserMediaElementMediaStream::From(*element_).SetMediaStream(stream);
    element_->DispatchEvent(*Event::Create(event_type_names::kStream));
  }
}

void UserMediaRequestProviderCallbacks::OnError(
    ScriptWrappable* callback_this_value,
    const V8MediaStreamError* error,
    CaptureController* capture_controller,
    UserMediaRequestResult result) {
  if (element_ && element_->GetExecutionContext()) {
    ScriptState* script_state =
        ToScriptStateForMainWorld(element_->GetDocument().GetFrame());
    if (script_state) {
      ScriptState::Scope scope(script_state);
      HTMLUserMediaElementMediaStream::From(*element_).SetError(
          WorldSafeV8Reference<v8::Value>(script_state->GetIsolate(),
                                          error->ToV8(script_state)));
    }
    element_->DispatchEvent(*Event::Create(event_type_names::kStream));
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
    HTMLUserMediaElement* element,
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

  MediaStream* existing_stream =
      HTMLUserMediaElementMediaStream::stream(*element);
  if (existing_stream && existing_stream->active()) {
    return;
  }

  // Constraints that are set on the HTMLUserMediaElement.
  const MediaStreamConstraints* constraints =
      UserMediaElementConstraints::From(*element).Constraints();

  // Constraints that will be used for the UserMediaRequest.
  MediaStreamConstraints* request_constraints = nullptr;

  if (permission_descriptors.size() == 2) {
    // Camera and Microphone element.
    if (!IsConstraintEnabled(constraints->audio()) &&
        !IsConstraintEnabled(constraints->video())) {
      HTMLUserMediaElementMediaStream::From(*element).SetError(
          WorldSafeV8Reference<v8::Value>(
              window->GetIsolate(),
              V8ThrowException::CreateTypeError(window->GetIsolate(),
                                                "No constraints set")));
      element->DispatchEvent(*Event::Create(event_type_names::kStream));
      return;
    }
    request_constraints = MediaStreamConstraints::Create();
    request_constraints->setAudio(constraints->audio());
    request_constraints->setVideo(constraints->video());
  } else if (permission_descriptors[0]->name ==
             mojom::blink::PermissionName::AUDIO_CAPTURE) {
    // Audio only element.
    if (!IsConstraintEnabled(constraints->audio())) {
      HTMLUserMediaElementMediaStream::From(*element).SetError(
          WorldSafeV8Reference<v8::Value>(
              window->GetIsolate(),
              V8ThrowException::CreateTypeError(window->GetIsolate(),
                                                "No audio constraints set")));
      element->DispatchEvent(*Event::Create(event_type_names::kStream));
      return;
    }
    request_constraints = MediaStreamConstraints::Create();
    request_constraints->setAudio(constraints->audio());
  } else {
    // Video only element.
    CHECK_EQ(permission_descriptors[0]->name,
             mojom::blink::PermissionName::VIDEO_CAPTURE);
    if (!IsConstraintEnabled(constraints->video())) {
      HTMLUserMediaElementMediaStream::From(*element).SetError(
          WorldSafeV8Reference<v8::Value>(
              window->GetIsolate(),
              V8ThrowException::CreateTypeError(window->GetIsolate(),
                                                "No video constraints set")));
      element->DispatchEvent(*Event::Create(event_type_names::kStream));
      return;
    }
    request_constraints = MediaStreamConstraints::Create();
    request_constraints->setVideo(constraints->video());
  }

  ExceptionState exception_state(window->GetIsolate());
  UserMediaRequest* request = UserMediaRequest::Create(
      window, client, UserMediaRequestType::kUserMedia, request_constraints,
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element),
      exception_state);

  if (exception_state.HadException()) {
    HTMLUserMediaElementMediaStream::From(*element).SetError(
        WorldSafeV8Reference<v8::Value>(
            window->GetIsolate(),
            V8ThrowException::CreateTypeError(
                window->GetIsolate(), "Stream creation failed")));
    element->DispatchEvent(*Event::Create(event_type_names::kStream));
    return;
  }

  if (request) {
    request->Start();
  }
}

}  // namespace blink
