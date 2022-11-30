// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_child_process_host_delegate.h"

namespace content {

absl::optional<std::string> BrowserChildProcessHostDelegate::GetServiceName() {
  return absl::nullopt;
}

bool BrowserChildProcessHostDelegate::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

}  // namespace content
