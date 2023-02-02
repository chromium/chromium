// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_child_process_backgrounded_bridge.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "base/process/process.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/public/browser/child_process_data.h"

namespace content {

namespace {

base::RepeatingCallback<void(int, bool)>& GetCallback() {
  static base::NoDestructor<base::RepeatingCallback<void(int, bool)>> callback;
  return *callback;
}

}  // namespace

BrowserChildProcessBackgroundedBridge::BrowserChildProcessBackgroundedBridge(
    BrowserChildProcessHostImpl* process)
    : process_(process) {
  base::PortProvider* port_provider =
      BrowserChildProcessHost::GetPortProvider();
  if (port_provider->TaskForPid(process_->GetData().GetProcess().Pid())) {
    Initialize();
  } else {
    // The process has launched but the task port is not available yet. Defer
    // initialization until it is.
    port_provider->AddObserver(this);
  }
}

BrowserChildProcessBackgroundedBridge::
    ~BrowserChildProcessBackgroundedBridge() {
  [NSNotificationCenter.defaultCenter
      removeObserver:did_become_active_observer_];
  [NSNotificationCenter.defaultCenter
      removeObserver:did_resign_active_observer_];
}

// static
void BrowserChildProcessBackgroundedBridge::SetCallbackForTesting(
    base::RepeatingCallback<void(int, bool)> callback) {
  DCHECK_NE(GetCallback().is_null(), callback.is_null());
  GetCallback() = std::move(callback);
}

void BrowserChildProcessBackgroundedBridge::Initialize() {
  // Do the initial ajustment based on the initial value of the
  // TASK_CATEGORY_POLICY role of the browser process.
  base::SelfPortProvider self_port_provider;
  process_->SetProcessBackgrounded(
      base::Process::Current().IsProcessBackgrounded(&self_port_provider));

  // Now subscribe to both NSApplicationDidBecomeActiveNotification and
  // NSApplicationDidResignActiveNotification, which are sent when the browser
  // process becomes foreground and background, respectively. The blocks
  // implicity captures `this`. It is safe to do so since the subscriptions are
  // removed in the destructor
  did_become_active_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:NSApplicationDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                OnForeground();
              }];
  did_resign_active_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:NSApplicationDidResignActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                OnBackground();
              }];
}

void BrowserChildProcessBackgroundedBridge::OnReceivedTaskPort(
    base::ProcessHandle process_handle) {
  if (process_->GetData().GetProcess().Handle() != process_handle) {
    return;
  }

  // Just received the task port for the target process. It is now possible to
  // change its TASK_CATEGORY_POLICY.
  BrowserChildProcessHost::GetPortProvider()->RemoveObserver(this);
  Initialize();
}

void BrowserChildProcessBackgroundedBridge::OnForeground() {
  process_->SetProcessBackgrounded(false);

  if (GetCallback()) {
    GetCallback().Run(process_->GetData().GetProcess().Pid(), false);
  }
}

void BrowserChildProcessBackgroundedBridge::OnBackground() {
  process_->SetProcessBackgrounded(true);

  if (GetCallback()) {
    GetCallback().Run(process_->GetData().GetProcess().Pid(), true);
  }
}

}  // namespace content
