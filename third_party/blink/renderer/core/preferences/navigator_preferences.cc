// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/navigator_preferences.h"

#include "third_party/blink/renderer/core/preferences/preference_manager.h"

namespace blink {

NavigatorPreferences& NavigatorPreferences::From(Navigator& navigator) {
  NavigatorPreferences* supplement = navigator.GetNavigatorPreferences();
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorPreferences>(navigator);
    navigator.SetNavigatorPreferences(supplement);
  }
  return *supplement;
}

PreferenceManager* NavigatorPreferences::preferences(Navigator& navigator) {
  return From(navigator).preferences();
}

PreferenceManager* NavigatorPreferences::preferences() {
  return preference_manager_.Get();
}

void NavigatorPreferences::Trace(Visitor* visitor) const {
  visitor->Trace(preference_manager_);
}

NavigatorPreferences::NavigatorPreferences(Navigator& navigator) {
  preference_manager_ =
      MakeGarbageCollected<PreferenceManager>(navigator.GetExecutionContext());
}

}  // namespace blink
