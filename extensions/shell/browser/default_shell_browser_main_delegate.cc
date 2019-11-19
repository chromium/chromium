// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/default_shell_browser_main_delegate.h"

#include "apps/launcher.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_tokenizer.h"
#include "build/build_config.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/shell_extension_system.h"

#if defined(USE_AURA)
#include "extensions/shell/browser/shell_desktop_controller_aura.h"
#endif

#if defined(OS_MACOSX)
#include "extensions/shell/browser/shell_desktop_controller_mac.h"
#endif

namespace extensions {

namespace {

void LoadExtensionsFromCommandLine(ShellExtensionSystem* extension_system) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kLoadExtension))
    return;

  base::CommandLine::StringType path_list =
      command_line->GetSwitchValueNative(switches::kLoadExtension);

  base::StringTokenizerT<base::CommandLine::StringType,
                         base::CommandLine::StringType::const_iterator>
      tokenizer(path_list, FILE_PATH_LITERAL(","));
  while (tokenizer.GetNext()) {
    extension_system->LoadExtension(
        base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token())));
  }
}

void LoadAppsFromCommandLine(ShellExtensionSystem* extension_system,
                             content::BrowserContext* browser_context) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kLoadApps)) {
    LOG(ERROR) << "No app specified. Use --" << switches::kLoadApps
               << " to load and launch an app.";
    return;
  }

  base::CommandLine::StringType path_list =
      command_line->GetSwitchValueNative(switches::kLoadApps);

  base::StringTokenizerT<base::CommandLine::StringType,
                         base::CommandLine::StringType::const_iterator>
      tokenizer(path_list, FILE_PATH_LITERAL(","));

  const Extension* launch_app = nullptr;
  while (tokenizer.GetNext()) {
    base::FilePath app_absolute_dir =
        base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));

    const Extension* extension = extension_system->LoadApp(app_absolute_dir);
    if (extension && !launch_app)
      launch_app = extension;
  }

  if (launch_app) {
    base::FilePath current_directory;
    base::PathService::Get(base::DIR_CURRENT, &current_directory);
    apps::LaunchPlatformAppWithCommandLine(browser_context, launch_app,
                                           *command_line, current_directory,
                                           AppLaunchSource::kSourceCommandLine);
  } else {
    LOG(ERROR) << "Could not load any apps.";
  }
}

}  // namespace

DefaultShellBrowserMainDelegate::DefaultShellBrowserMainDelegate() {
}

DefaultShellBrowserMainDelegate::~DefaultShellBrowserMainDelegate() {
}

void DefaultShellBrowserMainDelegate::Start(
    content::BrowserContext* browser_context) {
  ShellExtensionSystem* extension_system =
      static_cast<ShellExtensionSystem*>(ExtensionSystem::Get(browser_context));
  extension_system->FinishInitialization();

  LoadExtensionsFromCommandLine(extension_system);
  LoadAppsFromCommandLine(extension_system, browser_context);
}

void DefaultShellBrowserMainDelegate::Shutdown() {
}

DesktopController* DefaultShellBrowserMainDelegate::CreateDesktopController(
    content::BrowserContext* context) {
#if defined(USE_AURA)
  return new ShellDesktopControllerAura(context);
#elif defined(OS_MACOSX)
  return new ShellDesktopControllerMac();
#else
  return NULL;
#endif
}

}  // namespace extensions
