// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/html_element_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_options.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

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
    "Must be handling a user gesture if there isn't already an element in "
    "Picture-in-Picture.";
const char kDisablePictureInPicturePresent[] =
    "\"disablePictureInPicture\" attribute is present.";
const char kInvalidSize[] =
    "The width and height attributes must be greater "
    "than zero.";

}  // namespace

// static
ScriptPromise HTMLElementPictureInPicture::requestPictureInPicture(
    ScriptState* script_state,
    HTMLElement& element,
    PictureInPictureOptions* options,
    ExceptionState& exception_state) {
  CheckIfPictureInPictureIsAllowed(element, options, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  auto promise = resolver->Promise();

  PictureInPictureControllerImpl::From(element.GetDocument())
      .EnterPictureInPicture(&element, options, resolver);

  return promise;
}

// static
void HTMLElementPictureInPicture::CheckIfPictureInPictureIsAllowed(
    HTMLElement& element,
    PictureInPictureOptions* options,
    ExceptionState& exception_state) {
  Document& document = element.GetDocument();
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);

  switch (controller.VerifyElementAndOptions(element, options)) {
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
    case Status::kDisabledByFeaturePolicy:
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
    case Status::kInvalidWidthOrHeightOption:
      exception_state.ThrowTypeError(kInvalidSize);
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
