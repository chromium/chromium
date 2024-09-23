// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/model/ios_chrome_pref_model_associator_client.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"

IOSChromePrefModelAssociatorClient::IOSChromePrefModelAssociatorClient() {}

IOSChromePrefModelAssociatorClient::~IOSChromePrefModelAssociatorClient() {}

base::Value IOSChromePrefModelAssociatorClient::MaybeMergePreferenceValues(
    std::string_view pref_name,
    const base::Value& local_value,
    const base::Value& server_value) const {
  return base::Value();
}

const sync_preferences::SyncablePrefsDatabase&
IOSChromePrefModelAssociatorClient::GetSyncablePrefsDatabase() const {
  return ios_chrome_syncable_prefs_database_;
}
