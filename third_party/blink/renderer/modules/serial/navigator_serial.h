// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_NAVIGATOR_SERIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_NAVIGATOR_SERIAL_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Serial;

class NavigatorSerial final : public GarbageCollected<NavigatorSerial>,
                              public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorSerial);

 public:
  static const char kSupplementName[];

  static NavigatorSerial& From(Navigator&);

  static Serial* serial(Navigator&);
  Serial* serial() { return serial_; }

  void Trace(Visitor*) override;

 private:
  explicit NavigatorSerial(Navigator&);

  Member<Serial> serial_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_NAVIGATOR_SERIAL_H_
