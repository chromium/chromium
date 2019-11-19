// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_cursor_monitor_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#if defined(OS_CHROMEOS)
#include "remoting/host/chromeos/mouse_cursor_monitor_aura.h"
#endif

namespace remoting {

class MouseCursorMonitorProxy::Core
    : public webrtc::MouseCursorMonitor::Callback {
 public:
  explicit Core(base::WeakPtr<MouseCursorMonitorProxy> proxy);
  ~Core() override;

  void CreateMouseCursorMonitor(const webrtc::DesktopCaptureOptions& options);

  void Init(webrtc::MouseCursorMonitor::Mode mode);
  void Capture();

  void SetMouseCursorMonitorForTests(
      std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor);

 private:
  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(webrtc::MouseCursorMonitor::CursorState state,
                             const webrtc::DesktopVector& position) override;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<MouseCursorMonitorProxy> proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MouseCursorMonitorProxy::Core::Core(
    base::WeakPtr<MouseCursorMonitorProxy> proxy)
    : proxy_(proxy), caller_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  thread_checker_.DetachFromThread();
}

MouseCursorMonitorProxy::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MouseCursorMonitorProxy::Core::CreateMouseCursorMonitor(
    const webrtc::DesktopCaptureOptions& options) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_CHROMEOS)
  mouse_cursor_monitor_.reset(new MouseCursorMonitorAura());
#else   // defined(OS_CHROMEOS)
  mouse_cursor_monitor_.reset(webrtc::MouseCursorMonitor::CreateForScreen(
      options, webrtc::kFullDesktopScreenId));
#endif  // defined(OS_CHROMEOS)
  if (!mouse_cursor_monitor_)
    LOG(ERROR) << "Failed to initialize MouseCursorMonitor.";
}

void MouseCursorMonitorProxy::Core::Init(
    webrtc::MouseCursorMonitor::Mode mode) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (mouse_cursor_monitor_)
    mouse_cursor_monitor_->Init(this, mode);
}

void MouseCursorMonitorProxy::Core::Capture() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (mouse_cursor_monitor_)
    mouse_cursor_monitor_->Capture();
}

void MouseCursorMonitorProxy::Core::SetMouseCursorMonitorForTests(
    std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor) {
  mouse_cursor_monitor_ = std::move(mouse_cursor_monitor);
}

void MouseCursorMonitorProxy::Core::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<webrtc::MouseCursor> owned_cursor(cursor);
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorMonitorProxy::OnMouseCursor, proxy_,
                                std::move(owned_cursor)));
}

void MouseCursorMonitorProxy::Core::OnMouseCursorPosition(
    webrtc::MouseCursorMonitor::CursorState state,
    const webrtc::DesktopVector& position) {
  DCHECK(thread_checker_.CalledOnValidThread());

  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorMonitorProxy::OnMouseCursorPosition,
                                proxy_, state, position));
}

MouseCursorMonitorProxy::MouseCursorMonitorProxy(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    const webrtc::DesktopCaptureOptions& options)
    : capture_task_runner_(capture_task_runner) {
  core_.reset(new Core(weak_factory_.GetWeakPtr()));
  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::CreateMouseCursorMonitor,
                                base::Unretained(core_.get()), options));
}

MouseCursorMonitorProxy::~MouseCursorMonitorProxy() {
  capture_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void MouseCursorMonitorProxy::Init(Callback* callback, Mode mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_ = callback;
  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::Init, base::Unretained(core_.get()), mode));
}

void MouseCursorMonitorProxy::Capture() {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Capture, base::Unretained(core_.get())));
}

void MouseCursorMonitorProxy::SetMouseCursorMonitorForTests(
    std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor) {
  capture_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetMouseCursorMonitorForTests,
                                base::Unretained(core_.get()),
                                std::move(mouse_cursor_monitor)));
}

void MouseCursorMonitorProxy::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_->OnMouseCursor(cursor.release());
}

void MouseCursorMonitorProxy::OnMouseCursorPosition(
    CursorState state,
    const webrtc::DesktopVector& position) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_->OnMouseCursorPosition(state, position);
}

}  // namespace remoting
