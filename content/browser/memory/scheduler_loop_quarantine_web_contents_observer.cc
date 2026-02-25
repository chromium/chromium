// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/scheduler_loop_quarantine_web_contents_observer.h"

#include "base/memory/safety_checks.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
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
// state. This variable isn't atomic because we only ever access it on the
// Browser UI thread.
bool g_reconfiguration_done = false;

bool IsEligibleForQuarantineExclusion(NavigationHandle& navigation_handle) {
  // We ignore non-primary main frame navigations to prevent abuse from
  // subframes to disable the queue, but also because most user impact occurs on
  // the primary main frame.
  const bool is_primary_main_frame = navigation_handle.IsInPrimaryMainFrame();
  // SAFETY: We want to start the quarantine/zapping of free'd memory if it
  // is exploitable by the web. "about:blank" is specifically the empty
  // blank no content page.
  const bool is_about_blank = navigation_handle.GetURL().IsAboutBlank();
  return is_primary_main_frame && !is_about_blank;
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Ties the lifetime of the quarantine exclusion directly to the
// NavigationHandle. This naturally handles parallel subframe or same-document
// navigations without any manual state tracking or interference.
class QuarantineExclusionHolder
    : public NavigationHandleUserData<QuarantineExclusionHolder> {
 public:
  ~QuarantineExclusionHolder() override = default;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

 private:
  explicit QuarantineExclusionHolder(NavigationHandle& handle) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(IsEligibleForQuarantineExclusion(handle));
  }
  friend class NavigationHandleUserData<QuarantineExclusionHolder>;
  base::allocator::ScopedSchedulerLoopQuarantineExclusion exclusion_;
};
NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(QuarantineExclusionHolder);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
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

void SchedulerLoopQuarantineWebContentsObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // If the quarantine hasn't been configured yet, there is no need to disable
  // the quarantine.
  if (!g_reconfiguration_done ||
      !IsEligibleForQuarantineExclusion(*navigation_handle)) {
    return;
  }
  // `opt_out_scheduler_loop_quarantine_` is only available when
  // PartitionAlloc is malloc. Without PA the quarantine is a no-op.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  QuarantineExclusionHolder::CreateForNavigationHandle(*navigation_handle);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void SchedulerLoopQuarantineWebContentsObserver::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_reconfiguration_done) {
    // Some other WebContents has already triggered the reconfiguration.
    return;
  }
  // SAFETY: We want to start the quarantine/zapping of free'd memory if it is
  // exploitable by the web. "about:blank" is specifically the empty blank no
  // content page.
  if (!IsEligibleForQuarantineExclusion(*navigation_handle)) {
    return;
  }

  // We are only interested in the first navigation for reconfigure so we mark
  // it as configured for other observers.
  // ReconfigureSchedulerLoopQuarantineBranch() is only available when
  // PartitionAlloc is built. Without PA the quarantine is a no-op.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::ReconfigureSchedulerLoopQuarantineBranch(
      base::allocator::SchedulerLoopQuarantineBranchType::kMain);
  // We currently opt navigation code out of the quarantine which is normally
  // done in DidStartNavigation, but because we hadn't configured yet we need to
  // do it now.
  QuarantineExclusionHolder::CreateForNavigationHandle(*navigation_handle);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  g_reconfiguration_done = true;
  return;
}

// static
void SchedulerLoopQuarantineWebContentsObserver::MaybeCreateForWebContents(
    WebContents* web_contents) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  // Without PartitionAlloc there is nothing to reconfigure, so skip creating
  // the observer entirely.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if (base::allocator::IsSchedulerLoopQuarantineEnabled("")) {
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
