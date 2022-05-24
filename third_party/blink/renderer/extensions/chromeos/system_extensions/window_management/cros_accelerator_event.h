// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_ACCELERATOR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_ACCELERATOR_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class CrosAcceleratorEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CrosAcceleratorEvent* Create();

  CrosAcceleratorEvent();
  ~CrosAcceleratorEvent() override;

  void Trace(Visitor*) const override;

  const AtomicString& InterfaceName() const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_MANAGEMENT_ACCELERATOR_EVENT_H_
