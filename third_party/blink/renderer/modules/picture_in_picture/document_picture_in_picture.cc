// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/document_picture_in_picture.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"

namespace blink {

namespace {

const char kNoPictureInPictureElement[] =
    "There is no Picture-in-Picture element in this document.";

}  // namespace

// static
bool DocumentPictureInPicture::pictureInPictureEnabled(Document& document) {
  return PictureInPictureControllerImpl::From(document)
      .PictureInPictureEnabled();
}

// static
ScriptPromise DocumentPictureInPicture::exitPictureInPicture(
    ScriptState* script_state,
    Document& document) {
  PictureInPictureControllerImpl& controller =
      PictureInPictureControllerImpl::From(document);
  Element* picture_in_picture_element =
      controller.PictureInPictureElement(document);

  if (!picture_in_picture_element) {
    return ScriptPromise::RejectWithDOMException(
        script_state, DOMException::Create(DOMExceptionCode::kInvalidStateError,
                                           kNoPictureInPictureElement));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  DCHECK(IsHTMLVideoElement(picture_in_picture_element));
  document.GetTaskRunner(TaskType::kMediaElementEvent)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(
              &PictureInPictureControllerImpl::ExitPictureInPicture,
              WrapPersistent(&controller),
              WrapPersistent(ToHTMLVideoElement(picture_in_picture_element)),
              WrapPersistent(resolver)));
  return promise;
}

// static
Element* DocumentPictureInPicture::pictureInPictureElement(TreeScope& scope) {
  return PictureInPictureControllerImpl::From(scope.GetDocument())
      .PictureInPictureElement(scope);
}

}  // namespace blink
