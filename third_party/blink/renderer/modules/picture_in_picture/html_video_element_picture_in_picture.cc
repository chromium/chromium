// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/html_video_element_picture_in_picture.h"

#include "third_party/blink/public/common/picture_in_picture/picture_in_picture_control_info.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_control.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"

namespace blink {

using Status = PictureInPictureControllerImpl::Status;

namespace {

const char kDetachedError[] =
    "The element is no longer associated with a document.";
const char kMetadataNotLoadedError[] =
    "Metadata for the video element are not loaded yet.";
const char kVideoTrackNotAvailableError[] =
    "The video element has no video track.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"picture-in-picture\" is disallowed by feature "
    "policy.";
const char kNotAvailable[] = "Picture-in-Picture is not available.";
const char kUserGestureRequired[] =
    "Must be handling a user gesture to request picture in picture.";
const char kDisablePictureInPicturePresent[] =
    "\"disablePictureInPicture\" attribute is present.";
}  // namespace

// static
ScriptPromise HTMLVideoElementPictureInPicture::requestPictureInPicture(
    ScriptState* script_state,
    HTMLVideoElement& element) {
  Document& document = element.GetDocument();
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);

  switch (controller.IsElementAllowed(element)) {
    case Status::kFrameDetached:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kDetachedError));
    case Status::kMetadataNotLoaded:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kMetadataNotLoadedError));
    case Status::kVideoTrackNotAvailable:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kVideoTrackNotAvailableError));
    case Status::kDisabledByFeaturePolicy:
      return ScriptPromise::RejectWithDOMException(
          script_state, DOMException::Create(DOMExceptionCode::kSecurityError,
                                             kFeaturePolicyBlocked));
    case Status::kDisabledByAttribute:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kInvalidStateError,
                               kDisablePictureInPicturePresent));
    case Status::kDisabledBySystem:
      return ScriptPromise::RejectWithDOMException(
          script_state,
          DOMException::Create(DOMExceptionCode::kNotSupportedError,
                               kNotAvailable));
    case Status::kEnabled:
      break;
  }

  // Frame is not null, otherwise `IsElementAllowed()` would have return
  // `kFrameDetached`.
  LocalFrame* frame = element.GetFrame();
  DCHECK(frame);
  if (!LocalFrame::ConsumeTransientUserActivation(frame)) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kNotAllowedError,
                                           kUserGestureRequired));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  document.GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(&PictureInPictureControllerImpl::EnterPictureInPicture,
                    WrapPersistent(&controller), WrapPersistent(&element),
                    WrapPersistent(resolver)));

  return promise;
}

void HTMLVideoElementPictureInPicture::setPictureInPictureControls(
    HTMLVideoElement& element,
    const HeapVector<PictureInPictureControl>& controls) {
  Document& document = element.GetDocument();

  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);

  controller.SetPictureInPictureCustomControls(
      &element, ToPictureInPictureControlInfoVector(controls));
}

// static
bool HTMLVideoElementPictureInPicture::FastHasAttribute(
    const QualifiedName& name,
    const HTMLVideoElement& element) {
  DCHECK(name == HTMLNames::disablepictureinpictureAttr);
  return element.FastHasAttribute(name);
}

// static
void HTMLVideoElementPictureInPicture::SetBooleanAttribute(
    const QualifiedName& name,
    HTMLVideoElement& element,
    bool value) {
  DCHECK(name == HTMLNames::disablepictureinpictureAttr);
  element.SetBooleanAttribute(name, value);

  if (!value)
    return;

  Document& document = element.GetDocument();
  TreeScope& scope = element.GetTreeScope();
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);
  if (controller.PictureInPictureElement(scope) == &element) {
    controller.ExitPictureInPicture(&element, nullptr);
  }
}

// static
std::vector<PictureInPictureControlInfo>
HTMLVideoElementPictureInPicture::ToPictureInPictureControlInfoVector(
    const HeapVector<PictureInPictureControl>& controls) {
  std::vector<PictureInPictureControlInfo> converted_controls;
  for (const PictureInPictureControl& control : controls) {
    PictureInPictureControlInfo current_converted_control;
    HeapVector<MediaImage> current_icons = control.icons();

    // Only two icons are supported, so cap the loop at running that many times
    // to avoid potential problems.
    for (wtf_size_t j = 0; j < current_icons.size() && j < 2; ++j) {
      PictureInPictureControlInfo::Icon current_icon;
      current_icon.src = KURL(WebString(current_icons[j].src()));

      WebVector<WebSize> sizes = WebIconSizesParser::ParseIconSizes(
          WebString(current_icons[j].sizes()));
      std::vector<gfx::Size> converted_sizes;
      for (size_t i = 0; i < sizes.size(); ++i)
        converted_sizes.push_back(static_cast<gfx::Size>(sizes[i]));

      current_icon.sizes = converted_sizes;
      current_icon.type = WebString(current_icons[j].type()).Utf8();
      current_converted_control.icons.push_back(current_icon);
    }

    current_converted_control.id = WebString(control.id()).Utf8();
    current_converted_control.label = WebString(control.label()).Utf8();
    converted_controls.push_back(current_converted_control);
  }
  return converted_controls;
}

}  // namespace blink
