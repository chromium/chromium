// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

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
ModelContext* ModelContextSupplement::GetIfExists(Navigator& navigator) {
  auto* window = navigator.DomWindow();
  if (!window || !window->document()) {
    return nullptr;
  }
  ModelContextSupplement* supplement =
      Supplement<Document>::From<ModelContextSupplement>(*window->document());
  return supplement ? supplement->modelContext() : nullptr;
}

// static
ModelContext* ModelContextSupplement::modelContext(Navigator& navigator) {
  auto* window = navigator.DomWindow();
  if (!window || !window->document()) {
    return nullptr;
  }
  return From(*window->document()).modelContext();
}

// static
ModelContextTesting* ModelContextSupplement::modelContextTesting(
    Navigator& navigator) {
  auto* window = navigator.DomWindow();
  if (!window || !window->document()) {
    return nullptr;
  }
  return From(*window->document()).modelContextTesting();
}

ModelContextSupplement::ModelContextSupplement(Document& document)
    : Supplement<Document>(document) {}

void ModelContextSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(model_context_);
  visitor->Trace(model_context_testing_);
  Supplement<Document>::Trace(visitor);
}

ModelContext* ModelContextSupplement::modelContext() {
  if (!model_context_) {
    Document* document = GetSupplementable();
    CHECK(document);
    auto* window = document->domWindow();
    if (window) {
      model_context_ = MakeGarbageCollected<ModelContext>(
          *document, window->GetTaskRunner(TaskType::kUserInteraction));
    }
  }
  return model_context_.Get();
}

ModelContextTesting* ModelContextSupplement::modelContextTesting() {
  if (!model_context_testing_ && modelContext()) {
    model_context_testing_ =
        MakeGarbageCollected<ModelContextTesting>(*modelContext());
  }
  return model_context_testing_.Get();
}

}  // namespace blink
