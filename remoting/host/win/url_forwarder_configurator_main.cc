// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <cwchar>
#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_types.h"
#include "remoting/base/logging.h"
#include "remoting/host/switches.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/simple_task_dialog.h"

namespace remoting {

namespace {

#if defined(OFFICIAL_BUILD)
constexpr wchar_t kUrlForwarderProgId[] = L"CrdUrlForwarder";
#else
constexpr wchar_t kUrlForwarderProgId[] = L"ChromotingUrlForwarder";
#endif

constexpr wchar_t kProtocolToTestSetup[] = L"http";

constexpr base::TimeDelta kPollingInterval =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kPollingTimeout = base::TimeDelta::FromMinutes(1);

// |log_current_default_app| logs the current default app if it is not the CRD
//     URL forwarder.
bool IsUrlForwarderSetUp(bool log_current_default_app = false) {
  // This method is modified from chrome/installer/util/shell_util.cc

  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr =
      ::CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                         CLSCTX_INPROC, IID_PPV_ARGS(&registration));
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create IApplicationAssociationRegistration";
    return false;
  }
  base::win::ScopedCoMem<wchar_t> current_app;
  hr = registration->QueryCurrentDefault(kProtocolToTestSetup, AT_URLPROTOCOL,
                                         AL_EFFECTIVE, &current_app);
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to query default app for protocol "
                << kProtocolToTestSetup;
    return false;
  }
  if (std::wcscmp(kUrlForwarderProgId, current_app) != 0) {
    if (log_current_default_app) {
      HOST_LOG << "Current default app for " << kProtocolToTestSetup << " is "
               << current_app << " instead of " << kUrlForwarderProgId;
    }
    return false;
  }
  return true;
}

// Launches the Windows 'settings' modern app with the 'default apps' view
// focused. This only works for Windows 8 and Windows 10. The appModelId
// looks arbitrary but it is the same in Win8 and Win10. There is no easy way to
// retrieve the appModelId from the registry.
// Returns a boolean indicating whether the default apps view is successfully
// launched.
bool LaunchDefaultAppsSettingsModernDialog() {
  // This method is modified from chrome/installer/util/shell_util.cc

  static const wchar_t kControlPanelAppModelId[] =
      L"windows.immersivecontrolpanel_cw5n1h2txyewy"
      L"!microsoft.windows.immersivecontrolpanel";

  Microsoft::WRL::ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&activator));
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create IApplicationActivationManager";
    return false;
  }
  DWORD pid = 0;
  CoAllowSetForegroundWindow(activator.Get(), nullptr);
  hr = activator->ActivateApplication(
      kControlPanelAppModelId, L"page=SettingsPageAppsDefaults", AO_NONE, &pid);
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to activate application";
    return false;
  }
  return true;
}

bool ShowSetUpUrlForwarderDialog() {
  // |resource_module| does not need to be freed as GetModuleHandle() does not
  // increment the refcount for the module.  This DLL is not unloaded until the
  // process exits so using a stored handle is safe.
  HMODULE resource_module = GetModuleHandle(L"remoting_core.dll");
  if (resource_module == nullptr) {
    PLOG(ERROR) << "GetModuleHandle() failed";
    return false;
  }

  SimpleTaskDialog task_dialog(resource_module);
  if (!task_dialog.SetTitleTextWithStringId(IDS_URL_FORWARDER_NAME) ||
      !task_dialog.SetMessageTextWithStringId(
          IDS_SET_UP_URL_FORWARDER_MESSAGE) ||
      !task_dialog.AppendButtonWithStringId(
          IDOK, IDS_OPEN_DEFAULT_APPS_SETTINGS_BUTTON) ||
      !task_dialog.AppendButtonWithStringId(IDCANCEL, IDS_CANCEL)) {
    LOG(ERROR) << "Failed to load text for the setup dialog.";
    return false;
  }
  task_dialog.set_default_button(IDOK);

  absl::optional<int> button_result = task_dialog.Show();
  if (!button_result.has_value()) {
    LOG(ERROR) << "Failed to show the setup dialog.";
    return false;
  }
  switch (*button_result) {
    case IDOK:
      return true;
    case IDCANCEL:
      return false;
    default:
      NOTREACHED() << "Unknown button: " << *button_result;
      return false;
  }
}

// Class for running the setup process.
class SetUpProcess {
 public:
  SetUpProcess();
  ~SetUpProcess();

  // Starts the setup process and calls |done_callback| once done.
  void Start(base::OnceCallback<void(bool)> done_callback);

 private:
  void OnSetUpDialogContinue();
  void OnSetUpDialogCancel();

  void PollUrlForwarderSetupState();

  base::OnceCallback<void(bool)> done_callback_;
  base::TimeDelta total_poll_time_;
};

SetUpProcess::SetUpProcess() = default;

SetUpProcess::~SetUpProcess() {
  DCHECK(!done_callback_);
}

void SetUpProcess::Start(base::OnceCallback<void(bool)> done_callback) {
  DCHECK(!done_callback_);

  done_callback_ = std::move(done_callback);
  if (IsUrlForwarderSetUp()) {
    HOST_LOG << "URL forwarder has already been set up.";
    std::move(done_callback_).Run(true);
    return;
  }

  if (ShowSetUpUrlForwarderDialog()) {
    OnSetUpDialogContinue();
  } else {
    OnSetUpDialogCancel();
  }
}

void SetUpProcess::OnSetUpDialogContinue() {
  HOST_LOG << "Launching default apps settings dialog...";
  if (!LaunchDefaultAppsSettingsModernDialog()) {
    std::move(done_callback_).Run(false);
    return;
  }

  DCHECK(total_poll_time_.is_zero());
  HOST_LOG << "Polling default app for protocol " << kProtocolToTestSetup
           << "...";
  PollUrlForwarderSetupState();
}

void SetUpProcess::OnSetUpDialogCancel() {
  HOST_LOG << "User canceled the setup process";
  std::move(done_callback_).Run(false);
}

void SetUpProcess::PollUrlForwarderSetupState() {
  if (IsUrlForwarderSetUp()) {
    std::move(done_callback_).Run(true);
    return;
  }
  if (total_poll_time_ >= kPollingTimeout) {
    LOG(ERROR)
        << "Timed out waiting for the URL forwarder to become the default app";
    std::move(done_callback_).Run(false);
    return;
  }
  total_poll_time_ += kPollingInterval;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SetUpProcess::PollUrlForwarderSetupState,
                     base::Unretained(this)),
      kPollingInterval);
}

}  // namespace

int UrlForwarderConfiguratorMain() {
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "UrlForwarderConfigurator");
  base::SingleThreadTaskExecutor task_executor(base::MessagePumpType::UI);
  base::win::ScopedCOMInitializer com;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSetUpUrlForwarderSwitchName)) {
    base::RunLoop run_loop;
    SetUpProcess set_up_process;
    bool success = false;
    set_up_process.Start(base::BindOnce(
        [](base::OnceClosure quit_closure, bool& out_success, bool success) {
          out_success = success;
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), std::ref(success)));
    run_loop.Run();
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // The default action is to check if the URL forwarder has been properly set
  // up.
  return IsUrlForwarderSetUp(/* log_current_default_app= */ true)
             ? EXIT_SUCCESS
             : EXIT_FAILURE;
}

}  // namespace remoting
