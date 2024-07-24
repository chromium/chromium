// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_OBSERVER_H_

#include "content/common/content_export.h"

namespace content {

struct ChildProcessData;
struct ChildProcessTerminationInfo;

// An observer API implemented by classes which are interested in browser child
// process events. Note that render processes cannot be observed through this
// interface; use RenderProcessHostObserver instead.
class CONTENT_EXPORT BrowserChildProcessObserver {
 public:
  // Called when a child process has successfully launched and has connected to
  // it child process host. `data.GetProcess()` is guaranteed to be valid.
  virtual void BrowserChildProcessLaunchedAndConnected(
      const ChildProcessData& data) {}

  // Called after a ChildProcessHost is disconnected from the child process.
  virtual void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) {}

  // Called when a child process disappears unexpectedly as a result of a crash.
  virtual void BrowserChildProcessCrashed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) {}

  // Called when a child process disappears unexpectedly as a result of being
  // killed.
  virtual void BrowserChildProcessKilled(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) {}

  // Called when a child process never launches successfully. In this case,
  // info.status will be TERMINATION_STATUS_LAUNCH_FAILED and info.exit_code
  // will contain a platform specific launch failure error code.
  virtual void BrowserChildProcessLaunchFailed(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) {}

  // Called when a child process exits without crashing or being killed.
  virtual void BrowserChildProcessExitedNormally(
      const ChildProcessData& data,
      const ChildProcessTerminationInfo& info) {}

  // Note for Android. There is no way to reliably distinguish between Crash
  // and Kill. Arbitrarily choose all abnormal terminations on Android to call
  // BrowserChildProcessKilled, which means BrowserChildProcessCrashed will
  // never be called on Android.

 protected:
  // The observer can be destroyed on any thread.
  virtual ~BrowserChildProcessObserver() {}

  static void Add(BrowserChildProcessObserver* observer);
  static void Remove(BrowserChildProcessObserver* observer);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSER_CHILD_PROCESS_OBSERVER_H_
