// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

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
ModelContext* ModelContextSupplement::modelContext(Navigator& navigator) {
  auto* window = navigator.DomWindow();
  if (!window || !window->document()) {
    return nullptr;
  }
  window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
                                mojom::blink::ConsoleMessageSource::kJavaScript,
                                mojom::blink::ConsoleMessageLevel::kWarning,
                                "navigator.modelContext is deprecated. Please "
                                "use document.modelContext instead."),
                            /*discard_duplicates=*/true);
  return From(*window->document()).modelContext();
}

// static
ModelContext* ModelContextSupplement::modelContext(Document& document) {
  return From(document).modelContext();
}

ModelContextSupplement::ModelContextSupplement(Document& document)
    : Supplement<Document>(document) {}

void ModelContextSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(model_context_);
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

}  // namespace blink
