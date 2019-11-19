// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_NAVIGATOR_HID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_NAVIGATOR_HID_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class HID;

class NavigatorHID final : public GarbageCollected<NavigatorHID>,
                           public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorHID);

 public:
  static const char kSupplementName[];

  // Gets, or creates, NavigatorHID supplement on Navigator.
  // See platform/Supplementable.h
  static NavigatorHID& From(Navigator&);

  static HID* hid(Navigator&);
  HID* hid();

  void Trace(blink::Visitor*) override;

  explicit NavigatorHID(Navigator&);

 private:
  Member<HID> hid_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_NAVIGATOR_HID_H_
