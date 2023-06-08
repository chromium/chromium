// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_ACTIVE_DISPLAY_MONITOR_X11_H_
#define REMOTING_HOST_LINUX_ACTIVE_DISPLAY_MONITOR_X11_H_

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "remoting/host/active_display_monitor.h"

namespace remoting {

class ActiveDisplayMonitorX11 : public ActiveDisplayMonitor {
 public:
  ActiveDisplayMonitorX11(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      Callback active_display_callback);

  ActiveDisplayMonitorX11(const ActiveDisplayMonitorX11&) = delete;
  ActiveDisplayMonitorX11& operator=(const ActiveDisplayMonitorX11&) = delete;

  ~ActiveDisplayMonitorX11() override;

 private:
  class Core;

  base::SequenceBound<Core> core_;
  base::CancelableRepeatingCallback<Callback::RunType> active_display_callback_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_ACTIVE_DISPLAY_MONITOR_X11_H_
