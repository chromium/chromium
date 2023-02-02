// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_PROCESS_BACKGROUNDED_OBSERVER_MAC_H_
#define CONTENT_PUBLIC_TEST_BROWSER_PROCESS_BACKGROUNDED_OBSERVER_MAC_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"

namespace content {

// This class allows tests to be notified when a process was
// backgrounded or foregrounded by the BrowserChildProcessBackgroundedBridge
// class. Only one instance can be instantiated at a time (verified by a
// DCHECK).
class CONTENT_EXPORT ProcessBackgroundedObserver {
 public:
  // This callback is invoked every time a process is backgrounded or
  // foregrounded. The parameters are the ID of the process and whether the
  // process is now backgrounded.
  using OnProcessBackgroundedChangedCallback =
      base::RepeatingCallback<void(int, bool)>;
  explicit ProcessBackgroundedObserver(
      OnProcessBackgroundedChangedCallback callback);
  ~ProcessBackgroundedObserver();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_PROCESS_BACKGROUNDED_OBSERVER_MAC_H_
