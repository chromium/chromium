// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/guest_view_feature_util.h"

#include "base/feature_list.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension_features.h"

namespace extensions {

bool AreWebviewMPArchBehaviorsEnabled(
    content::BrowserContext* browser_context) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  if (prefs->GetPrefAsBoolean(
          {pref_names::kChromeAppsWebViewPermissiveBehaviorAllowed,
           PrefType::kBool, PrefScope::kProfile})) {
    return false;
  }

  return base::FeatureList::IsEnabled(
      extensions_features::kWebviewTagMPArchBehavior);
}

}  // namespace extensions
