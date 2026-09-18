// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/desktop_session_factory_linux.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/daemon_process.h"
#include "remoting/host/desktop_session.h"
#include "remoting/host/linux/gdm_managed_desktop_session_backend.h"
#include "remoting/host/mojom/desktop_session.mojom.h"

namespace remoting {

DesktopSessionFactoryLinux::DesktopSessionFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : io_task_runner_(io_task_runner) {}

DesktopSessionFactoryLinux::~DesktopSessionFactoryLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DesktopSessionFactoryLinux::Start(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_);

  backend_ = std::make_unique<GdmManagedDesktopSessionBackend>(io_task_runner_);
  backend_->Start(std::move(callback));
}

std::unique_ptr<DesktopSession>
DesktopSessionFactoryLinux::CreateDesktopSession(
    int id,
    DaemonProcess* daemon_process,
    const mojom::DesktopSessionOptions& options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);

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
