// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACTION_EXECUTOR_H_
#define REMOTING_HOST_ACTION_EXECUTOR_H_

#include <memory>

namespace remoting {

namespace protocol {
class ActionRequest;
}  // namespace protocol

class ActionExecutor {
 public:
  ActionExecutor(const ActionExecutor&) = delete;
  ActionExecutor& operator=(const ActionExecutor&) = delete;

  virtual ~ActionExecutor();

  // Creates an action executor for the current platform / host architecture.
  static std::unique_ptr<ActionExecutor> Create();

  // Implementations must never assume the presence of any |request| fields,
  // nor assume that their contents are valid.
  virtual void ExecuteAction(const protocol::ActionRequest& request) = 0;

 protected:
  ActionExecutor();
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACTION_EXECUTOR_H_
