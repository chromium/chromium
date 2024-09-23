// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_ACCESS_REPORT_MANAGER_H_
#define CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_ACCESS_REPORT_MANAGER_H_

#include <string>

#include "base/values.h"
#include "content/browser/security/coop/cross_origin_opener_policy_reporter.h"
#include "content/browser/security/coop/coop_swap_result.h"

namespace content {

class FrameTreeNode;

// Used to monitor (potential) COOP breakages.
// A CrossOriginOpenerPolicyAccessReportManager lives in the browser process and
// has a 1:1 relationship with a RenderFrameHost.
class CrossOriginOpenerPolicyAccessReportManager {
 public:
  CrossOriginOpenerPolicyAccessReportManager();
  ~CrossOriginOpenerPolicyAccessReportManager();

  CrossOriginOpenerPolicyReporter* coop_reporter() {
    return coop_reporter_.get();
  }
  void set_coop_reporter(
      std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter) {
    coop_reporter_ = std::move(coop_reporter);
  }

  // For every other window in the same browsing context group, but in a
  // different virtual browsing context group, install the necessary
  // CoopAccessMonitor. The first window is identified by |node|.
  static void InstallAccessMonitorsIfNeeded(FrameTreeNode* node);

  // Return a new virtual browsing context group ID belong to a new
  // CoopRelatedGroup.
  static int GetNewVirtualBrowsingContextGroup();

  // Return a virtual browsing context group ID that is suitable given the
  // information passed in. `enforce_result` and `report_only_result` indicate
  // whether actual COOP values and report-only COOP values, respectively, need
  // a browsing context group swap. It also accounts for swapping in the same
  // CoopRelatedGroup or in a different one. Combined with
  // `current_virtual_browsing_context_group`, we can decide what virtual
  // browsing context group is suitable and return its ID.
  static int GetVirtualBrowsingContextGroup(
      CoopSwapResult enforce_result,
      CoopSwapResult report_only_result,
      int current_virtual_browsing_context_group);

 private:
  // Generate a new, previously unused, virtual browsing context group ID.
  static int NextVirtualBrowsingContextGroup();

  // Generate a new, previously unused, virtual CoopRelatedGroup ID.
  static int NextVirtualCoopRelatedGroup();

  // Install the CoopAccessMonitors monitoring accesses from `accessing_node`
  // toward `accessed_node`. `is_in_same_virtual_coop_related_group` indicates
  // whether the two nodes would be in the same CoopRelatedGroup. If that's the
  // case, do not report window.postMessage() and window.closed accesses, which
  // would be permitted by COOP: restrict-properties.
  void MonitorAccesses(FrameTreeNode* accessing_node,
                       FrameTreeNode* accessed_node,
                       bool is_in_same_virtual_coop_related_group);

  std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_ACCESS_REPORT_MANAGER_H_
