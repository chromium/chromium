// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"

#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

PictureInPictureController::PictureInPictureController(Document& document)
    : Supplement<Document>(document) {}

// static
const char PictureInPictureController::kSupplementName[] =
    "PictureInPictureController";

// static
PictureInPictureController& PictureInPictureController::From(
    Document& document) {
  PictureInPictureController* controller =
      Supplement<Document>::From<PictureInPictureController>(document);
  if (!controller) {
    controller =
        CoreInitializer::GetInstance().CreatePictureInPictureController(
            document);
    ProvideTo(document, controller);
  }
  return *controller;
}

// static
bool PictureInPictureController::IsElementInPictureInPicture(
    const Element* element) {
  DCHECK(element);
  Document& document = element->GetDocument();
  PictureInPictureController* controller =
      Supplement<Document>::From<PictureInPictureController>(document);
  return controller && controller->IsPictureInPictureElement(element);
}

void PictureInPictureController::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
