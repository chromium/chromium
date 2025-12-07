// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/delegating_desktop_display_info_monitor.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/desktop_display_info.h"

namespace remoting {

DelegatingDesktopDisplayInfoMonitor::DelegatingDesktopDisplayInfoMonitor(
    base::WeakPtr<DesktopDisplayInfoMonitor> underlying)
    : underlying_(underlying) {}

DelegatingDesktopDisplayInfoMonitor::~DelegatingDesktopDisplayInfoMonitor() =
    default;

void DelegatingDesktopDisplayInfoMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (started_ || !underlying_) {
    return;
  }

  started_ = true;
  underlying_->AddCallback(base::BindRepeating(
      &DelegatingDesktopDisplayInfoMonitor::OnUnderlyingDisplayInfoChanged,
      base::Unretained(this)));
  if (underlying_->IsStarted()) {
    if (underlying_->GetLatestDisplayInfo()) {
      // Notify that the display info is available on the next task frame to
      // prevent reentrancy issues.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&DelegatingDesktopDisplayInfoMonitor::
                                        OnUnderlyingDisplayInfoChanged,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    underlying_->Start();
  }
}

bool DelegatingDesktopDisplayInfoMonitor::IsStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return started_;
}

const DesktopDisplayInfo*
DelegatingDesktopDisplayInfoMonitor::GetLatestDisplayInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_ || !underlying_) {
    return nullptr;
  }
  return underlying_->GetLatestDisplayInfo();
}

void DelegatingDesktopDisplayInfoMonitor::AddCallback(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.AddUnsafe(std::move(callback));
}

void DelegatingDesktopDisplayInfoMonitor::OnUnderlyingDisplayInfoChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(started_);
  callbacks_.Notify();
}

}  // namespace remoting
