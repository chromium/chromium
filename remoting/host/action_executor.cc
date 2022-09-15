// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/action_executor.h"

namespace remoting {

// static
std::unique_ptr<ActionExecutor> ActionExecutor::Create() {
  return nullptr;
}

ActionExecutor::ActionExecutor() = default;

ActionExecutor::~ActionExecutor() = default;

}  // namespace remoting
