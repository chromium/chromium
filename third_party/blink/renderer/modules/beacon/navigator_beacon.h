// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BEACON_NAVIGATOR_BEACON_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BEACON_NAVIGATOR_BEACON_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ScriptState;
class ExceptionState;
class KURL;
class ArrayBufferViewOrBlobOrStringOrFormData;

class NavigatorBeacon final : public GarbageCollected<NavigatorBeacon>,
                              public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorBeacon);

 public:
  static const char kSupplementName[];

  static NavigatorBeacon& From(Navigator&);

  explicit NavigatorBeacon(Navigator&);
  virtual ~NavigatorBeacon();

  static bool sendBeacon(ScriptState*,
                         Navigator&,
                         const String&,
                         const ArrayBufferViewOrBlobOrStringOrFormData&,
                         ExceptionState&);

  void Trace(blink::Visitor*) override;

 private:
  bool SendBeaconImpl(ScriptState*,
                      const String&,
                      const ArrayBufferViewOrBlobOrStringOrFormData&,
                      ExceptionState&);
  bool CanSendBeacon(ExecutionContext*, const KURL&, ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BEACON_NAVIGATOR_BEACON_H_
