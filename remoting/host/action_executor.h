// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ACTION_EXECUTOR_H_
#define REMOTING_HOST_ACTION_EXECUTOR_H_

#include <memory>

#include "base/macros.h"

namespace remoting {

namespace protocol {
class ActionRequest;
}  // namespace protocol

class ActionExecutor {
 public:
  virtual ~ActionExecutor();

  // Creates an action executor for the current platform / host architecture.
  static std::unique_ptr<ActionExecutor> Create();

  // Implementations must never assume the presence of any |request| fields,
  // nor assume that their contents are valid.
  virtual void ExecuteAction(const protocol::ActionRequest& request) = 0;

 protected:
  ActionExecutor();

 private:
  DISALLOW_COPY_AND_ASSIGN(ActionExecutor);
};

}  // namespace remoting

#endif  // REMOTING_HOST_ACTION_EXECUTOR_H_
