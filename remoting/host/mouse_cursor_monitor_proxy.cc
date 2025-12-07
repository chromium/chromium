// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mouse_cursor_monitor_proxy.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "remoting/host/chromeos/mouse_cursor_monitor_aura.h"
#endif

namespace remoting {

class MouseCursorMonitorProxy::Core
    : public webrtc::MouseCursorMonitor::Callback {
 public:
  explicit Core(base::WeakPtr<MouseCursorMonitorProxy> proxy);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() override;

  void CreateMouseCursorMonitor(
      base::OnceCallback<std::unique_ptr<webrtc::MouseCursorMonitor>()>
          creator);

  void Init(webrtc::MouseCursorMonitor::Mode mode);
  void Capture();

 private:
  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* mouse_cursor) override;
  void OnMouseCursorPosition(const webrtc::DesktopVector& position) override;

  base::ThreadChecker thread_checker_;

  base::WeakPtr<MouseCursorMonitorProxy> proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor_;
};

MouseCursorMonitorProxy::Core::Core(
    base::WeakPtr<MouseCursorMonitorProxy> proxy)
    : proxy_(proxy),
      caller_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  thread_checker_.DetachFromThread();
}

MouseCursorMonitorProxy::Core::~Core() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MouseCursorMonitorProxy::Core::CreateMouseCursorMonitor(
    base::OnceCallback<std::unique_ptr<webrtc::MouseCursorMonitor>()> creator) {
  DCHECK(thread_checker_.CalledOnValidThread());

  mouse_cursor_monitor_ = std::move(creator).Run();

  if (!mouse_cursor_monitor_) {
    LOG(ERROR) << "Failed to initialize MouseCursorMonitor.";
  }
}

void MouseCursorMonitorProxy::Core::Init(
    webrtc::MouseCursorMonitor::Mode mode) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (mouse_cursor_monitor_) {
    mouse_cursor_monitor_->Init(this, mode);
  }
}

void MouseCursorMonitorProxy::Core::Capture() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (mouse_cursor_monitor_) {
    mouse_cursor_monitor_->Capture();
  }
}

void MouseCursorMonitorProxy::Core::OnMouseCursor(webrtc::MouseCursor* cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<webrtc::MouseCursor> owned_cursor(cursor);
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorMonitorProxy::OnMouseCursor, proxy_,
                                std::move(owned_cursor)));
}

void MouseCursorMonitorProxy::Core::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK(thread_checker_.CalledOnValidThread());

  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MouseCursorMonitorProxy::OnMouseCursorPosition,
                                proxy_, position));
}

MouseCursorMonitorProxy::MouseCursorMonitorProxy(
    scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
    base::OnceCallback<std::unique_ptr<webrtc::MouseCursorMonitor>()> creator)
    : capture_task_runner_(capture_task_runner) {
  core_ = std::make_unique<Core>(weak_factory_.GetWeakPtr());
  capture_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Core::CreateMouseCursorMonitor,
                     base::Unretained(core_.get()), std::move(creator)));
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

void MouseCursorMonitorProxy::OnMouseCursor(
    std::unique_ptr<webrtc::MouseCursor> cursor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_->OnMouseCursor(cursor.release());
}

void MouseCursorMonitorProxy::OnMouseCursorPosition(
    const webrtc::DesktopVector& position) {
  DCHECK(thread_checker_.CalledOnValidThread());
  callback_->OnMouseCursorPosition(position);
}

}  // namespace remoting
