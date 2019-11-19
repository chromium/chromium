// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sms/navigator_sms.h"

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/sms/sms_receiver.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

NavigatorSMS::NavigatorSMS(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

const char NavigatorSMS::kSupplementName[] = "NavigatorSMS";

NavigatorSMS& NavigatorSMS::From(Navigator& navigator) {
  NavigatorSMS* supplement =
      Supplement<Navigator>::From<NavigatorSMS>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorSMS>(navigator);
    Supplement<Navigator>::ProvideTo(navigator, supplement);
  }
  return *supplement;
}

SMSReceiver* NavigatorSMS::GetSMSReceiver(ScriptState* script_state) {
  if (!sms_receiver_) {
    sms_receiver_ =
        MakeGarbageCollected<SMSReceiver>(ExecutionContext::From(script_state));
  }
  return sms_receiver_.Get();
}

SMSReceiver* NavigatorSMS::sms(ScriptState* script_state,
                               Navigator& navigator) {
  return NavigatorSMS::From(navigator).GetSMSReceiver(script_state);
}

void NavigatorSMS::Trace(blink::Visitor* visitor) {
  visitor->Trace(sms_receiver_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
