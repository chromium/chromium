// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOUSE_CURSOR_MONITOR_PROXY_H_
#define REMOTING_HOST_MOUSE_CURSOR_MONITOR_PROXY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopCaptureOptions;
}  // namespace webrtc

namespace remoting {

class MouseCursorMonitorProxy : public webrtc::MouseCursorMonitor {
 public:
  MouseCursorMonitorProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      const webrtc::DesktopCaptureOptions& options);
  ~MouseCursorMonitorProxy() override;

  // webrtc::MouseCursorMonitor interface.
  void Init(Callback* callback, Mode mode) override;
  void Capture() override;

  void SetMouseCursorMonitorForTests(
      std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor);

 private:
  class Core;

  void OnMouseCursor(std::unique_ptr<webrtc::MouseCursor> cursor);
  void OnMouseCursorPosition(CursorState state,
                             const webrtc::DesktopVector& position);

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  Callback* callback_ = nullptr;

  base::WeakPtrFactory<MouseCursorMonitorProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MouseCursorMonitorProxy);
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOUSE_CURSOR_MONITOR_PROXY_H_
