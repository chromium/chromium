// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/command_line.h"
#include "components/os_crypt/key_storage_config_linux.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "headless/app/headless_shell_switches.h"
#endif

namespace headless {

#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kProductName[] = "HeadlessChrome";
#endif

void HeadlessBrowserMainParts::PostCreateMainMessageLoop() {
#if defined(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS_ASH)
  bluez::BluezDBusManager::Initialize(/*system_bus=*/nullptr);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // Set up crypt config. This needs to be done before anything starts the
  // network service, as the raw encryption key needs to be shared with the
  // network service for encrypted cookie storage.
  std::unique_ptr<os_crypt::Config> config =
      std::make_unique<os_crypt::Config>();
  // Forward to os_crypt the flag to use a specific password store.
  config->store = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kPasswordStore);
  // Use a default product name
  config->product_name = kProductName;
  // OSCrypt may target keyring, which requires calls from the main thread.
  config->main_thread_runner = content::GetUIThreadTaskRunner({});
  // OSCrypt can be disabled in a special settings file, but headless doesn't
  // need to support that.
  config->should_use_preference = false;
  config->user_data_path = base::FilePath();
  OSCrypt::SetConfig(std::move(config));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace headless
