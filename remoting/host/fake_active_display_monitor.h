// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_ACTIVE_DISPLAY_MONITOR_H_
#define REMOTING_HOST_FAKE_ACTIVE_DISPLAY_MONITOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/active_display_monitor.h"

namespace remoting {

class FakeActiveDisplayMonitor : public ActiveDisplayMonitor {
 public:
  explicit FakeActiveDisplayMonitor(ActiveDisplayMonitor::Callback callback);

  FakeActiveDisplayMonitor(const FakeActiveDisplayMonitor&) = delete;
  FakeActiveDisplayMonitor& operator=(const FakeActiveDisplayMonitor&) = delete;

  ~FakeActiveDisplayMonitor() override;

  base::WeakPtr<FakeActiveDisplayMonitor> GetWeakPtr();
  void SetActiveDisplay(webrtc::ScreenId display);

 private:
  ActiveDisplayMonitor::Callback callback_;

  base::WeakPtrFactory<FakeActiveDisplayMonitor> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_ACTIVE_DISPLAY_MONITOR_H_
