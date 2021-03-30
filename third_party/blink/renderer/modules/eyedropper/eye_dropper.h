// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_EYEDROPPER_EYE_DROPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_EYEDROPPER_EYE_DROPPER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// The EyeDropper API enables developers to use a browser-supplied eyedropper
// in their web applications. This feature is still
// under development, and is not part of the standard. It can be enabled
// by passing --enable-blink-features=EyeDropperAPI. See
// https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/EyeDropper/explainer.md
// for more details.
class EyeDropper final : public EventTargetWithInlineData,
                         public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit EyeDropper(ScriptState*);
  static EyeDropper* Create(ScriptState*);
  EyeDropper(const EyeDropper&) = delete;
  EyeDropper& operator=(const EyeDropper&) = delete;
  ~EyeDropper() override;

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // Opens the eyedropper and replaces the cursor with a browser-defined preview
  // and sets opened boolean to true.
  ScriptPromise open();

  // Exits the eyedropper mode and the cursor returns to its regular
  // functionality. Sets opened boolean to false.
  void close();

  // States if the eyedropper is opened and in use.
  bool opened() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(colorselect, kColorselect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)

  void Trace(Visitor*) const override;

 private:
  bool opened_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_EYEDROPPER_EYE_DROPPER_H_
