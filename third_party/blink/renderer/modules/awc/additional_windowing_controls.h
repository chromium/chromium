// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AWC_ADDITIONAL_WINDOWING_CONTROLS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AWC_ADDITIONAL_WINDOWING_CONTROLS_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class ScriptPromise;
class ScriptState;

// Complements LocalDOMWindow with additional windowing controls.
class AdditionalWindowingControls {
  STATIC_ONLY(AdditionalWindowingControls);

 public:
  // Web-exposed interfaces:
  static ScriptPromise maximize(ScriptState*,
                                LocalDOMWindow&,
                                ExceptionState& exception_state);
  static ScriptPromise minimize(ScriptState*,
                                LocalDOMWindow&,
                                ExceptionState& exception_state);
  static ScriptPromise restore(ScriptState*,
                               LocalDOMWindow&,
                               ExceptionState& exception_state);
  static ScriptPromise setResizable(ScriptState*,
                                    LocalDOMWindow&,
                                    bool resizable,
                                    ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AWC_ADDITIONAL_WINDOWING_CONTROLS_H_
