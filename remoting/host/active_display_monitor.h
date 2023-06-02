// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACTIVE_DISPLAY_MONITOR_H_
#define REMOTING_HOST_ACTIVE_DISPLAY_MONITOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ActiveDisplayMonitor {
 public:
  using Callback = base::RepeatingCallback<void(webrtc::ScreenId)>;

  virtual ~ActiveDisplayMonitor() = default;

  static std::unique_ptr<ActiveDisplayMonitor> Create(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      Callback active_display_callback);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACTIVE_DISPLAY_MONITOR_H_
