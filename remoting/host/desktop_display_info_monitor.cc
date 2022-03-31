// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_control.h"

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
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : ui_task_runner_(ui_task_runner),
      client_session_control_(client_session_control),
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
  timer_.Start(FROM_HERE, kPollingInterval, this,
               &DesktopDisplayInfoMonitor::QueryDisplayInfo);
}

void DesktopDisplayInfoMonitor::QueryDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_session_control_) {
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

  if (!client_session_control_ || desktop_display_info_ == info) {
    return;
  }

  desktop_display_info_ = std::move(info);

  auto layout = std::make_unique<protocol::VideoLayout>();
  HOST_LOG << "DDIM::OnDisplayInfoLoaded";
  for (const auto& display : desktop_display_info_.displays()) {
    protocol::VideoTrackLayout* track = layout->add_video_track();
    track->set_position_x(display.x);
    track->set_position_y(display.y);
    track->set_width(display.width);
    track->set_height(display.height);
    track->set_x_dpi(display.dpi);
    track->set_y_dpi(display.dpi);
    track->set_screen_id(display.id);
    HOST_LOG << "   Display: " << display.x << "," << display.y << " "
             << display.width << "x" << display.height << " @ " << display.dpi
             << ", id=" << display.id;
  }
  client_session_control_->OnDesktopDisplayChanged(std::move(layout));
}

}  // namespace remoting
