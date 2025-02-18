// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_service.h"

#import "components/prefs/pref_registry_simple.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

namespace auto_deletion {

AutoDeletionService::AutoDeletionService() {}

AutoDeletionService::~AutoDeletionService() = default;

// static
void AutoDeletionService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDownloadAutoDeletionEnabled, false);
  registry->RegisterBooleanPref(prefs::kDownloadAutoDeletionIPHShown, false);
}
}  // namespace auto_deletion
