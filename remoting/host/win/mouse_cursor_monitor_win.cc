// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/mouse_cursor_monitor_win.h"

#include <windows.h>

#include <WinUser.h>

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace remoting {

namespace {

// Poll cursor image at 100 Hz. webrtc::MouseCursorMonitorWin will not load the
// cursor bitmap if the handle is unchanged, so the call should not be
// expensive. https://devblogs.microsoft.com/oldnewthing/20151116-00/?p=92091
// suggests watching for `EVENT_OBJECT_NAMECHANGE` for cursor image changes.
// This appears to work on Windows 11 23H2, but no longer works on and after
// 24H2. See http://crbug.com/469831346. EVENT_OBJECT_LOCATIONCHANGE still
// works, so it remains event driven.
constexpr base::TimeDelta kCursorImagePollingInterval = base::Hertz(100);

}  // namespace

// The delegate runs on DesktopEventHandler's internal sequence, and its
// lifetime is managed by DesktopEventHandler. It runs capturing jobs on the
// internal sequence and posts the result back to MouseCursorMonitorWin on its
// caller's sequence.
class MouseCursorMonitorWin::Delegate : public DesktopEventHandler::Delegate,
                                        webrtc::MouseCursorMonitor::Callback {
 public:
  explicit Delegate(base::WeakPtr<MouseCursorMonitorWin> monitor);
  ~Delegate() override;

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;

  // DesktopEventHandler::Delegate implementation.
  void OnWorkerThreadStarted() override;
  void OnWorkerThreadStopping() override;
  void OnEvent(DWORD event, LONG object_id) override;

 private:
  void CaptureCursorImage();
  void CaptureCursorPosition();

  // webrtc::MouseCursorMonitor::Callback implementation
  void OnMouseCursor(webrtc::MouseCursor* cursor) override;

  // This is only used to capture the cursor image.
  std::unique_ptr<webrtc::MouseCursorMonitor> webrtc_monitor_;
  base::RepeatingTimer capture_cursor_image_timer_;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;
  // Bound to the sequence of `caller_task_runner_`.
  base::WeakPtr<MouseCursorMonitorWin> monitor_;
};

MouseCursorMonitorWin::Delegate::Delegate(
    base::WeakPtr<MouseCursorMonitorWin> monitor)
    : monitor_(monitor) {
  caller_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

MouseCursorMonitorWin::Delegate::~Delegate() = default;

void MouseCursorMonitorWin::Delegate::OnWorkerThreadStarted() {
  webrtc_monitor_ = webrtc::MouseCursorMonitor::Create(
      webrtc::DesktopCaptureOptions::CreateDefault());
  webrtc_monitor_->Init(this, webrtc::MouseCursorMonitor::SHAPE_ONLY);

  capture_cursor_image_timer_.Start(FROM_HERE, kCursorImagePollingInterval,
                                    this, &Delegate::CaptureCursorImage);

  // Capture the initial cursor image and position.
  CaptureCursorImage();
  CaptureCursorPosition();
}

void MouseCursorMonitorWin::Delegate::OnWorkerThreadStopping() {
  capture_cursor_image_timer_.Stop();
}

void MouseCursorMonitorWin::Delegate::OnEvent(DWORD event, LONG object_id) {
  if (object_id != OBJID_CURSOR) {
    return;
  }

  switch (event) {
    case EVENT_OBJECT_LOCATIONCHANGE:
      CaptureCursorPosition();
      break;
    case EVENT_OBJECT_NAMECHANGE: {
      static bool logged_once = false;
      LOG_IF(INFO, !logged_once)
          << "EVENT_OBJECT_NAMECHANGE fired. See: crbug.com/474129133";
      logged_once = true;
      break;
    }
    default:
      LOG(WARNING) << "Unknown event: " << event;
  }
}

void MouseCursorMonitorWin::Delegate::CaptureCursorImage() {
  webrtc_monitor_->Capture();
}

void MouseCursorMonitorWin::Delegate::CaptureCursorPosition() {
  POINT pos;
  if (!GetCursorPos(&pos)) {
    PLOG(ERROR) << "Failed to capture cursor position";
    return;
  }
  webrtc::DesktopVector screen_top_left{GetSystemMetrics(SM_XVIRTUALSCREEN),
                                        GetSystemMetrics(SM_YVIRTUALSCREEN)};
  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MouseCursorMonitorWin::OnMouseCursorPosition, monitor_,
          webrtc::DesktopVector{pos.x, pos.y}.subtract(screen_top_left)));
}

void MouseCursorMonitorWin::Delegate::OnMouseCursor(
    webrtc::MouseCursor* cursor) {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorMonitorWin::OnMouseCursor, monitor_,
                                base::WrapUnique(cursor)));
}

MouseCursorMonitorWin::MouseCursorMonitorWin(
    std::unique_ptr<DesktopDisplayInfoMonitor> display_monitor) {
  display_monitor_ = std::move(display_monitor);
}

MouseCursorMonitorWin::~MouseCursorMonitorWin() = default;

void MouseCursorMonitorWin::Init(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;
  display_monitor_->Start();
  event_handler_.Start(
      /*min_event=*/EVENT_OBJECT_LOCATIONCHANGE,
      /*max_event=*/EVENT_OBJECT_NAMECHANGE,
      std::make_unique<Delegate>(weak_ptr_factory_.GetWeakPtr()));
}

void MouseCursorMonitorWin::SetPreferredCaptureInterval(
    base::TimeDelta interval) {
  // No-op since callback will be run once cursor is changed.
}

void MouseCursorMonitorWin::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> cursor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_) {
    callback_->OnMouseCursor(std::move(cursor));
  }
}

void MouseCursorMonitorWin::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!callback_) {
    return;
  }
  callback_->OnMouseCursorPosition(position);
  const auto* display_info = display_monitor_->GetLatestDisplayInfo();
  if (!display_info) {
    return;
  }
  std::optional<protocol::FractionalCoordinate> fractional_position =
      display_info->ToFractionalCoordinate(position);
  if (fractional_position.has_value()) {
    callback_->OnMouseCursorFractionalPosition(*fractional_position);
  } else {
    LOG(ERROR) << "Cursor position " << position.x() << ", " << position.y()
               << " is not within any display.";
  }
}

}  // namespace remoting
