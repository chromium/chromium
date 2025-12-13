// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/polling_desktop_display_info_monitor.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_loader.h"

namespace remoting {

namespace {

// Polling interval for querying the OS for changes to the multi-monitor
// configuration. Before this class was written, the DesktopCapturerProxy would
// poll after each captured frame, which could occur up to 30x per second. The
// value chosen here is slower than this (to reduce the load on the OS), but
// still fast enough to be responsive to any changes.
constexpr base::TimeDelta kPollingInterval = base::Milliseconds(100);

}  // namespace

PollingDesktopDisplayInfoMonitor::PollingDesktopDisplayInfoMonitor(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    std::unique_ptr<DesktopDisplayInfoLoader> info_loader)
    : ui_task_runner_(ui_task_runner),
      desktop_display_info_loader_(std::move(info_loader)) {
  // The loader must be initialized and used on the UI thread (though it can be
  // created on any thread).
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DesktopDisplayInfoLoader::Init,
                     base::Unretained(desktop_display_info_loader_.get())));
}

PollingDesktopDisplayInfoMonitor::~PollingDesktopDisplayInfoMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ui_task_runner_->DeleteSoon(FROM_HERE,
                              desktop_display_info_loader_.release());
}

void PollingDesktopDisplayInfoMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsStarted()) {
    timer_.Start(FROM_HERE, kPollingInterval, this,
                 &PollingDesktopDisplayInfoMonitor::QueryDisplayInfo);
  }
}

bool PollingDesktopDisplayInfoMonitor::IsStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return timer_.IsRunning();
}

const DesktopDisplayInfo*
PollingDesktopDisplayInfoMonitor::GetLatestDisplayInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return desktop_display_info_ ? &desktop_display_info_.value() : nullptr;
}

void PollingDesktopDisplayInfoMonitor::AddCallback(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_list_.AddUnsafe(std::move(callback));
}

base::WeakPtr<PollingDesktopDisplayInfoMonitor>
PollingDesktopDisplayInfoMonitor::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PollingDesktopDisplayInfoMonitor::QueryDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_list_.empty()) {
    ui_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&DesktopDisplayInfoLoader::GetCurrentDisplayInfo,
                       base::Unretained(desktop_display_info_loader_.get())),
        base::BindOnce(&PollingDesktopDisplayInfoMonitor::OnDisplayInfoLoaded,
                       weak_factory_.GetWeakPtr()));
  }
}

void PollingDesktopDisplayInfoMonitor::OnDisplayInfoLoaded(
    DesktopDisplayInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (callback_list_.empty() || desktop_display_info_ == info) {
    return;
  }

  desktop_display_info_ = std::move(info);
  callback_list_.Notify();
}

}  // namespace remoting
