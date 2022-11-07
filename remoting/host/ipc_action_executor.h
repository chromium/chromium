// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_ACTION_EXECUTOR_H_
#define REMOTING_HOST_IPC_ACTION_EXECUTOR_H_

#include "base/memory/scoped_refptr.h"
#include "remoting/host/action_executor.h"
#include "remoting/proto/action.pb.h"

namespace remoting {

class DesktopSessionProxy;

// Routes ActionExecutor calls though the IPC channel to the desktop session
// agent running in the desktop integration process.
class IpcActionExecutor : public ActionExecutor {
 public:
  explicit IpcActionExecutor(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcActionExecutor(const IpcActionExecutor&) = delete;
  IpcActionExecutor& operator=(const IpcActionExecutor&) = delete;

  ~IpcActionExecutor() override;

  // ActionStub interface.
  void ExecuteAction(const protocol::ActionRequest& request) override;

 private:
  // Wraps the IPC channel to the desktop process.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_ACTION_EXECUTOR_H_
