// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_user_activation.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"

namespace blink {

const char NavigatorUserActivation::kSupplementName[] =
    "NavigatorUserActivation";

NavigatorUserActivation& NavigatorUserActivation::From(Navigator& navigator) {
  NavigatorUserActivation* supplement =
      Supplement<Navigator>::From<NavigatorUserActivation>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorUserActivation>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

UserActivation* NavigatorUserActivation::userActivation(Navigator& navigator) {
  return From(navigator).userActivation();
}

UserActivation* NavigatorUserActivation::userActivation() {
  return user_activation_.Get();
}

void NavigatorUserActivation::Trace(Visitor* visitor) const {
  visitor->Trace(user_activation_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorUserActivation::NavigatorUserActivation(Navigator& navigator)
    : Supplement(navigator) {
  user_activation_ =
      MakeGarbageCollected<UserActivation>(navigator.DomWindow());
}

}  // namespace blink
