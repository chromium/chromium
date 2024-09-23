// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/device_posture/navigator_device_posture.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/device_posture/device_posture.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
const char NavigatorDevicePosture::kSupplementName[] = "NavigatorDevicePosture";

// static
DevicePosture* NavigatorDevicePosture::devicePosture(Navigator& navigator) {
  DCHECK(RuntimeEnabledFeatures::DevicePostureEnabled(
      navigator.GetExecutionContext()));

  UseCounter::Count(navigator.GetExecutionContext(), WebFeature::kFoldableAPIs);
  NavigatorDevicePosture* supplement =
      Supplement<Navigator>::From<NavigatorDevicePosture>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorDevicePosture>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->posture_.Get();
}

NavigatorDevicePosture::NavigatorDevicePosture(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      posture_(MakeGarbageCollected<DevicePosture>(
          GetSupplementable()->DomWindow())) {}

void NavigatorDevicePosture::Trace(Visitor* visitor) const {
  visitor->Trace(posture_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
