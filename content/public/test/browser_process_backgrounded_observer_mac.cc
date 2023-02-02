// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_process_backgrounded_observer_mac.h"

#include "content/browser/gpu/browser_child_process_backgrounded_bridge.h"

namespace content {

ProcessBackgroundedObserver::ProcessBackgroundedObserver(
    base::RepeatingCallback<void(int, bool)> callback) {
  DCHECK(callback);
  BrowserChildProcessBackgroundedBridge::SetCallbackForTesting(
      std::move(callback));
}

ProcessBackgroundedObserver::~ProcessBackgroundedObserver() {
  BrowserChildProcessBackgroundedBridge::SetCallbackForTesting(
      base::RepeatingCallback<void(int, bool)>());
}

}  // namespace content