// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_child_process_backgrounded_bridge.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <memory>

#include "base/process/process.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/public/browser/child_process_data.h"

namespace content {

namespace {

bool g_notifications_enabled = true;

}  // namespace

struct BrowserChildProcessBackgroundedBridge::ObjCStorage {
  // Registration IDs for NSApplicationDidBecomeActiveNotification and
  // NSApplicationDidResignActiveNotification.
  id __strong did_become_active_observer = nil;
  id __strong did_resign_active_observer = nil;
};

BrowserChildProcessBackgroundedBridge::BrowserChildProcessBackgroundedBridge(
    BrowserChildProcessHostImpl* process)
    : process_(process), objc_storage_(std::make_unique<ObjCStorage>()) {
  base::PortProvider* port_provider =
      BrowserChildProcessHost::GetPortProvider();
  if (port_provider->TaskForHandle(process_->GetData().GetProcess().Handle()) !=
      MACH_PORT_NULL) {
    Initialize();
  } else {
    // The process has launched but the task port is not available yet. Defer
    // initialization until it is.
    scoped_port_provider_observer_.Observe(port_provider);
  }
}

BrowserChildProcessBackgroundedBridge::
    ~BrowserChildProcessBackgroundedBridge() {
  if (objc_storage_->did_become_active_observer) {
    [NSNotificationCenter.defaultCenter
        removeObserver:objc_storage_->did_become_active_observer];
  }
  if (objc_storage_->did_resign_active_observer) {
    [NSNotificationCenter.defaultCenter
        removeObserver:objc_storage_->did_resign_active_observer];
  }
}

void BrowserChildProcessBackgroundedBridge::
    SimulateBrowserProcessForegroundedForTesting() {
  OnBrowserProcessForegrounded();
}

void BrowserChildProcessBackgroundedBridge::
    SimulateBrowserProcessBackgroundedForTesting() {
  OnBrowserProcessBackgrounded();
}

// static
void BrowserChildProcessBackgroundedBridge::SetOSNotificationsEnabledForTesting(
    bool enabled) {
  g_notifications_enabled = enabled;
}

void BrowserChildProcessBackgroundedBridge::Initialize() {
  // Do the initial adjustment based on the initial value of the
  // TASK_CATEGORY_POLICY role of the browser process.
  base::SelfPortProvider self_port_provider;
  const base::Process::Priority browser_process_priority =
      base::Process::Current().GetPriority(&self_port_provider);
  process_->SetProcessPriority(browser_process_priority);

  if (!g_notifications_enabled) {
    return;
  }

  // Now subscribe to both NSApplicationDidBecomeActiveNotification and
  // NSApplicationDidResignActiveNotification, which are sent when the browser
  // process becomes foreground and background, respectively. The blocks
  // implicitly captures `this`. It is safe to do so since the subscriptions are
  // removed in the destructor.
  objc_storage_->did_become_active_observer =
      [NSNotificationCenter.defaultCenter
          addObserverForName:NSApplicationDidBecomeActiveNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    OnBrowserProcessForegrounded();
                  }];
  objc_storage_->did_resign_active_observer =
      [NSNotificationCenter.defaultCenter
          addObserverForName:NSApplicationDidResignActiveNotification
                      object:nil
                       queue:nil
                  usingBlock:^(NSNotification* notification) {
                    OnBrowserProcessBackgrounded();
                  }];
}

void BrowserChildProcessBackgroundedBridge::OnReceivedTaskPort(
    base::ProcessHandle process_handle) {
  if (process_->GetData().GetProcess().Handle() != process_handle) {
    return;
  }

  // Just received the task port for the target process. It is now possible to
  // change its TASK_CATEGORY_POLICY.
  scoped_port_provider_observer_.Reset();
  Initialize();
}

void BrowserChildProcessBackgroundedBridge::OnBrowserProcessForegrounded() {
  process_->SetProcessPriority(base::Process::Priority::kUserBlocking);
}

void BrowserChildProcessBackgroundedBridge::OnBrowserProcessBackgrounded() {
  process_->SetProcessPriority(base::Process::Priority::kUserVisible);
}

}  // namespace content
