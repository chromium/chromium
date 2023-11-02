// Copyright 2021 The Chromium Authors
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

absl::optional<blink::FrameToken> GetFrameToken(
    FrameTreeNode* frame,
    SiteInstanceGroup* site_instance_group) {
  RenderFrameHostImpl* rfh = frame->current_frame_host();
  if (rfh->GetSiteInstance()->group() == site_instance_group)
    return rfh->GetFrameToken();

  RenderFrameProxyHost* proxy =
      rfh->browsing_context_state()->GetRenderFrameProxyHost(
          site_instance_group);
  if (proxy)
    return proxy->GetFrameToken();

  return absl::nullopt;
}

// Find all the related windows that might try to access the new document in
// |frame|, but are in a different virtual browsing context group.
std::vector<FrameTreeNode*> CollectOtherWindowForCoopAccess(
    FrameTreeNode* frame) {
  DCHECK(frame->IsMainFrame());
  int virtual_browsing_context_group =
      frame->current_frame_host()->virtual_browsing_context_group();

  std::vector<FrameTreeNode*> out;
  for (RenderFrameHostImpl* rfh :
       frame->current_frame_host()
           ->delegate()
           ->GetActiveTopLevelDocumentsInBrowsingContextGroup(
               frame->current_frame_host())) {
    // Filter out windows from the same virtual browsing context group.
    if (rfh->virtual_browsing_context_group() == virtual_browsing_context_group)
      continue;

    out.push_back(rfh->frame_tree_node());
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
  std::vector<FrameTreeNode*> other_main_frames =
      CollectOtherWindowForCoopAccess(frame);

  // Fenced frame roots are in their own browsing instance and shouldn't be
  // joined with any other main frames.
  DCHECK(!frame->IsInFencedFrameTree() || other_main_frames.empty());

  CrossOriginOpenerPolicyAccessReportManager* access_manager_frame =
      frame->current_frame_host()->coop_access_report_manager();

  for (FrameTreeNode* other : other_main_frames) {
    CrossOriginOpenerPolicyAccessReportManager* access_manager_other =
        other->current_frame_host()->coop_access_report_manager();

    // If the current frame has a reporter, install the access monitors to
    // monitor the accesses between this frame and the other frame.
    access_manager_frame->MonitorAccesses(frame, other);
    access_manager_frame->MonitorAccesses(other, frame);

    // If the other frame has a reporter, install the access monitors to monitor
    // the accesses between this frame and the other frame.
    access_manager_other->MonitorAccesses(frame, other);
    access_manager_other->MonitorAccesses(other, frame);
  }
}

void CrossOriginOpenerPolicyAccessReportManager::MonitorAccesses(
    FrameTreeNode* accessing_node,
    FrameTreeNode* accessed_node) {
  DCHECK_NE(accessing_node, accessed_node);
  DCHECK(accessing_node->current_frame_host()->coop_access_report_manager() ==
             this ||
         accessed_node->current_frame_host()->coop_access_report_manager() ==
             this);
  if (!coop_reporter_.get())
    return;

  // TODO(arthursonzogni): DCHECK same browsing context group.
  // TODO(arthursonzogni): DCHECK different virtual browsing context group.

  // Accesses are made either from the main frame or its same-origin iframes.
  // Accesses from the cross-origin ones aren't reported.
  //
  // It means all the accessed from the first window are made from documents
  // inside the same SiteInstance. Only one SiteInstance has to be updated.

  SiteInstanceGroup* site_instance_group =
      accessing_node->current_frame_host()->GetSiteInstance()->group();

  absl::optional<blink::FrameToken> accessed_window_token =
      GetFrameToken(accessed_node, site_instance_group);
  if (!accessed_window_token)
    return;

  bool access_from_coop_page =
      this ==
      accessing_node->current_frame_host()->coop_access_report_manager();

  accessing_node->current_frame_host()
      ->GetAssociatedLocalMainFrame()
      ->InstallCoopAccessMonitor(
          *accessed_window_token,
          coop_reporter_->CreateReporterParams(access_from_coop_page,
                                               accessing_node, accessed_node));
}

// static
int CrossOriginOpenerPolicyAccessReportManager::
    NextVirtualBrowsingContextGroup() {
  static int id = -1;
  return ++id;
}

}  // namespace content
