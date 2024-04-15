// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/url_forwarder_configurator_win.h"

#include <windows.h>

#include <wtsapi32.h>

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/switches.h"
#include "remoting/host/win/wts_session_change_observer.h"

namespace remoting {

namespace {

// If the user has already changed the default browser to the URL forwarder,
// then the setup process will finish immediately without showing any UI, so we
// delay reporting USER_INTERVENTION_REQUIRED so that the client doesn't pop up
// a toast when it's unnecessary.
constexpr base::TimeDelta kReportUserInterventionRequiredDelay =
    base::Milliseconds(500);

base::win::ScopedHandle GetCurrentUserToken() {
  HANDLE user_token = nullptr;
  if (!WTSQueryUserToken(WTS_CURRENT_SESSION, &user_token)) {
    PLOG(ERROR) << "Failed to get current user token";
    return base::win::ScopedHandle();
  }
  return base::win::ScopedHandle(user_token);
}

// If |switch_name| is empty, the process will be launched with no extra
// switches.
bool LaunchConfiguratorProcess(const std::string& switch_name,
                               base::win::ScopedHandle user_token) {
  DCHECK(user_token.IsValid());
  base::LaunchOptions launch_options;
  launch_options.as_user = user_token.Get();
  // The remoting_desktop.exe binary (where this code runs) has extra manifest
  // flags (uiAccess and requireAdministrator) that are undesirable for the
  // url_forwarder_configurator child process, so remoting_host.exe is used
  // instead.
  base::FilePath path;
  if (!base::PathService::Get(base::DIR_EXE, &path)) {
    LOG(ERROR) << "Failed to get executable path.";
    return false;
  }
  path = path.AppendASCII("remoting_host.exe");
  base::CommandLine command(path);
  command.AppendSwitchASCII(kProcessTypeSwitchName,
                            kProcessTypeUrlForwarderConfigurator);
  if (!switch_name.empty()) {
    command.AppendSwitch(switch_name);
  }
  int exit_code = -1;
  base::LaunchProcess(command, launch_options).WaitForExit(&exit_code);
  return exit_code == 0;
}

}  // namespace

UrlForwarderConfiguratorWin::UrlForwarderConfiguratorWin()
    : io_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

UrlForwarderConfiguratorWin::~UrlForwarderConfiguratorWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UrlForwarderConfiguratorWin::IsUrlForwarderSetUp(
    IsUrlForwarderSetUpCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto user_token = GetCurrentUserToken();
  if (user_token.IsValid()) {
    io_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LaunchConfiguratorProcess, std::string(),
                       std::move(user_token)),
        std::move(callback));
    return;
  }

  if (GetLastError() != ERROR_NO_TOKEN) {
    std::move(callback).Run(false);
    return;
  }

  if (is_url_forwarder_set_up_callback_) {
    LOG(ERROR) << "Query is already in progress.";
    std::move(callback).Run(false);
    return;
  }
  DCHECK(!wts_session_change_observer_);

  HOST_LOG << "Can't determine URL Forwarder state as no user is signed in. "
           << "Will check again after user signs in.";
  is_url_forwarder_set_up_callback_ = std::move(callback);
  wts_session_change_observer_ = std::make_unique<WtsSessionChangeObserver>();
  bool start_success = wts_session_change_observer_->Start(
      base::BindRepeating(&UrlForwarderConfiguratorWin::OnWtsSessionChange,
                          base::Unretained(this)));
  if (!start_success) {
    std::move(is_url_forwarder_set_up_callback_).Run(false);
  }
}

void UrlForwarderConfiguratorWin::SetUpUrlForwarder(
    const SetUpUrlForwarderCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (set_up_url_forwarder_callback_) {
    LOG(ERROR) << "Setup is already in progress.";
    callback.Run(SetUpUrlForwarderResponse::FAILED);
    return;
  }

  set_up_url_forwarder_callback_ = callback;
  report_user_intervention_required_timer_.Start(
      FROM_HERE, kReportUserInterventionRequiredDelay, this,
      &UrlForwarderConfiguratorWin::OnReportUserInterventionRequired);
  auto user_token = GetCurrentUserToken();
  if (!user_token.IsValid()) {
    OnSetUpResponse(false);
    return;
  }
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LaunchConfiguratorProcess, kSetUpUrlForwarderSwitchName,
                     std::move(user_token)),
      base::BindOnce(&UrlForwarderConfiguratorWin::OnSetUpResponse,
                     weak_factory_.GetWeakPtr()));
}

void UrlForwarderConfiguratorWin::OnSetUpResponse(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  report_user_intervention_required_timer_.AbandonAndStop();
  set_up_url_forwarder_callback_.Run(success
                                         ? SetUpUrlForwarderResponse::COMPLETE
                                         : SetUpUrlForwarderResponse::FAILED);
  set_up_url_forwarder_callback_.Reset();
}

void UrlForwarderConfiguratorWin::OnReportUserInterventionRequired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  set_up_url_forwarder_callback_.Run(
      SetUpUrlForwarderResponse::USER_INTERVENTION_REQUIRED);
}

void UrlForwarderConfiguratorWin::OnWtsSessionChange(uint32_t event,
                                                     uint32_t session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (event != WTS_SESSION_LOGON) {
    return;
  }

  DCHECK(is_url_forwarder_set_up_callback_);

  HOST_LOG << "User logged in. Checking URL forwarder setup state now.";
  auto user_token = GetCurrentUserToken();
  if (!user_token.IsValid()) {
    std::move(is_url_forwarder_set_up_callback_).Run(false);
    return;
  }
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LaunchConfiguratorProcess, std::string(),
                     std::move(user_token)),
      std::move(is_url_forwarder_set_up_callback_));
  wts_session_change_observer_.reset();
}

// static
std::unique_ptr<UrlForwarderConfigurator> UrlForwarderConfigurator::Create() {
  return std::make_unique<UrlForwarderConfiguratorWin>();
}

}  // namespace remoting
