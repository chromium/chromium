// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_NAVIGATOR_SMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_NAVIGATOR_SMS_H_

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class ScriptState;
class SMSReceiver;

class NavigatorSMS final : public GarbageCollected<NavigatorSMS>,
                           public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorSMS);

 public:
  static const char kSupplementName[];

  static NavigatorSMS& From(Navigator&);

  static SMSReceiver* sms(ScriptState*, Navigator&);

  explicit NavigatorSMS(Navigator&);

  void Trace(blink::Visitor*) override;

 private:
  SMSReceiver* GetSMSReceiver(ScriptState*);

  Member<SMSReceiver> sms_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_NAVIGATOR_SMS_H_
