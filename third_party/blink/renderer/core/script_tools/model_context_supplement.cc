// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script_tools/model_context_supplement.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

namespace blink {

// static
ModelContextSupplement& ModelContextSupplement::From(Navigator& navigator) {
  ModelContextSupplement* supplement = navigator.GetModelContextSupplement();
  if (!supplement) {
    supplement = MakeGarbageCollected<ModelContextSupplement>(navigator);
    navigator.SetModelContextSupplement(supplement);
  }
  return *supplement;
}

// static
ModelContext* ModelContextSupplement::GetIfExists(Navigator& navigator) {
  ModelContextSupplement* supplement = navigator.GetModelContextSupplement();
  return supplement ? supplement->modelContext() : nullptr;
}

// static
ModelContext* ModelContextSupplement::modelContext(Navigator& navigator) {
  return From(navigator).modelContext();
}

// static
ModelContextTesting* ModelContextSupplement::modelContextTesting(
    Navigator& navigator) {
  return From(navigator).modelContextTesting();
}

ModelContextSupplement::ModelContextSupplement(Navigator& navigator)
    : navigator_(navigator) {}

void ModelContextSupplement::Trace(Visitor* visitor) const {
  visitor->Trace(model_context_);
  visitor->Trace(model_context_testing_);
  visitor->Trace(navigator_);
}

ModelContext* ModelContextSupplement::modelContext() {
  if (!model_context_) {
    if (auto* window = navigator_->DomWindow()) {
      model_context_ = MakeGarbageCollected<ModelContext>(
          window->GetTaskRunner(TaskType::kUserInteraction));
    }
  }
  return model_context_.Get();
}

ModelContextTesting* ModelContextSupplement::modelContextTesting() {
  if (!model_context_testing_ && modelContext()) {
    model_context_testing_ =
        MakeGarbageCollected<ModelContextTesting>(modelContext());
  }
  return model_context_testing_.Get();
}

}  // namespace blink
