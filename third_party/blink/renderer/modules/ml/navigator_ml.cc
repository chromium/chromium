// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"

namespace blink {

const char NavigatorML::kSupplementName[] = "NavigatorML";

NavigatorML::NavigatorML(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      ml_(MakeGarbageCollected<ML>(navigator.GetExecutionContext())) {}

ML* NavigatorML::ml(Navigator& navigator) {
  NavigatorML* supplement = Supplement<Navigator>::From<NavigatorML>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorML>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement->ml_;
}

void NavigatorML::Trace(Visitor* visitor) const {
  visitor->Trace(ml_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
