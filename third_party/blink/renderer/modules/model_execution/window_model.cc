// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/model_execution/window_model.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/model_execution/model_manager.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

WindowModel::WindowModel(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void WindowModel::Trace(Visitor* visitor) const {
  visitor->Trace(model_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

// static
const char WindowModel::kSupplementName[] = "WindowModel";

// static
WindowModel& WindowModel::From(LocalDOMWindow& window) {
  WindowModel* supplement =
      Supplement<LocalDOMWindow>::From<WindowModel>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<WindowModel>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

// static
ModelManager* WindowModel::model(LocalDOMWindow& window) {
  return From(window).model();
}

ModelManager* WindowModel::model() {
  if (!model_) {
    model_ = MakeGarbageCollected<ModelManager>(GetSupplementable());
  }
  return model_.Get();
}

}  // namespace blink
