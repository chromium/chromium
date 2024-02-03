// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/content_settings/content_settings_service.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/api/types.h"
#include "extensions/common/extension_id.h"

using extensions::api::types::ChromeSettingScope;

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
    const ExtensionId& extension_id,
    const base::Time& install_time,
    bool is_enabled) {
  content_settings_store_->RegisterExtension(
      extension_id, install_time, is_enabled);
}

void ContentSettingsService::OnExtensionPrefsLoaded(
    const ExtensionId& extension_id,
    const ExtensionPrefs* prefs) {
  const base::Value::List* content_settings =
      prefs->ReadPrefAsList(extension_id, pref_names::kPrefContentSettings);
  if (content_settings) {
    content_settings_store_->SetExtensionContentSettingFromList(
        extension_id, *content_settings, ChromeSettingScope::kRegular);
  }
  content_settings = prefs->ReadPrefAsList(
      extension_id, pref_names::kPrefIncognitoContentSettings);
  if (content_settings) {
    content_settings_store_->SetExtensionContentSettingFromList(
        extension_id, *content_settings,
        ChromeSettingScope::kIncognitoPersistent);
  }
}

void ContentSettingsService::OnExtensionPrefsDeleted(
    const ExtensionId& extension_id) {
  content_settings_store_->UnregisterExtension(extension_id);
}

void ContentSettingsService::OnExtensionStateChanged(
    const ExtensionId& extension_id,
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
