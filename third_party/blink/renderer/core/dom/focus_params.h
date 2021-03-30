// Copyright 2020 The Chromium Authors. All rights reserved.
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
  FocusParams(SelectionBehaviorOnFocus selection,
              mojom::blink::FocusType focus_type,
              InputDeviceCapabilities* capabilities,
              const FocusOptions* focus_options = FocusOptions::Create())
      : selection_behavior(selection),
        type(focus_type),
        source_capabilities(capabilities),
        options(focus_options) {}

  SelectionBehaviorOnFocus selection_behavior =
      SelectionBehaviorOnFocus::kRestore;
  mojom::blink::FocusType type = mojom::blink::FocusType::kNone;
  InputDeviceCapabilities* source_capabilities = nullptr;
  const FocusOptions* options = nullptr;
  bool omit_blur_events = false;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_FOCUS_PARAMS_H_
