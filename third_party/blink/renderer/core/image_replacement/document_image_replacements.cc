// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/image_replacement/document_image_replacements.h"

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/image_replacement/image_replacement.h"

namespace blink {

// static
const char DocumentImageReplacements::kSupplementName[] =
    "DocumentImageReplacements";

// static
DocumentImageReplacements* DocumentImageReplacements::FromIfExists(
    Document& document) {
  return Supplement<Document>::From<DocumentImageReplacements>(document);
}

// static
DocumentImageReplacements& DocumentImageReplacements::From(Document& document) {
  DocumentImageReplacements* replacements = FromIfExists(document);
  if (!replacements) {
    replacements = MakeGarbageCollected<DocumentImageReplacements>(document);
    Supplement<Document>::ProvideTo(document, replacements);
  }
  return *replacements;
}

DocumentImageReplacements::DocumentImageReplacements(Document& document)
    : Supplement<Document>(document) {}

void DocumentImageReplacements::RegisterImageReplacement(
    HTMLImageElement* element,
    ImageReplacement* replacement) {
  auto result = replacements_.Set(element, replacement);
  CHECK(result.is_new_entry) << "HTMLImageElement already has a pending "
                                "replacement.";
}

ImageReplacement* DocumentImageReplacements::UnregisterImageReplacement(
    HTMLImageElement* element) {
  return replacements_.Take(element);
}

ImageReplacement* DocumentImageReplacements::GetImageReplacement(
    HTMLImageElement* element) const {
  auto it = replacements_.find(element);
  if (it != replacements_.end()) {
    return it->value.Get();
  }
  return nullptr;
}

void DocumentImageReplacements::Trace(Visitor* visitor) const {
  visitor->Trace(replacements_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
