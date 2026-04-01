// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/user_media_request_provider_impl.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/mediastream/html_user_media_element_media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_client.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"

namespace blink {

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
  // TODO: b/494194590: Handle errors
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
  LocalDOMWindow* window = element->GetDocument().domWindow();
  if (!window) {
    return;
  }

  UserMediaClient* client = UserMediaClient::From(window);
  if (!client) {
    return;
  }

  MediaStream* existing_stream =
      HTMLUserMediaElementMediaStream::stream(*element);
  if (existing_stream && existing_stream->active()) {
    return;
  }

  // TODO: b/494481412: Parse constraints from HTMLUserMediaElement
  MediaConstraints audio_constraints;
  MediaConstraints video_constraints;

  for (const auto& descriptor : permission_descriptors) {
    if (descriptor->name == mojom::blink::PermissionName::AUDIO_CAPTURE) {
      audio_constraints.Initialize();
    } else if (descriptor->name ==
               mojom::blink::PermissionName::VIDEO_CAPTURE) {
      video_constraints.Initialize();
    }
  }

  UserMediaRequest* request = MakeGarbageCollected<UserMediaRequest>(
      window, client, UserMediaRequestType::kUserMedia, audio_constraints,
      video_constraints,
      /*should_prefer_current_tab=*/false,
      /*capture_controller=*/nullptr,
      MakeGarbageCollected<UserMediaRequestProviderCallbacks>(element));
  request->Start();
}

}  // namespace blink
