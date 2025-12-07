// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOUSE_CURSOR_MONITOR_PROXY_H_
#define REMOTING_HOST_MOUSE_CURSOR_MONITOR_PROXY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class MouseCursorMonitorProxy : public webrtc::MouseCursorMonitor {
 public:
  MouseCursorMonitorProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      base::OnceCallback<std::unique_ptr<webrtc::MouseCursorMonitor>()>
          creator);

  MouseCursorMonitorProxy(const MouseCursorMonitorProxy&) = delete;
  MouseCursorMonitorProxy& operator=(const MouseCursorMonitorProxy&) = delete;

  ~MouseCursorMonitorProxy() override;

  // webrtc::MouseCursorMonitor interface.
  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

 private:
  class Core;

  void OnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor);
  void OnMouseCursorPosition(const webrtc::DesktopVector& position);

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  raw_ptr<Callback> callback_ = nullptr;

  base::WeakPtrFactory<MouseCursorMonitorProxy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_CURSOR_MONITOR_PROXY_H_
