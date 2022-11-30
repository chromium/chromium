// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_ACCELERATOR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_ACCELERATOR_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class CrosAcceleratorEventInit;

class CrosAcceleratorEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CrosAcceleratorEvent* Create(
      const AtomicString& type,
      const CrosAcceleratorEventInit* event_init);

  CrosAcceleratorEvent(const AtomicString& type,
                       const CrosAcceleratorEventInit* event_init);
  ~CrosAcceleratorEvent() override;

  const String& acceleratorName() { return accelerator_name_; }
  bool repeat() { return repeat_; }

  void Trace(Visitor*) const override;

  const AtomicString& InterfaceName() const override;

 private:
  const String accelerator_name_;
  const bool repeat_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_ACCELERATOR_EVENT_H_
