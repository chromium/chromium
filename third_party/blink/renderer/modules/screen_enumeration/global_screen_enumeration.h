// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_GLOBAL_SCREEN_ENUMERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_GLOBAL_SCREEN_ENUMERATION_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class ScriptPromise;
class ScriptState;

// A proposed interface for querying the state of the device's screen space.
// https://github.com/webscreens/window-placement
// TODO(crbug.com/897300): Remove this; use the new WindowScreens supplement.
class GlobalScreenEnumeration {
  STATIC_ONLY(GlobalScreenEnumeration);

 public:
  // Resolves to the list of |Screen| objects in the device's screen space.
  // TODO(crbug.com/897300): Remove this; use the new WindowScreens supplement.
  static ScriptPromise getScreensDeprecated(ScriptState* script_state,
                                            LocalDOMWindow&,
                                            ExceptionState& exception_state);

  // Resolves to true if the number of available screens is greater than one.
  // TODO(crbug.com/897300): Remove this; use Screen.isExtended.
  static ScriptPromise isMultiScreen(ScriptState* script_state,
                                     LocalDOMWindow&,
                                     ExceptionState& exception_state);

  // TODO(crbug.com/897300): Remove this; use Screens.onchange.
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(screenschange, kScreenschange)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ENUMERATION_GLOBAL_SCREEN_ENUMERATION_H_
