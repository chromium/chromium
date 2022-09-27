// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_H_

#include <cstdint>
#include "third_party/blink/public/mojom/chromeos/system_extensions/window_management/cros_window_management.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class CrosWindowManagement;
class ScriptPromise;

class CrosWindow : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CrosWindow(CrosWindowManagement* manager, base::UnguessableToken id);
  CrosWindow(CrosWindowManagement* manager,
             mojom::blink::CrosWindowInfoPtr window);

  void Trace(Visitor*) const override;

  // Sets the CrosWindowInfoPtr to `window_info_ptr`.
  void Update(mojom::blink::CrosWindowInfoPtr window_info_ptr);

  String id();

  String title();
  String appId();
  String windowState();
  bool isFocused();
  String visibilityState();
  int32_t screenLeft();
  int32_t screenTop();
  int32_t width();
  int32_t height();

  ScriptPromise moveTo(ScriptState* script_state, int32_t x, int32_t y);
  ScriptPromise moveBy(ScriptState* script_state,
                       int32_t delta_x,
                       int32_t delta_y);
  ScriptPromise resizeTo(ScriptState* script_state,
                         int32_t width,
                         int32_t height);
  ScriptPromise resizeBy(ScriptState* script_state,
                         int32_t delta_width,
                         int32_t delta_height);
  ScriptPromise setFullscreen(ScriptState* script_state, bool fullscreen);
  ScriptPromise maximize(ScriptState* script_state);
  ScriptPromise minimize(ScriptState* script_state);
  ScriptPromise restore(ScriptState* script_state);
  ScriptPromise focus(ScriptState* script_state);
  ScriptPromise close(ScriptState* script_state);

 private:
  Member<CrosWindowManagement> window_management_;

  mojom::blink::CrosWindowInfoPtr window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_H_
