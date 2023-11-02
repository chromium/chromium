// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CROSS_ORIGIN_OPENER_POLICY_ACCESS_REPORT_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_CROSS_ORIGIN_OPENER_POLICY_ACCESS_REPORT_MANAGER_H_

#include <string>

#include "base/values.h"
#include "content/browser/network/cross_origin_opener_policy_reporter.h"

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

  // Generate a new, previously unused, virtualBrowsingContextId.
  static int NextVirtualBrowsingContextGroup();

 private:
  // Install the CoopAccessMonitors monitoring accesses from |accessing_node|
  // toward |accessed_node|.
  void MonitorAccesses(FrameTreeNode* accessing_node,
                       FrameTreeNode* accessed_node);

  std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CROSS_ORIGIN_OPENER_POLICY_ACCESS_REPORT_MANAGER_H_
