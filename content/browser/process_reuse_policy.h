// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_REUSE_POLICY_H_
#define CONTENT_BROWSER_PROCESS_REUSE_POLICY_H_

namespace content {

// The policy to apply when selecting a RenderProcessHost for a SiteInstance.
// If no suitable RenderProcessHost for the SiteInstance exists according to the
// policy, and there are processes with unmatched service workers for the site,
// the newest process with an unmatched service worker is reused. If still no
// RenderProcessHost exists, a new RenderProcessHost will be created unless the
// soft process limit has been reached. When the limit has been reached, an
// existing suitable (e.g., same-site if Site Isolation is enabled)
// RenderProcessHost will be chosen randomly to be reused when possible.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProcessReusePolicy)
enum class ProcessReusePolicy {
  // In this mode, SiteInstances don't proactively reuse processes. An
  // existing process with an unmatched service worker for the site is reused
  // only for navigations, not for service workers. When the process limit has
  // been reached, a randomly chosen RenderProcessHost is reused as in the
  // other policies.
  kDefault = 0,

  // In this mode, all instances of the site will be hosted in the same
  // RenderProcessHost.
  kProcessPerSite = 1,

  // In this mode, the subframe's site will be rendered in a RenderProcessHost
  // that is already in use for the site, either for a pending navigation or a
  // committed navigation. If multiple such processes exist, ones that have
  // foreground frames are given priority, and otherwise one is selected
  // randomly.
  kReusePendingOrCommittedSiteSubframe = 2,

  // Similar to kReusePendingOrCommittedSiteSubframe, but only applied to
  // workers. Reuse decisions may vary from those for
  // kReusePendingOrCommittedSiteSubframe.
  kReusePendingOrCommittedSiteWorker = 3,

  // When used, this is similar to kReusePendingOrCommittedSiteSubframe, but
  // for main frames, and limiting the number of main frames a RenderProcessHost
  // can host to a certain threshold.
  kReusePendingOrCommittedSiteWithMainFrameThreshold = 4,

  // When used, this main frame's site will be rendered in a RenderProcessHost
  // that is already in use for the site and hosting prerendered frames only.
  kReusePrerenderingProcessForMainFrame = 5,

  kMaxValue = kReusePrerenderingProcessForMainFrame,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/browser/enums.xml:ProcessReusePolicy)

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_REUSE_POLICY_H_
