// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_SERVICE_H_
#define EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "extensions/browser/api/content_settings/content_settings_store.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ContentSettingsStore;

// This service hosts a single ContentSettingsStore for the
// chrome.contentSettings API.
class ContentSettingsService : public BrowserContextKeyedAPI,
                               public ExtensionPrefsObserver,
                               public EarlyExtensionPrefsObserver {
 public:
  explicit ContentSettingsService(content::BrowserContext* context);

  ContentSettingsService(const ContentSettingsService&) = delete;
  ContentSettingsService& operator=(const ContentSettingsService&) = delete;

  ~ContentSettingsService() override;

  scoped_refptr<ContentSettingsStore> content_settings_store() const {
    return content_settings_store_;
  }

  // Convenience function to get the service for some browser context.
  static ContentSettingsService* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ContentSettingsService>*
      GetFactoryInstance();

  // ExtensionPrefsObserver implementation.
  void OnExtensionRegistered(const ExtensionId& extension_id,
                             const base::Time& install_time,
                             bool is_enabled) override;
  void OnExtensionPrefsLoaded(const ExtensionId& extension_id,
                              const ExtensionPrefs* prefs) override;
  void OnExtensionPrefsDeleted(const ExtensionId& extension_id) override;
  void OnExtensionStateChanged(const ExtensionId& extension_id,
                               bool state) override;
  void OnExtensionPrefsWillBeDestroyed(ExtensionPrefs* prefs) override;

  // EarlyExtensionPrefsObserver implementation.
  void OnExtensionPrefsAvailable(ExtensionPrefs* prefs) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ContentSettingsService>;

  // BrowserContextKeyedAPI implementation.
  static const bool kServiceRedirectedInIncognito = true;
  static const char* service_name() { return "ContentSettingsService"; }

  scoped_refptr<ContentSettingsStore> content_settings_store_;
  base::ScopedObservation<ExtensionPrefs, ExtensionPrefsObserver>
      scoped_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CONTENT_SETTINGS_CONTENT_SETTINGS_SERVICE_H_
