// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_WINDOW_PROXY_H_
#define REMOTING_HOST_HOST_WINDOW_PROXY_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/host_window.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {

// Takes an instance of |HostWindow| and runs it on the |ui_task_runner| thread.
class HostWindowProxy : public HostWindow {
 public:
  HostWindowProxy(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      std::unique_ptr<HostWindow> host_window);

  HostWindowProxy(const HostWindowProxy&) = delete;
  HostWindowProxy& operator=(const HostWindowProxy&) = delete;

  ~HostWindowProxy() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  // All thread switching logic is implemented in the ref-counted |Core| class.
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_WINDOW_PROXY_H_
