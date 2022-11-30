// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/prefs/ios_chrome_pref_model_associator_client.h"

#include "base/no_destructor.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"

// static
IOSChromePrefModelAssociatorClient*
IOSChromePrefModelAssociatorClient::GetInstance() {
  static base::NoDestructor<IOSChromePrefModelAssociatorClient> instance;
  return instance.get();
}

IOSChromePrefModelAssociatorClient::IOSChromePrefModelAssociatorClient() {}

IOSChromePrefModelAssociatorClient::~IOSChromePrefModelAssociatorClient() {}

bool IOSChromePrefModelAssociatorClient::IsMergeableListPreference(
    const std::string& pref_name) const {
  return false;
}

bool IOSChromePrefModelAssociatorClient::IsMergeableDictionaryPreference(
    const std::string& pref_name) const {
  const content_settings::WebsiteSettingsRegistry& registry =
      *content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const content_settings::WebsiteSettingsInfo* info : registry) {
    if (info->pref_name() == pref_name)
      return true;
  }
  return false;
}

base::Value IOSChromePrefModelAssociatorClient::MaybeMergePreferenceValues(
    const std::string& pref_name,
    const base::Value& local_value,
    const base::Value& server_value) const {
  return base::Value();
}
