// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/lazy_event_dispatch_util.h"

#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

namespace {

// Previously installed version number.
const char kPrefPreviousVersion[] = "previous_version";

// A preference key storing the information about an extension that was
// installed but not loaded. We keep the pending info here so that we can send
// chrome.runtime.onInstalled event during the extension load.
const char kPrefPendingOnInstalledEventDispatchInfo[] =
    "pending_on_installed_event_dispatch_info";

}  // namespace

LazyEventDispatchUtil::LazyEventDispatchUtil(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

LazyEventDispatchUtil::~LazyEventDispatchUtil() {}

void LazyEventDispatchUtil::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LazyEventDispatchUtil::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void LazyEventDispatchUtil::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  base::Version previous_version;
  if (ReadPendingOnInstallInfoFromPref(extension->id(), &previous_version)) {
    for (auto& observer : observers_) {
      observer.OnExtensionInstalledAndLoaded(browser_context_, extension,
                                             previous_version);
    }
    RemovePendingOnInstallInfoFromPref(extension->id());
  }
}

void LazyEventDispatchUtil::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UninstallReason reason) {
  RemovePendingOnInstallInfoFromPref(extension->id());
}

void LazyEventDispatchUtil::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  StorePendingOnInstallInfoToPref(extension);
}

bool LazyEventDispatchUtil::ReadPendingOnInstallInfoFromPref(
    const ExtensionId& extension_id,
    base::Version* previous_version) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  DCHECK(prefs);

  const base::DictionaryValue* info = nullptr;
  if (!prefs->ReadPrefAsDictionary(
          extension_id, kPrefPendingOnInstalledEventDispatchInfo, &info)) {
    return false;
  }

  std::string previous_version_string;
  info->GetString(kPrefPreviousVersion, &previous_version_string);
  // |previous_version_string| can be empty.
  *previous_version = base::Version(previous_version_string);
  return true;
}

void LazyEventDispatchUtil::RemovePendingOnInstallInfoFromPref(
    const ExtensionId& extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  DCHECK(prefs);

  prefs->UpdateExtensionPref(extension_id,
                             kPrefPendingOnInstalledEventDispatchInfo, nullptr);
}

void LazyEventDispatchUtil::StorePendingOnInstallInfoToPref(
    const Extension* extension) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
  DCHECK(prefs);

  // |pending_on_install_info| currently only contains a version string. Instead
  // of making the pref hold a plain string, we store it as a dictionary value
  // so that we can add more stuff to it in the future if necessary.
  auto pending_on_install_info = std::make_unique<base::DictionaryValue>();
  base::Version previous_version = ExtensionRegistry::Get(browser_context_)
                                       ->GetStoredVersion(extension->id());
  pending_on_install_info->SetString(kPrefPreviousVersion,
                                     previous_version.IsValid()
                                         ? previous_version.GetString()
                                         : std::string());
  prefs->UpdateExtensionPref(extension->id(),
                             kPrefPendingOnInstalledEventDispatchInfo,
                             std::move(pending_on_install_info));
}

}  // namespace extensions
