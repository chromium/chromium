// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_browser_main_parts.h"

#include <string>

#include "apps/browser_context_keyed_service_factories.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/nacl/common/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "extensions/browser/browser_context_keyed_service_factories.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/updater/update_service.h"
#include "extensions/common/constants.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_browser_context.h"
#include "extensions/shell/browser/shell_browser_context_keyed_service_factories.h"
#include "extensions/shell/browser/shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/browser/shell_extension_system_factory.h"
#include "extensions/shell/browser/shell_extensions_browser_client.h"
#include "extensions/shell/browser/shell_prefs.h"
#include "extensions/shell/browser/shell_update_query_params_delegate.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "extensions/shell/common/switches.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "chromeos/network/network_handler.h"
#include "extensions/shell/browser/shell_audio_controller_chromeos.h"
#include "extensions/shell/browser/shell_network_controller_chromeos.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/nacl_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/shell/browser/shell_nacl_browser_delegate.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif

using base::CommandLine;
using content::BrowserContext;

#if BUILDFLAG(ENABLE_NACL)
#endif

namespace extensions {

namespace {

// Intentionally dereferences a null pointer to test the crash reporter.
void CrashForTest() {
  int* bad_pointer = nullptr;
  *bad_pointer = 0;
}

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters,
    ShellBrowserMainDelegate* browser_main_delegate)
    : extension_system_(nullptr),
      parameters_(parameters),
      run_message_loop_(true),
      browser_main_delegate_(browser_main_delegate) {
}

ShellBrowserMainParts::~ShellBrowserMainParts() {
}

void ShellBrowserMainParts::PreMainMessageLoopStart() {
#if defined(USE_AURA) && defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
#if defined(OS_CHROMEOS)
  // Perform initialization of D-Bus objects here rather than in the below
  // helper classes so those classes' tests can initialize stub versions of the
  // D-Bus objects.
  chromeos::DBusThreadManager::Initialize();
  dbus::Bus* bus = chromeos::DBusThreadManager::Get()->GetSystemBus();
  if (bus) {
    bluez::BluezDBusManager::Initialize(bus);
    chromeos::CrasAudioClient::Initialize(bus);
    chromeos::PowerManagerClient::Initialize(bus);
  } else {
    bluez::BluezDBusManager::InitializeFake();
    chromeos::CrasAudioClient::InitializeFake();
    chromeos::PowerManagerClient::InitializeFake();
  }

  chromeos::disks::DiskMountManager::Initialize();

  chromeos::NetworkHandler::Initialize();
  network_controller_.reset(new ShellNetworkController(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kAppShellPreferredNetwork)));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAppShellAllowRoaming)) {
    network_controller_->SetCellularAllowRoaming(true);
  }
#elif defined(OS_LINUX)
  // app_shell doesn't need GTK, so the fake input method context can work.
  // See crbug.com/381852 and revision fb69f142.
  // TODO(michaelpg): Verify this works for target environments.
  ui::InitializeInputMethodForTesting();

  bluez::BluezDBusManager::Initialize(nullptr /* system_bus */);
#else
  ui::InitializeInputMethodForTesting();
#endif
}

int ShellBrowserMainParts::PreEarlyInitialization() {
  return content::RESULT_CODE_NORMAL_EXIT;
}

int ShellBrowserMainParts::PreCreateThreads() {
  // TODO(jamescook): Initialize chromeos::CrosSettings here?

  content::ChildProcessSecurityPolicy::GetInstance()->RegisterWebSafeScheme(
      kExtensionScheme);

  // Return no error.
  return 0;
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  extensions_client_ = std::make_unique<ShellExtensionsClient>();
  ExtensionsClient::Set(extensions_client_.get());

  // BrowserContextKeyedAPIServiceFactories require an ExtensionsBrowserClient.
  extensions_browser_client_ = std::make_unique<ShellExtensionsBrowserClient>();
  ExtensionsBrowserClient::Set(extensions_browser_client_.get());

  apps::EnsureBrowserContextKeyedServiceFactoriesBuilt();
  EnsureBrowserContextKeyedServiceFactoriesBuilt();
  shell::EnsureBrowserContextKeyedServiceFactoriesBuilt();

  // Initialize our "profile" equivalent.
  browser_context_ = std::make_unique<ShellBrowserContext>();

  // app_shell only supports a single user, so all preferences live in the user
  // data directory, including the device-wide local state.
  local_state_ = shell_prefs::CreateLocalState(browser_context_->GetPath());
  sessions::SessionIdGenerator::GetInstance()->Init(local_state_.get());
  user_pref_service_ =
      shell_prefs::CreateUserPrefService(browser_context_.get());
  extensions_browser_client_->InitWithBrowserContext(browser_context_.get(),
                                                     user_pref_service_.get());

#if defined(OS_CHROMEOS)
  mojo::PendingRemote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  content::GetMediaSessionService().BindMediaControllerManager(
      media_controller_manager.InitWithNewPipeAndPassReceiver());
  chromeos::CrasAudioHandler::Initialize(
      std::move(media_controller_manager),
      base::MakeRefCounted<chromeos::AudioDevicesPrefHandlerImpl>(
          local_state_.get()));
  audio_controller_.reset(new ShellAudioController());
#endif

  // Create BrowserContextKeyedServices now that we have an
  // ExtensionsBrowserClient that BrowserContextKeyedAPIServices can query.
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      browser_context_.get());

