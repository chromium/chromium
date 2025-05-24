// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webui/chrome_urls/pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace chrome_urls {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kInternalOnlyUisEnabled, false);
}

}  // namespace chrome_urls
