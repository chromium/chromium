// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"

namespace blink {

const char NavigatorML::kSupplementName[] = "NavigatorML";

NavigatorML::NavigatorML(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ml_(MakeGarbageCollected<ML>(navigator.GetExecutionContext())) {}

ML* NavigatorML::ml(NavigatorBase& navigator) {
  NavigatorML* supplement =
      Supplement<NavigatorBase>::From<NavigatorML>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorML>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->ml_.Get();
}

void NavigatorML::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
  Supplement<NavigatorBase>::Trace(visitor);
}

}  // namespace blink
