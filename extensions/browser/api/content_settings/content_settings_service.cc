// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/content_settings/content_settings_service.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/extension_prefs_scope.h"
#include "extensions/browser/pref_names.h"

namespace extensions {

ContentSettingsService::ContentSettingsService(content::BrowserContext* context)
    : content_settings_store_(base::MakeRefCounted<ContentSettingsStore>()) {}

ContentSettingsService::~ContentSettingsService() = default;

// static
ContentSettingsService* ContentSettingsService::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ContentSettingsService>::Get(context);
}

// BrowserContextKeyedAPI implementation.
BrowserContextKeyedAPIFactory<ContentSettingsService>*
ContentSettingsService::GetFactoryInstance() {
  static base::LazyInstance<
      BrowserContextKeyedAPIFactory<ContentSettingsService>>::DestructorAtExit
      factory = LAZY_INSTANCE_INITIALIZER;
  return factory.Pointer();
}

void ContentSettingsService::OnExtensionRegistered(
    const std::string& extension_id,
    const base::Time& install_time,
    bool is_enabled) {
  content_settings_store_->RegisterExtension(
      extension_id, install_time, is_enabled);
}

void ContentSettingsService::OnExtensionPrefsLoaded(
    const std::string& extension_id,
    const ExtensionPrefs* prefs) {
  const base::Value::List* content_settings =
      prefs->ReadPrefAsList(extension_id, pref_names::kPrefContentSettings);
  if (content_settings) {
    content_settings_store_->SetExtensionContentSettingFromList(
        extension_id, *content_settings, kExtensionPrefsScopeRegular);
  }
  content_settings = prefs->ReadPrefAsList(
      extension_id, pref_names::kPrefIncognitoContentSettings);
  if (content_settings) {
    content_settings_store_->SetExtensionContentSettingFromList(
        extension_id, *content_settings,
        kExtensionPrefsScopeIncognitoPersistent);
  }
}

void ContentSettingsService::OnExtensionPrefsDeleted(
    const std::string& extension_id) {
  content_settings_store_->UnregisterExtension(extension_id);
}

void ContentSettingsService::OnExtensionStateChanged(
    const std::string& extension_id,
    bool state) {
  content_settings_store_->SetExtensionState(extension_id, state);
}

void ContentSettingsService::OnExtensionPrefsWillBeDestroyed(
    ExtensionPrefs* prefs) {
  DCHECK(scoped_observation_.IsObservingSource(prefs));
  scoped_observation_.Reset();
}

void ContentSettingsService::OnExtensionPrefsAvailable(ExtensionPrefs* prefs) {
  scoped_observation_.Observe(prefs);
}

}  // namespace extensions
