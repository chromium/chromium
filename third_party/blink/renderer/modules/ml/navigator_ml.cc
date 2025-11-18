// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"

namespace blink {

NavigatorML::NavigatorML(NavigatorBase& navigator)
    : ml_(MakeGarbageCollected<ML>(navigator.GetExecutionContext())) {}

ML* NavigatorML::ml(NavigatorBase& navigator) {
  NavigatorML* supplement = navigator.GetNavigatorML();
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorML>(navigator);
    navigator.SetNavigatorML(supplement);
  }
  return supplement->ml_.Get();
}

void NavigatorML::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
}

}  // namespace blink
