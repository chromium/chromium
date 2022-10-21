// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUS_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUS_PARAMS_H_

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_focus_options.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

struct FocusParams {
  STACK_ALLOCATED();

 public:
  FocusParams() : options(FocusOptions::Create()) {}
  explicit FocusParams(bool gate_on_user_activation)
      : options(FocusOptions::Create()),
        gate_on_user_activation(gate_on_user_activation) {}
  FocusParams(SelectionBehaviorOnFocus selection,
              mojom::blink::FocusType focus_type,
              InputDeviceCapabilities* capabilities,
              const FocusOptions* focus_options = FocusOptions::Create(),
              bool gate_on_user_activation = false)
      : selection_behavior(selection),
        type(focus_type),
        source_capabilities(capabilities),
        options(focus_options),
        gate_on_user_activation(gate_on_user_activation) {}

  SelectionBehaviorOnFocus selection_behavior =
      SelectionBehaviorOnFocus::kRestore;
  mojom::blink::FocusType type = mojom::blink::FocusType::kNone;
  InputDeviceCapabilities* source_capabilities = nullptr;
  const FocusOptions* options = nullptr;
  bool omit_blur_events = false;
  bool gate_on_user_activation = false;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUS_PARAMS_H_
