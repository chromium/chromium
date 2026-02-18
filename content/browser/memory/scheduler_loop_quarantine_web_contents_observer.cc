// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/scheduler_loop_quarantine_web_contents_observer.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "partition_alloc/buildflags.h"

// `//chrome` and `//android_webview` expect `use_partition_alloc=true`,
// but some other `//content` embedders like Edge do support both.
// Hence this #ifdef.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/partition_alloc_support.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace content {
namespace {
// We only care about the very first navigation so we use a global to track this
// state. This variable isn't atomic because we only every access it on the
// Browser UI thread.
bool g_reconfiguration_done = false;
}  // namespace

SchedulerLoopQuarantineWebContentsObserver::
    SchedulerLoopQuarantineWebContentsObserver(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<SchedulerLoopQuarantineWebContentsObserver>(
          *web_contents) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
}

SchedulerLoopQuarantineWebContentsObserver::
    ~SchedulerLoopQuarantineWebContentsObserver() = default;

void SchedulerLoopQuarantineWebContentsObserver::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_reconfiguration_done) {
    // Some other WebContents has already triggered the reconfiguration, we are
    // not needed anymore, so we can detach.
    web_contents()->RemoveUserData(UserDataKey());  // will delete `this`.
    return;
  }
  // SAFETY: We want to start the quarantine/zapping of free'd memory if it is
  // exploitable by the web. "about:blank" is specifically the empty blank no
  // content page.
  if (navigation_handle->GetURL().IsAboutBlank()) {
    return;
  }

  // We are only interested in the first navigation, reconfigure, remove
  // ourselves, and mark it as configured for other observers.
  // ReconfigureSchedulerLoopQuarantineBranch() is only available when
  // PartitionAlloc is built. Without PA the quarantine is a no-op.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::ReconfigureSchedulerLoopQuarantineBranch(
      base::allocator::SchedulerLoopQuarantineBranchType::kMain);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  g_reconfiguration_done = true;
  web_contents()->RemoveUserData(UserDataKey());  // will delete `this`.
  // `this` is now deleted do not touch anything here anymore.
  return;
}

// static
void SchedulerLoopQuarantineWebContentsObserver::MaybeCreateForWebContents(
    WebContents* web_contents) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Without PartitionAlloc there is nothing to reconfigure, so skip creating
  // the observer entirely.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (base::allocator::IsSchedulerLoopQuarantineEnabled("") &&
      !AlreadyTriggeredReconfiguration()) {
    SchedulerLoopQuarantineWebContentsObserver::CreateForWebContents(
        web_contents);
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

// static
bool SchedulerLoopQuarantineWebContentsObserver::
    AlreadyTriggeredReconfiguration() {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_reconfiguration_done;
}

// static
void SchedulerLoopQuarantineWebContentsObserver::ResetForTesting() {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  g_reconfiguration_done = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SchedulerLoopQuarantineWebContentsObserver);
}  // namespace content