#if defined(USE_AURA)
  aura::Env::GetInstance()->set_context_factory(content::GetContextFactory());
#endif

  storage_monitor::StorageMonitor::Create();

  desktop_controller_.reset(
      browser_main_delegate_->CreateDesktopController(browser_context_.get()));

  // TODO(jamescook): Initialize user_manager::UserManager.

  update_query_params_delegate_.reset(new ShellUpdateQueryParamsDelegate);
  update_client::UpdateQueryParams::SetDelegate(
      update_query_params_delegate_.get());

  InitExtensionSystem();

#if BUILDFLAG(ENABLE_NACL)
  nacl::NaClBrowser::SetDelegate(
      std::make_unique<ShellNaClBrowserDelegate>(browser_context_.get()));
  // Track the task so it can be canceled if app_shell shuts down very quickly,
  // such as in browser tests.
  task_tracker_.PostTask(content::GetIOThreadTaskRunner({}).get(), FROM_HERE,
                         base::BindOnce(nacl::NaClProcessHost::EarlyStartup));
#endif

  content::ShellDevToolsManagerDelegate::StartHttpHandler(
      browser_context_.get());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kBrowserCrashTest))
    CrashForTest();

  if (parameters_.ui_task) {
    // For running browser tests.
    std::move(*parameters_.ui_task).Run();
    delete parameters_.ui_task;
    run_message_loop_ = false;
  } else {
    browser_main_delegate_->Start(browser_context_.get());
  }
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  if (!run_message_loop_)
    return true;
  desktop_controller_->Run();
  *result_code = content::RESULT_CODE_NORMAL_EXIT;
  return true;
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  // Close apps before shutting down browser context and extensions system.
  desktop_controller_->CloseAppWindows();

  // NOTE: Please destroy objects in the reverse order of their creation.
  browser_main_delegate_->Shutdown();
  content::ShellDevToolsManagerDelegate::StopHttpHandler();

#if BUILDFLAG(ENABLE_NACL)
  task_tracker_.TryCancelAll();
#endif

  BrowserContextDependencyManager::GetInstance()->DestroyBrowserContextServices(
      browser_context_.get());
  extension_system_ = nullptr;

  desktop_controller_.reset();

  storage_monitor::StorageMonitor::Destroy();

#if defined(OS_CHROMEOS)
  audio_controller_.reset();
  chromeos::CrasAudioHandler::Shutdown();
#endif

  sessions::SessionIdGenerator::GetInstance()->Shutdown();

  user_pref_service_->CommitPendingWrite();
  user_pref_service_.reset();
  local_state_->CommitPendingWrite();
  local_state_.reset();

  browser_context_.reset();
}

void ShellBrowserMainParts::PostDestroyThreads() {
  extensions_browser_client_.reset();
  ExtensionsBrowserClient::Set(nullptr);

#if defined(OS_CHROMEOS)
  network_controller_.reset();
  chromeos::NetworkHandler::Shutdown();
  chromeos::disks::DiskMountManager::Shutdown();
  device::BluetoothAdapterFactory::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  chromeos::PowerManagerClient::Shutdown();
  chromeos::CrasAudioClient::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
#elif defined(OS_LINUX)
  device::BluetoothAdapterFactory::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  bluez::BluezDBusThreadManager::Shutdown();
#endif
}

void ShellBrowserMainParts::InitExtensionSystem() {
  DCHECK(browser_context_);
  extension_system_ = static_cast<ShellExtensionSystem*>(
      ExtensionSystem::Get(browser_context_.get()));
  extension_system_->InitForRegularProfile(true /* extensions_enabled */);
}

}  // namespace extensions
