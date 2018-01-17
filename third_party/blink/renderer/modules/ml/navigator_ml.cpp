// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/ml/NavigatorML.h"

#include "core/frame/Navigator.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/ml/ML.h"

namespace blink {

NavigatorML::NavigatorML(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

const char* NavigatorML::SupplementName() {
  return "NavigatorML";
}

NavigatorML& NavigatorML::From(Navigator& navigator) {
  NavigatorML* supplement = static_cast<NavigatorML*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorML(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

Document* NavigatorML::GetDocument() {
  if (!GetSupplementable() || !GetSupplementable()->GetFrame())
    return nullptr;

  return GetSupplementable()->GetFrame()->GetDocument();
}

ML* NavigatorML::ml(Navigator& navigator) {
  NavigatorML& self = NavigatorML::From(navigator);
  if (!self.ml_) {
    if (!navigator.GetFrame())
      return nullptr;
    self.ml_ = new ML(&self);
  }
  return self.ml_.Get();
}

void NavigatorML::Trace(blink::Visitor* visitor) {
  visitor->Trace(ml_);
  Supplement<Navigator>::Trace(visitor);
}

void NavigatorML::TraceWrappers(const ScriptWrappableVisitor* visitor) const {
  visitor->TraceWrappers(ml_);
  Supplement<Navigator>::TraceWrappers(visitor);
}

}  // namespace blink
