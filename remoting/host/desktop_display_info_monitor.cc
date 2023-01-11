// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_monitor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

namespace {

// Polling interval for querying the OS for changes to the multi-monitor
// configuration. Before this class was written, the DesktopCapturerProxy would
// poll after each captured frame, which could occur up to 30x per second. The
// value chosen here is slower than this (to reduce the load on the OS), but
// still fast enough to be responsive to any changes.
constexpr base::TimeDelta kPollingInterval = base::Milliseconds(100);

}  // namespace

DesktopDisplayInfoMonitor::DesktopDisplayInfoMonitor(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      desktop_display_info_loader_(DesktopDisplayInfoLoader::Create()) {
  // The loader must be initialized and used on the UI thread (though it can be
  // created on any thread).
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DesktopDisplayInfoLoader::Init,
                     base::Unretained(desktop_display_info_loader_.get())));
}

DesktopDisplayInfoMonitor::~DesktopDisplayInfoMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ui_task_runner_->DeleteSoon(FROM_HERE,
                              desktop_display_info_loader_.release());
}

void DesktopDisplayInfoMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!timer_running_) {
    timer_running_ = true;
    timer_.Start(FROM_HERE, kPollingInterval, this,
                 &DesktopDisplayInfoMonitor::QueryDisplayInfoImpl);
  }
}

void DesktopDisplayInfoMonitor::QueryDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!timer_running_) {
    QueryDisplayInfoImpl();
  }
}

void DesktopDisplayInfoMonitor::AddCallback(
    DesktopDisplayInfoMonitor::Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Adding callbacks is not supported after displays have been loaded.
  DCHECK_EQ(desktop_display_info_.NumDisplays(), 0);

  callback_list_.AddUnsafe(std::move(callback));
}

void DesktopDisplayInfoMonitor::QueryDisplayInfoImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_list_.empty()) {
    ui_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&DesktopDisplayInfoLoader::GetCurrentDisplayInfo,
                       base::Unretained(desktop_display_info_loader_.get())),
        base::BindOnce(&DesktopDisplayInfoMonitor::OnDisplayInfoLoaded,
                       weak_factory_.GetWeakPtr()));
  }
}

void DesktopDisplayInfoMonitor::OnDisplayInfoLoaded(DesktopDisplayInfo info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (callback_list_.empty() || desktop_display_info_ == info) {
    return;
  }

  desktop_display_info_ = std::move(info);
  callback_list_.Notify(desktop_display_info_);
}

}  // namespace remoting
