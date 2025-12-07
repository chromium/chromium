// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_MOUSE_CURSOR_MONITOR_H_
#define REMOTING_HOST_FAKE_MOUSE_CURSOR_MONITOR_H_

#include "base/memory/raw_ptr.h"
#include "remoting/protocol/mouse_cursor_monitor.h"

namespace remoting {

class FakeMouseCursorMonitor : public protocol::MouseCursorMonitor {
 public:
  FakeMouseCursorMonitor();

  FakeMouseCursorMonitor(const FakeMouseCursorMonitor&) = delete;
  FakeMouseCursorMonitor& operator=(const FakeMouseCursorMonitor&) = delete;

  ~FakeMouseCursorMonitor() override;

  void Init(Callback* callback) override;
  void SetPreferredCaptureInterval(base::TimeDelta interval) override;

 private:
  raw_ptr<Callback> callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_MOUSE_CURSOR_MONITOR_H_
