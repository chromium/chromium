// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_CHILD_PROCESS_BACKGROUNDED_BRIDGE_H_
#define CONTENT_BROWSER_GPU_BROWSER_CHILD_PROCESS_BACKGROUNDED_BRIDGE_H_

#include <objc/objc.h>

#include "base/memory/raw_ptr.h"
#include "base/process/port_provider_mac.h"
#include "base/scoped_observation.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class BrowserChildProcessHostImpl;

// This class ensures that the backgrounded state of `process` mirrors the
// backgrounded state of the browser process.
class CONTENT_EXPORT BrowserChildProcessBackgroundedBridge
    : public base::PortProvider::Observer {
 public:
  explicit BrowserChildProcessBackgroundedBridge(
      BrowserChildProcessHostImpl* process);
  ~BrowserChildProcessBackgroundedBridge() override;

  void SimulateBrowserProcessForegroundedForTesting();
  void SimulateBrowserProcessBackgroundedForTesting();

  static void SetOSNotificationsEnabledForTesting(bool enabled);

 private:
  void Initialize();

  void OnReceivedTaskPort(base::ProcessHandle process) override;

  void OnBrowserProcessForegrounded();
  void OnBrowserProcessBackgrounded();

  raw_ptr<BrowserChildProcessHostImpl> process_;

  base::ScopedObservation<base::PortProvider, base::PortProvider::Observer>
      scoped_port_provider_observer_{this};

  // Registration IDs for NSApplicationDidBecomeActiveNotification and
  // NSApplicationDidResignActiveNotification.
  id did_become_active_observer_ = nil;
  id did_resign_active_observer_ = nil;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_CHILD_PROCESS_BACKGROUNDED_BRIDGE_H_
