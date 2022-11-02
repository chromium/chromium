// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/runtime/shell_runtime_api_delegate.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/api/runtime.h"
#include "extensions/shell/browser/shell_extension_system.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/power/power_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

using extensions::api::runtime::PlatformInfo;

namespace extensions {

ShellRuntimeAPIDelegate::ShellRuntimeAPIDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

ShellRuntimeAPIDelegate::~ShellRuntimeAPIDelegate() = default;

void ShellRuntimeAPIDelegate::AddUpdateObserver(UpdateObserver* observer) {}

void ShellRuntimeAPIDelegate::RemoveUpdateObserver(UpdateObserver* observer) {}

void ShellRuntimeAPIDelegate::ReloadExtension(const std::string& extension_id) {
  static_cast<ShellExtensionSystem*>(ExtensionSystem::Get(browser_context_))
      ->ReloadExtension(extension_id);
}

bool ShellRuntimeAPIDelegate::CheckForUpdates(const std::string& extension_id,
                                              UpdateCheckCallback callback) {
  return false;
}

void ShellRuntimeAPIDelegate::OpenURL(const GURL& uninstall_url) {}

bool ShellRuntimeAPIDelegate::GetPlatformInfo(PlatformInfo* info) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  info->os = api::runtime::PLATFORM_OS_CROS;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  info->os = api::runtime::PLATFORM_OS_LINUX;
#endif
  return true;
}

bool ShellRuntimeAPIDelegate::RestartDevice(std::string* error_message) {
// We allow chrome.runtime.restart() to request a device restart on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_API, "AppShell chrome.runtime API");
  return true;
#else
  *error_message = "Restart is only supported on ChromeOS.";
  return false;
#endif
}

}  // namespace extensions
