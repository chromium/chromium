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
enum class ProcessReusePolicy {
  // In this mode, all instances of the site will be hosted in the same
  // RenderProcessHost.
  PROCESS_PER_SITE,

  // In this mode, the subframe's site will be rendered in a RenderProcessHost
  // that is already in use for the site, either for a pending navigation or a
  // committed navigation. If multiple such processes exist, ones that have
  // foreground frames are given priority, and otherwise one is selected
  // randomly.
  REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME,

  // Similar to REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME, but only applied to
  // workers. Reuse decisions may vary from those for
  // REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME.
  REUSE_PENDING_OR_COMMITTED_SITE_WORKER,

  // When used, this is similar to REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME, but
  // for main frames, and limiting the number of main frames a RenderProcessHost
  // can host to a certain threshold.
  REUSE_PENDING_OR_COMMITTED_SITE_WITH_MAIN_FRAME_THRESHOLD,

  // In this mode, SiteInstances don't proactively reuse processes. An
  // existing process with an unmatched service worker for the site is reused
  // only for navigations, not for service workers. When the process limit has
  // been reached, a randomly chosen RenderProcessHost is reused as in the
  // other policies.
  DEFAULT,
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_REUSE_POLICY_H_
