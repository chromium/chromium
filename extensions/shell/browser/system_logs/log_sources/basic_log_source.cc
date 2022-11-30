// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/system_logs/log_sources/basic_log_source.h"

#include <memory>
#include <string>

#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace system_logs {

namespace {

constexpr char kAppShellVersionTag[] = "APPSHELL VERSION";
constexpr char kOsVersionTag[] = "OS VERSION";
constexpr char kExtensionsListKey[] = "extensions";

}  // namespace

BasicLogSource::BasicLogSource(content::BrowserContext* browser_context)
    : SystemLogsSource("AppShellBasic"), browser_context_(browser_context) {}

BasicLogSource::~BasicLogSource() = default;

void BasicLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();

  PopulateVersionStrings(response.get());
  PopulateExtensionInfoLogs(response.get());

  std::move(callback).Run(std::move(response));
}

void BasicLogSource::PopulateVersionStrings(SystemLogsResponse* response) {
  response->emplace(kAppShellVersionTag, version_info::GetVersionNumber());
  std::string os_version = base::SysInfo::OperatingSystemName() + ": " +
                           base::SysInfo::OperatingSystemVersion();
  response->emplace(kOsVersionTag, os_version);
}

void BasicLogSource::PopulateExtensionInfoLogs(SystemLogsResponse* response) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context_);
  std::string extensions_list;
  for (const auto& extension : extension_registry->enabled_extensions()) {
    // Format: <extension_id> : <extension_name> : <extension_version>
    // Work around the anonymizer tool recognizing some versions as IPv4s by
    // replacing dots "." with underscores "_".
    std::string version;
    base::ReplaceChars(extension->VersionString(), ".", "_", &version);
    extensions_list += extension->id() + " : " + extension->name() +
                       " : version " + version + "\n";
  }
  response->emplace(kExtensionsListKey, extensions_list);
}

}  // namespace system_logs
