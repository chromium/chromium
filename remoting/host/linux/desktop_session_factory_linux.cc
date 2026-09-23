// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/desktop_session_factory_linux.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "remoting/base/logging.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/gdm_managed_desktop_session_backend.h"
#include "remoting/host/linux/session_routing_config.h"
#include "remoting/host/linux/user_desktop_session_backend.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

namespace remoting {

DesktopSessionFactoryLinux::DesktopSessionFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner) {}

DesktopSessionFactoryLinux::~DesktopSessionFactoryLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DesktopSessionFactoryLinux::Start(bool is_corp_host, Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SessionRoutingConfig::LoadAndValidate,
                     SessionRoutingConfig::GetDefaultConfigFilePath(),
                     is_corp_host, /*expected_owner_uid=*/0),
      base::BindOnce(&DesktopSessionFactoryLinux::OnSessionRoutingConfigLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DesktopSessionFactoryLinux::OnSessionRoutingConfigLoaded(
    Callback callback,
    base::expected<std::optional<SessionRoutingConfig>, Loggable> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to load session routing configuration: "
               << result.error();
    std::move(callback).Run(base::unexpected(std::move(result).error()));
    return;
  }

  if (result->has_value()) {
    HOST_LOG << "Using UserDesktopSessionBackend.";
    backend_ = std::make_unique<UserDesktopSessionBackend>(io_task_runner_,
                                                           std::move(**result));
  } else {
    HOST_LOG << "Using GdmManagedDesktopSessionBackend.";
    backend_ =
        std::make_unique<GdmManagedDesktopSessionBackend>(io_task_runner_);
  }

  backend_->Start(std::move(callback));
}

std::unique_ptr<DesktopSession>
DesktopSessionFactoryLinux::CreateDesktopSession(
    int id,
    DaemonProcess* daemon_process,
    const mojom::DesktopSessionOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    LOG(ERROR) << "Desktop session backend is not initialized.";
    return nullptr;
  }

  return backend_->CreateDesktopSession(id, daemon_process, options);
}

void DesktopSessionFactoryLinux::TerminateAllSessions(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    std::move(callback).Run(base::ok());
    return;
  }

  backend_->TerminateAllSessions(std::move(callback));
}

DesktopSession* DesktopSessionFactoryLinux::GetSessionByUid(uid_t uid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_) {
    return nullptr;
  }

  return backend_->GetSessionByUid(uid);
}

}  // namespace remoting
