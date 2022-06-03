// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VIRTUALKEYBOARD_VIRTUAL_KEYBOARD_GEOMETRY_CHANGE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VIRTUALKEYBOARD_VIRTUAL_KEYBOARD_GEOMETRY_CHANGE_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class VirtualKeyboardGeometryChangeEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static VirtualKeyboardGeometryChangeEvent* Create(const AtomicString& type);

  explicit VirtualKeyboardGeometryChangeEvent(const AtomicString& type);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VIRTUALKEYBOARD_VIRTUAL_KEYBOARD_GEOMETRY_CHANGE_EVENT_H_
