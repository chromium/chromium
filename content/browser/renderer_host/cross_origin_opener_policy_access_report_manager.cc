// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cross_origin_opener_policy_access_report_manager.h"

#include "base/values.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/public/browser/site_instance.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/source_location.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

absl::optional<blink::FrameToken> GetFrameToken(FrameTreeNode* frame,
                                                SiteInstance* site_instance) {
  RenderFrameHostImpl* rfh = frame->current_frame_host();
  if (rfh->GetSiteInstance() == site_instance)
    return rfh->GetFrameToken();

  RenderFrameProxyHost* proxy =
      frame->render_manager()->GetRenderFrameProxyHost(
          static_cast<SiteInstanceImpl*>(site_instance)->group());
  if (proxy)
    return proxy->GetFrameToken();

  return absl::nullopt;
}

// Used to keep track of windows for which we need to send report or register
// metrics upon access.
struct WindowToMonitor {
  WindowToMonitor(FrameTreeNode* top_level_frame,
                  bool send_reports,
                  bool register_metrics)
      : top_level_frame(top_level_frame),
        send_reports(send_reports),
        register_metrics(register_metrics) {}

  FrameTreeNode* top_level_frame = nullptr;
  bool send_reports = false;
  bool register_metrics = false;
};

// Find all the related windows that might try to access the new document in
// |frame|, but are in a different virtual browsing context group.
std::vector<WindowToMonitor> CollectOtherWindowForCoopAccess(
    FrameTreeNode* frame) {
  DCHECK(frame->IsMainFrame());
  int virtual_browsing_context_group =
      frame->current_frame_host()->virtual_browsing_context_group();
  int soap_by_default_virtual_browsing_context_group =
      frame->current_frame_host()
          ->soap_by_default_virtual_browsing_context_group();

  std::vector<WindowToMonitor> out;
  for (RenderFrameHostImpl* rfh :
       frame->current_frame_host()
           ->delegate()
           ->GetActiveTopLevelDocumentsInBrowsingContextGroup(
               frame->current_frame_host())) {
    // Filter out windows from the same virtual browsing context group.
    bool send_reports =
        rfh->virtual_browsing_context_group() != virtual_browsing_context_group;
    bool register_metrics =
        rfh->soap_by_default_virtual_browsing_context_group() !=
        soap_by_default_virtual_browsing_context_group;
    if (!send_reports && !register_metrics)
      continue;

    out.emplace_back(WindowToMonitor(rfh->frame_tree_node(), send_reports,
                                     register_metrics));
  }
  return out;
}

}  // namespace

CrossOriginOpenerPolicyAccessReportManager::
    CrossOriginOpenerPolicyAccessReportManager() = default;

CrossOriginOpenerPolicyAccessReportManager::
    ~CrossOriginOpenerPolicyAccessReportManager() = default;

// static
void CrossOriginOpenerPolicyAccessReportManager::InstallAccessMonitorsIfNeeded(
    FrameTreeNode* frame) {
  if (!frame->IsMainFrame())
    return;

  // Find all the related windows that might try to access the new document,
  // but are from a different virtual browsing context group.
  std::vector<WindowToMonitor> other_windows =
      CollectOtherWindowForCoopAccess(frame);

  CrossOriginOpenerPolicyAccessReportManager* access_manager_frame =
      frame->current_frame_host()->coop_access_report_manager();

  for (WindowToMonitor& other : other_windows) {
    CrossOriginOpenerPolicyAccessReportManager* access_manager_other =
        other.top_level_frame->current_frame_host()
            ->coop_access_report_manager();

    // If the current frame has a reporter, install the access monitors to
    // monitor the accesses between this frame and the other frame.
    access_manager_frame->MonitorAccesses(frame, other.top_level_frame,
                                          other.send_reports,
                                          other.register_metrics);
    access_manager_frame->MonitorAccesses(other.top_level_frame, frame,
                                          other.send_reports,
                                          other.register_metrics);

    // If the other frame has a reporter, install the access monitors to monitor
    // the accesses between this frame and the other frame.
    access_manager_other->MonitorAccesses(frame, other.top_level_frame,
                                          other.send_reports,
                                          other.register_metrics);
    access_manager_other->MonitorAccesses(other.top_level_frame, frame,
                                          other.send_reports,
                                          other.register_metrics);
  }
}

void CrossOriginOpenerPolicyAccessReportManager::MonitorAccesses(
    FrameTreeNode* accessing_node,
    FrameTreeNode* accessed_node,
    bool send_reports,
    bool register_metrics) {
  DCHECK_NE(accessing_node, accessed_node);
  DCHECK(accessing_node->current_frame_host()->coop_access_report_manager() ==
             this ||
         accessed_node->current_frame_host()->coop_access_report_manager() ==
             this);

  if (!coop_reporter_.get())
    send_reports = false;

  // TODO(arthursonzogni): DCHECK same browsing context group.
  // TODO(arthursonzogni): DCHECK different virtual browsing context group.

  // Accesses are made either from the main frame or its same-origin iframes.
  // Accesses from the cross-origin ones aren't reported.
  //
  // It means all the accessed from the first window are made from documents
  // inside the same SiteInstance. Only one SiteInstance has to be updated.

  SiteInstance* site_instance =
      accessing_node->current_frame_host()->GetSiteInstance();

  absl::optional<blink::FrameToken> accessed_window_token =
      GetFrameToken(accessed_node, site_instance);
  if (!accessed_window_token)
    return;

  bool access_from_coop_page =
      this ==
      accessing_node->current_frame_host()->coop_access_report_manager();

  accessing_node->current_frame_host()
      ->GetAssociatedLocalMainFrame()
      ->InstallCoopAccessMonitor(
          *accessed_window_token, register_metrics,
          send_reports
              ? coop_reporter_->CreateReporterParams(
                    access_from_coop_page, accessing_node, accessed_node)
              : nullptr);
}

// static
int CrossOriginOpenerPolicyAccessReportManager::
    NextVirtualBrowsingContextGroup() {
  static int id = -1;
  return ++id;
}

}  // namespace content
