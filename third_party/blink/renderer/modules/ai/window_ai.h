// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_WINDOW_AI_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_WINDOW_AI_H_

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AI;
class LocalDOMWindow;

class WindowAI final : public GarbageCollected<WindowAI>,
                       public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  static WindowAI& From(LocalDOMWindow&);
  static AI* ai(LocalDOMWindow&);

  explicit WindowAI(LocalDOMWindow&);
  WindowAI(const WindowAI&) = delete;
  WindowAI& operator=(const WindowAI&) = delete;

  void Trace(Visitor*) const override;

 private:
  AI* ai();

  Member<AI> ai_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_WINDOW_AI_H_
