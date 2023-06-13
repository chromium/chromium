// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_part.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_documentfragment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

DocumentPart::DocumentPart(const V8UnionDocumentOrDocumentFragment* document) {
  switch (document->GetContentType()) {
    case V8UnionDocumentOrDocumentFragment::ContentType::kDocument:
      document_ = document->GetAsDocument();
      break;
    case V8UnionDocumentOrDocumentFragment::ContentType::kDocumentFragment:
      document_fragment_ = document->GetAsDocumentFragment();
      break;
    default:
      NOTREACHED();
  }
}
DocumentPart::~DocumentPart() = default;

void DocumentPart::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(document_fragment_);

  PartRoot::Trace(visitor);
}

// TODO(crbug.com/1453291) Implement this method.
DocumentPart* DocumentPart::clone() const {
  return nullptr;
}

}  // namespace blink
