// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/battery/navigator_battery.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/battery/battery_manager.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

NavigatorBattery::NavigatorBattery(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

ScriptPromise NavigatorBattery::getBattery(ScriptState* script_state,
                                           Navigator& navigator) {
  return NavigatorBattery::From(navigator).getBattery(script_state);
}

ScriptPromise NavigatorBattery::getBattery(ScriptState* script_state) {
  ExecutionContext* context = ExecutionContext::From(script_state);

  // Check to see if this request would be blocked according to the Battery
  // Status API specification.
  if (auto* document = To<Document>(context)) {
    LocalFrame* frame = document->GetFrame();
    if (frame) {
      if (!context->IsSecureContext())
        UseCounter::Count(document, WebFeature::kBatteryStatusInsecureOrigin);
      frame->CountUseIfFeatureWouldBeBlockedByFeaturePolicy(
          WebFeature::kBatteryStatusCrossOrigin,
          WebFeature::kBatteryStatusSameOriginABA);
    }
  }

  if (!battery_manager_)
    battery_manager_ = BatteryManager::Create(context);
  return battery_manager_->StartRequest(script_state);
}

const char NavigatorBattery::kSupplementName[] = "NavigatorBattery";

NavigatorBattery& NavigatorBattery::From(Navigator& navigator) {
  NavigatorBattery* supplement =
      Supplement<Navigator>::From<NavigatorBattery>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorBattery>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

void NavigatorBattery::Trace(blink::Visitor* visitor) {
  visitor->Trace(battery_manager_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
