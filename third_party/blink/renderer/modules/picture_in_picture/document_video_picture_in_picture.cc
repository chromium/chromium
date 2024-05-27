// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/document_video_picture_in_picture.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

const char kNoPictureInPictureElement[] =
    "There is no Picture-in-Picture element in this document.";

}  // namespace

// static
bool DocumentVideoPictureInPicture::pictureInPictureEnabled(
    Document& document) {
  return PictureInPictureController::From(document).PictureInPictureEnabled();
}

// static
ScriptPromise<IDLUndefined> DocumentVideoPictureInPicture::exitPictureInPicture(
    ScriptState* script_state,
    Document& document,
    ExceptionState& exception_state) {
  PictureInPictureController& controller =
      PictureInPictureController::From(document);
  Element* picture_in_picture_element = controller.PictureInPictureElement();

  if (!picture_in_picture_element) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNoPictureInPictureElement);
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  DCHECK(IsA<HTMLVideoElement>(picture_in_picture_element));
  controller.ExitPictureInPicture(
      To<HTMLVideoElement>(picture_in_picture_element), resolver);
  return promise;
}

}  // namespace blink
