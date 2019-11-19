// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_INFO_MAP_H_
#define EXTENSIONS_BROWSER_INFO_MAP_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/quota_service.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {
class ContentVerifier;
class Extension;

// Contains extension data that needs to be accessed on the IO thread. It can
// be created on any thread, but all other methods and destructor must be called
// on the IO thread.
// TODO(http://crbug.com/980774): Audit this to see what is still necessary.
class InfoMap : public base::RefCountedThreadSafe<
                    InfoMap,
                    content::BrowserThread::DeleteOnIOThread> {
 public:
  InfoMap();

  const ExtensionSet& extensions() const;
  const ExtensionSet& disabled_extensions() const;

  // Information about which extensions are assigned to which render processes.
  const ProcessMap& process_map() const { return process_map_; }

  // Callback for when new extensions are loaded.
  void AddExtension(const Extension* extension,
                    base::Time install_time,
                    bool incognito_enabled,
                    bool notifications_disabled);

  // Callback for when an extension is unloaded.
  void RemoveExtension(const std::string& extension_id,
                       const UnloadedExtensionReason reason);

  // Returns the time the extension was installed, or base::Time() if not found.
  base::Time GetInstallTime(const std::string& extension_id) const;

  // Returns true if the user has allowed this extension to run in incognito
  // mode.
  bool IsIncognitoEnabled(const std::string& extension_id) const;

  // Returns true if the given extension can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  bool CanCrossIncognito(const Extension* extension) const;

  // Adds an entry to process_map_.
  void RegisterExtensionProcess(const std::string& extension_id,
                                int process_id,
                                int site_instance_id);

  // Removes an entry from process_map_.
  void UnregisterExtensionProcess(const std::string& extension_id,
                                  int process_id,
                                  int site_instance_id);
  void UnregisterAllExtensionsInProcess(int process_id);

  // Returns the IO thread QuotaService. Creates the instance on first call.
  QuotaService* GetQuotaService();

  // Notifications can be enabled/disabled in real time by the user.
  void SetNotificationsDisabled(const std::string& extension_id,
                                bool notifications_disabled);
  bool AreNotificationsDisabled(const std::string& extension_id) const;

  void SetContentVerifier(ContentVerifier* verifier);
  ContentVerifier* content_verifier() { return content_verifier_.get(); }

  // Marks the extensions in this info map as running in lock screen context.
  void SetIsLockScreenContext(bool is_lock_screen_context);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<InfoMap>;

  // Extra dynamic data related to an extension.
  struct ExtraData;
  // Map of extension_id to ExtraData.
  typedef std::map<std::string, ExtraData> ExtraDataMap;

  ~InfoMap();

  ExtensionSet extensions_;
  ExtensionSet disabled_extensions_;

  // Extra data associated with enabled extensions.
  ExtraDataMap extra_data_;

  // Used by dispatchers to limit API quota for individual extensions.
  // The QuotaService is not thread safe. We need to create and destroy it on
  // the IO thread.
  std::unique_ptr<QuotaService> quota_service_;

  // Assignment of extensions to renderer processes.
  ProcessMap process_map_;

  scoped_refptr<ContentVerifier> content_verifier_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_INFO_MAP_H_
