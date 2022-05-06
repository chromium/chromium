// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_H_

#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class DOMPoint;
class DOMRect;
class CrosWindowManagement;
class ScriptPromise;

class CrosWindow : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CrosWindow(CrosWindowManagement* manager,
             mojom::blink::CrosWindowInfoPtr window);

  void Trace(Visitor*) const override;

  String id();

  String title();
  String appId();
  String windowState();
  bool isFocused();
  String visibilityState();
  DOMPoint* origin();
  DOMRect* bounds();

  ScriptPromise setOrigin(ScriptState* script_state, double x, double y);
  ScriptPromise setBounds(ScriptState* script_state,
                          double x,
                          double y,
                          double width,
                          double height);
  ScriptPromise setFullscreen(ScriptState* script_state, bool fullscreen);
  ScriptPromise maximize(ScriptState* script_state);
  ScriptPromise minimize(ScriptState* script_state);
  ScriptPromise focus(ScriptState* script_state);
  void close();

 private:
  Member<CrosWindowManagement> window_management_;

  mojom::blink::CrosWindowInfoPtr window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_H_
