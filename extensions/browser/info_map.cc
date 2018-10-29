// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/info_map.h"

#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

namespace {

void CheckOnValidThread() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

}  // namespace

struct InfoMap::ExtraData {
  // When the extension was installed.
  base::Time install_time;

  // True if the user has allowed this extension to run in incognito mode.
  bool incognito_enabled;

  // True if the user has disabled notifications for this extension manually.
  bool notifications_disabled;

  ExtraData();
  ~ExtraData();
};

InfoMap::ExtraData::ExtraData()
    : incognito_enabled(false), notifications_disabled(false) {
}

InfoMap::ExtraData::~ExtraData() {}

InfoMap::InfoMap() : ruleset_manager_(this) {}

const ExtensionSet& InfoMap::extensions() const {
  CheckOnValidThread();
  return extensions_;
}

const ExtensionSet& InfoMap::disabled_extensions() const {
  CheckOnValidThread();
  return disabled_extensions_;
}

void InfoMap::AddExtension(const Extension* extension,
                           base::Time install_time,
                           bool incognito_enabled,
                           bool notifications_disabled) {
  CheckOnValidThread();
  extensions_.Insert(extension);
  disabled_extensions_.Remove(extension->id());

  extra_data_[extension->id()].install_time = install_time;
  extra_data_[extension->id()].incognito_enabled = incognito_enabled;
  extra_data_[extension->id()].notifications_disabled = notifications_disabled;
}

void InfoMap::RemoveExtension(const std::string& extension_id,
                              const UnloadedExtensionReason reason) {
  CheckOnValidThread();
  const Extension* extension = extensions_.GetByID(extension_id);
  extra_data_.erase(extension_id);  // we don't care about disabled extra data
  bool was_uninstalled = (reason != UnloadedExtensionReason::DISABLE &&
                          reason != UnloadedExtensionReason::TERMINATE);
  if (extension) {
    if (!was_uninstalled)
      disabled_extensions_.Insert(extension);
    extensions_.Remove(extension_id);
  } else if (was_uninstalled) {
    // If the extension was uninstalled, make sure it's removed from the map of
    // disabled extensions.
    disabled_extensions_.Remove(extension_id);
  } else {
    // NOTE: This can currently happen if we receive multiple unload
    // notifications, e.g. setting incognito-enabled state for a
    // disabled extension (e.g., via sync).  See
    // http://code.google.com/p/chromium/issues/detail?id=50582 .
    NOTREACHED() << extension_id;
  }
}

base::Time InfoMap::GetInstallTime(const std::string& extension_id) const {
  auto iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.install_time;
  return base::Time();
}

bool InfoMap::IsIncognitoEnabled(const std::string& extension_id) const {
  // Keep in sync with duplicate in extensions/browser/process_manager.cc.
  auto iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.incognito_enabled;
  return false;
}

bool InfoMap::CanCrossIncognito(const Extension* extension) const {
  // This is duplicated from ExtensionService :(.
  return IsIncognitoEnabled(extension->id()) &&
         !IncognitoInfo::IsSplitMode(extension);
}

void InfoMap::RegisterExtensionProcess(const std::string& extension_id,
                                       int process_id,
                                       int site_instance_id) {
  if (!process_map_.Insert(extension_id, process_id, site_instance_id)) {
    NOTREACHED() << "Duplicate extension process registration for: "
                 << extension_id << "," << process_id << ".";
  }
}

void InfoMap::UnregisterExtensionProcess(const std::string& extension_id,
                                         int process_id,
                                         int site_instance_id) {
  if (!process_map_.Remove(extension_id, process_id, site_instance_id)) {
    NOTREACHED() << "Unknown extension process registration for: "
                 << extension_id << "," << process_id << ".";
  }
}

void InfoMap::UnregisterAllExtensionsInProcess(int process_id) {
  process_map_.RemoveAllFromProcess(process_id);
}

// This function is security sensitive. Bugs could cause problems that break
// restrictions on local file access or NaCl's validation caching. If you modify
// this function, please get a security review from a NaCl person.
bool InfoMap::MapUrlToLocalFilePath(const GURL& file_url,
                                    bool use_blocking_api,
                                    base::FilePath* file_path) {
  // Check that the URL is recognized by the extension system.
  const Extension* extension = extensions_.GetExtensionOrAppByURL(file_url);
  if (!extension)
    return false;

  // This is a short-cut which avoids calling a blocking file operation
  // (GetFilePath()), so that this can be called on the IO thread. It only
  // handles a subset of the urls.
  if (!use_blocking_api) {
    if (file_url.SchemeIs(extensions::kExtensionScheme)) {
      std::string path = file_url.path();
      base::TrimString(path, "/", &path);  // Remove first slash
      *file_path = extension->path().AppendASCII(path);
      return true;
    }
    return false;
  }

  std::string path = file_url.path();
  ExtensionResource resource;

  if (SharedModuleInfo::IsImportedPath(path)) {
    // Check if this is a valid path that is imported for this extension.
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(
        path, &new_extension_id, &new_relative_path);
    const Extension* new_extension = extensions_.GetByID(new_extension_id);
    if (!new_extension)
      return false;

    if (!SharedModuleInfo::ImportsExtensionById(extension, new_extension_id))
      return false;

    resource = new_extension->GetResource(new_relative_path);
  } else {
    // Check that the URL references a resource in the extension.
    resource = extension->GetResource(path);
  }

  if (resource.empty())
    return false;

  // GetFilePath is a blocking function call.
  const base::FilePath resource_file_path = resource.GetFilePath();
  if (resource_file_path.empty())
    return false;

  *file_path = resource_file_path;
  return true;
}

QuotaService* InfoMap::GetQuotaService() {
  CheckOnValidThread();
  if (!quota_service_)
    quota_service_.reset(new QuotaService());
  return quota_service_.get();
}

declarative_net_request::RulesetManager* InfoMap::GetRulesetManager() {
  CheckOnValidThread();
  return &ruleset_manager_;
}

const declarative_net_request::RulesetManager* InfoMap::GetRulesetManager()
    const {
  CheckOnValidThread();
  return &ruleset_manager_;
}

void InfoMap::SetNotificationsDisabled(
    const std::string& extension_id,
    bool notifications_disabled) {
  auto iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    iter->second.notifications_disabled = notifications_disabled;
}

bool InfoMap::AreNotificationsDisabled(
    const std::string& extension_id) const {
  auto iter = extra_data_.find(extension_id);
  if (iter != extra_data_.end())
    return iter->second.notifications_disabled;
  return false;
}

void InfoMap::SetContentVerifier(ContentVerifier* verifier) {
  content_verifier_ = verifier;
}

void InfoMap::SetIsLockScreenContext(bool is_lock_screen_context) {
  process_map_.set_is_lock_screen_context(is_lock_screen_context);
}

InfoMap::~InfoMap() {
  if (quota_service_) {
    BrowserThread::DeleteSoon(
        BrowserThread::IO, FROM_HERE, quota_service_.release());
  }
}

}  // namespace extensions
