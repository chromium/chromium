// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBUI_CHROME_URLS_PREF_NAMES_H_
#define COMPONENTS_WEBUI_CHROME_URLS_PREF_NAMES_H_

class PrefRegistrySimple;

namespace chrome_urls {

// Register preference names for chrome urls features.
void RegisterPrefs(PrefRegistrySimple* registry);

inline constexpr char kInternalOnlyUisEnabled[] = "internal_only_uis_enabled";

}  // namespace chrome_urls

#endif  // COMPONENTS_WEBUI_CHROME_URLS_PREF_NAMES_H_
