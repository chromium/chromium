// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_EVENT_H_

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window.h"

namespace blink {

class CrosWindowEventInit;

class CrosWindowEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CrosWindowEvent* Create(const AtomicString& type,
                                 const CrosWindowEventInit* event_init);

  CrosWindowEvent(const AtomicString& type,
                  const CrosWindowEventInit* event_init);
  ~CrosWindowEvent() override;

  CrosWindow* window() { return window_; }

  void Trace(Visitor*) const override;

  const AtomicString& InterfaceName() const override;

 private:
  Member<CrosWindow> window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_WINDOW_MANAGEMENT_CROS_WINDOW_EVENT_H_
