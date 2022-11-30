// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_throttle_handle.h"

namespace content {

DevToolsThrottleHandle::DevToolsThrottleHandle(
    base::OnceCallback<void()> throttle_callback)
    : throttle_callback_(std::move(throttle_callback)) {}

DevToolsThrottleHandle::~DevToolsThrottleHandle() {
  std::move(throttle_callback_).Run();
}

}  // namespace content
