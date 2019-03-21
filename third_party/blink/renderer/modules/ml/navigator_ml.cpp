// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/navigator_ml.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/ml/ml.h"

namespace blink {

NavigatorML::NavigatorML(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
const char NavigatorML::kSupplementName[] = "NavigatorML";

// static
NavigatorML& NavigatorML::From(Navigator& navigator) {
  NavigatorML* supplement = Supplement<Navigator>::From<NavigatorML>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorML>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
ML* NavigatorML::ml(Navigator& navigator) {
  if (!navigator.GetFrame())
    return nullptr;

  NavigatorML& self = NavigatorML::From(navigator);
  if (!self.ml_)
    self.ml_ = MakeGarbageCollected<ML>(self);

  return self.ml_.Get();
}

Document* NavigatorML::GetDocument() {
  if (!GetSupplementable() || !GetSupplementable()->GetFrame())
    return nullptr;

  return GetSupplementable()->GetFrame()->GetDocument();
}

void NavigatorML::Trace(blink::Visitor* visitor) {
  visitor->Trace(ml_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
