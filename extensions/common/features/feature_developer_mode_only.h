// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_DEVELOPER_MODE_ONLY_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_DEVELOPER_MODE_ONLY_H_

namespace extensions {

// Gets the developer mode as seen by the Feature system.
bool GetCurrentDeveloperMode(int context_id);

// Sets the current developer mode as seen by the Feature system for the
// current process.
// Note: If you want to change this value, you may be looking for
// util::SetDeveloperModeForProfile() (which updates all associated
// processes).
void SetCurrentDeveloperMode(int context_id, bool current_developer_mode);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_DEVELOPER_MODE_ONLY_H_
