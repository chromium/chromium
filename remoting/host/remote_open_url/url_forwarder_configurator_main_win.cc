// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <shobjidl.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <cwchar>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/time/time.h"
#include "base/win/default_apps_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_types.h"
#include "remoting/base/logging.h"
#include "remoting/base/user_settings.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/remote_open_url/remote_open_url_constants.h"
#include "remoting/host/user_setting_keys.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/simple_task_dialog.h"

namespace remoting {

namespace {

constexpr wchar_t kProtocolToTestSetup[] = L"http";

constexpr base::TimeDelta kPollingInterval = base::Milliseconds(500);
constexpr base::TimeDelta kPollingTimeout = base::Minutes(1);

// Returns the current default browser's ProgID, or an empty string if failed.
std::wstring GetDefaultBrowserProgId() {
  // This method is modified from chrome/installer/util/shell_util.cc

  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr =
      ::CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                         CLSCTX_INPROC, IID_PPV_ARGS(&registration));
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to create IApplicationAssociationRegistration";
    return std::wstring();
  }
  base::win::ScopedCoMem<wchar_t> current_app;
  hr = registration->QueryCurrentDefault(kProtocolToTestSetup, AT_URLPROTOCOL,
                                         AL_EFFECTIVE, &current_app);
  if (FAILED(hr)) {
    PLOG(ERROR) << "Failed to query default app for protocol "
                << kProtocolToTestSetup;
    return std::wstring();
  }
  return current_app.get();
}

// |log_current_default_app| logs the current default app if it is not the CRD
// URL forwarder.
bool IsUrlForwarderSetUp(bool log_current_default_app = false) {
  std::wstring current_app = GetDefaultBrowserProgId();
  if (current_app.empty()) {
    return false;
  }
  if (current_app != kUrlForwarderProgId) {
    if (log_current_default_app) {
      HOST_LOG << "Current default app for " << kProtocolToTestSetup << " is "
               << current_app << " instead of " << kUrlForwarderProgId;
    }
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

  std::optional<int> button_result = task_dialog.Show();
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

  std::wstring prog_id = GetDefaultBrowserProgId();
  LOG(INFO) << "Setting previous default browser to " << prog_id;

  UserSettings::GetInstance()->SetString(kWinPreviousDefaultWebBrowserProgId,
                                         base::WideToUTF8(prog_id));

  if (ShowSetUpUrlForwarderDialog()) {
    OnSetUpDialogContinue();
  } else {
    OnSetUpDialogCancel();
  }
}

void SetUpProcess::OnSetUpDialogContinue() {
  // Windows does not pick up changes in RegisteredApplications until
  // SHChangeNotify() is called, so we call it here in case the user has just
  // added the URL forwarder entry to the registry.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

  HOST_LOG << "Launching default apps settings dialog";
  if (!base::win::LaunchDefaultAppsSettingsModernDialog(
          /*protocol=*/std::wstring())) {
    std::move(done_callback_).Run(false);
    return;
  }

  DCHECK(total_poll_time_.is_zero());
  HOST_LOG << "Polling default app for protocol " << kProtocolToTestSetup;
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
