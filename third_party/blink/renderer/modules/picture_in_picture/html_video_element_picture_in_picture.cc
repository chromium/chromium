// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/html_video_element_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using Status = PictureInPictureController::Status;

namespace {

const char kDetachedError[] =
    "The element is no longer associated with a document.";
const char kMetadataNotLoadedError[] =
    "Metadata for the video element are not loaded yet.";
const char kVideoTrackNotAvailableError[] =
    "The video element has no video track.";
const char kFeaturePolicyBlocked[] =
    "Access to the feature \"picture-in-picture\" is disallowed by permissions "
    "policy.";
const char kNotAvailable[] = "Picture-in-Picture is not available.";
const char kUserGestureRequired[] =
    "Must be handling a user gesture if there isn't already an element in "
    "Picture-in-Picture.";
const char kDisablePictureInPicturePresent[] =
    "\"disablePictureInPicture\" attribute is present.";
const char kAutoPipAndroid[] = "The video is currently in auto-pip mode.";
const char kDocumentPip[] = "The video is currently in document pip mode.";

}  // namespace

// static
ScriptPromise<PictureInPictureWindow>
HTMLVideoElementPictureInPicture::requestPictureInPicture(
    ScriptState* script_state,
    HTMLVideoElement& element,
    ExceptionState& exception_state) {
  CheckIfPictureInPictureIsAllowed(element, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PictureInPictureWindow>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  PictureInPictureController::From(element.GetDocument())
      .EnterPictureInPicture(&element, resolver);

  return promise;
}

// static
bool HTMLVideoElementPictureInPicture::FastHasAttribute(
    const HTMLVideoElement& element,
    const QualifiedName& name) {
  DCHECK(name == html_names::kDisablepictureinpictureAttr);
  return element.FastHasAttribute(name);
}

// static
void HTMLVideoElementPictureInPicture::SetBooleanAttribute(
    HTMLVideoElement& element,
    const QualifiedName& name,
    bool value) {
  DCHECK(name == html_names::kDisablepictureinpictureAttr);
  element.SetBooleanAttribute(name, value);

  Document& document = element.GetDocument();
  TreeScope& scope = element.GetTreeScope();
  PictureInPictureController& controller =
      PictureInPictureController::From(document);

  if (name == html_names::kDisablepictureinpictureAttr && value &&
      controller.PictureInPictureElement(scope) == &element) {
    controller.ExitPictureInPicture(&element, nullptr);
  }
}

// static
void HTMLVideoElementPictureInPicture::CheckIfPictureInPictureIsAllowed(
    HTMLVideoElement& element,
    ExceptionState& exception_state) {
  Document& document = element.GetDocument();
  PictureInPictureController& controller =
      PictureInPictureController::From(document);

  switch (controller.IsElementAllowed(element, /*report_failure=*/true)) {
    case Status::kFrameDetached:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kDetachedError);
      return;
    case Status::kMetadataNotLoaded:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kMetadataNotLoadedError);
      return;
    case Status::kVideoTrackNotAvailable:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kVideoTrackNotAvailableError);
      return;
    case Status::kDisabledByPermissionsPolicy:
      exception_state.ThrowSecurityError(kFeaturePolicyBlocked);
      return;
    case Status::kDisabledByAttribute:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kDisablePictureInPicturePresent);
      return;
    case Status::kDisabledBySystem:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        kNotAvailable);
      return;
    case Status::kAutoPipAndroid:
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        kAutoPipAndroid);
      return;
    case Status::kDocumentPip:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        kDocumentPip);
      return;
    case Status::kEnabled:
      break;
  }

  // Frame is not null, otherwise `IsElementAllowed()` would have return
  // `kFrameDetached`.
  LocalFrame* frame = document.GetFrame();
  DCHECK(frame);
  if (!controller.PictureInPictureElement() &&
      !LocalFrame::ConsumeTransientUserActivation(frame)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kUserGestureRequired);
  }
}

}  // namespace blink
