// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/script_tools/dom_model_context.h"

namespace blink {

// static
const char ModelContextSupplement::kSupplementName[] = "ModelContextSupplement";

// static
ModelContextSupplement& ModelContextSupplement::From(Document& document) {
  ModelContextSupplement* supplement =
      Supplement<Document>::From<ModelContextSupplement>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<ModelContextSupplement>(document);
    ProvideTo(document, supplement);
  }
  return *supplement;
}

// static
ModelContext* ModelContextSupplement::GetIfExists(Document& document) {
  ModelContextSupplement* supplement =
      Supplement<Document>::From<ModelContextSupplement>(document);
  return supplement ? supplement->model_context_.Get() : nullptr;
}

// static
DOMModelContext* ModelContextSupplement::DOMModelContextIfExists(
    Document& document) {
  ModelContextSupplement* supplement =
      Supplement<Document>::From<ModelContextSupplement>(document);
  return supplement ? supplement->dom_model_context_.Get() : nullptr;
}

// static
ModelContext* ModelContextSupplement::modelContext(Document& document) {
  return From(document).modelContext();
}

// static
DOMModelContext* ModelContextSupplement::domModelContext(Document& document) {
  return From(document).domModelContext();
}

ModelContextSupplement::ModelContextSupplement(Document& document)
    : Supplement<Document>(document) {}

void ModelContextSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(model_context_);
  visitor->Trace(dom_model_context_);
  Supplement<Document>::Trace(visitor);
}

ModelContext* ModelContextSupplement::modelContext() {
  if (!model_context_) {
    Document* document = GetSupplementable();
    CHECK(document);
    model_context_ = MakeGarbageCollected<ModelContext>(*document);
  }
  return model_context_.Get();
}

DOMModelContext* ModelContextSupplement::domModelContext() {
  if (!dom_model_context_) {
    ModelContext* model_context = modelContext();
    dom_model_context_ = MakeGarbageCollected<DOMModelContext>(
        *GetSupplementable(), *model_context);
  }
  return dom_model_context_.Get();
}

}  // namespace blink
