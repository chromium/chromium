// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_WINDOW_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_WINDOW_MODEL_H_

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ModelManager;
class LocalDOMWindow;

class WindowModel final : public GarbageCollected<WindowModel>,
                          public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static WindowModel& From(LocalDOMWindow&);
  static ModelManager* model(LocalDOMWindow&);

  explicit WindowModel(LocalDOMWindow&);
  WindowModel(const WindowModel&) = delete;
  WindowModel& operator=(const WindowModel&) = delete;

  void Trace(Visitor*) const override;

 private:
  ModelManager* model();

  Member<ModelManager> model_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODEL_EXECUTION_WINDOW_MODEL_H_
