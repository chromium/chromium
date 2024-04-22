// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/security/coop/cross_origin_opener_policy_access_report_manager.h"

#include <utility>

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

// A map of virtual browsing context groups to the coop related groups they
// belong to. This mimics the real behavior of BrowsingInstance and
// CoopRelatedGroup. It is useful to restrict access in a more granular manner
// and to account for browsing context group reuse.
std::map<int, int>& GetVirtualBrowsingContextGroupToCoopRelatedGroupMap() {
  static auto& bcg_to_coop_group_map = *new std::map<int, int>();
  return bcg_to_coop_group_map;
}

std::optional<blink::FrameToken> GetFrameToken(
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

  return std::nullopt;
}

// Find all the related windows that might try to access the new document in
// `frame`, but are in a different virtual browsing context group. The
// associated boolean indicates whether they are in the same virtual
// CoopRelatedGroup.
std::vector<std::pair<FrameTreeNode*, bool>> CollectOtherWindowForCoopAccess(
    FrameTreeNode* frame) {
  DCHECK(frame->IsMainFrame());
  std::map<int, int>& bcg_to_coop_group_map =
      GetVirtualBrowsingContextGroupToCoopRelatedGroupMap();
  int virtual_browsing_context_group =
      frame->current_frame_host()->virtual_browsing_context_group();

  std::vector<std::pair<FrameTreeNode*, bool>> out;
  for (RenderFrameHostImpl* rfh :
       frame->current_frame_host()
           ->delegate()
           ->GetActiveTopLevelDocumentsInCoopRelatedGroup(
               frame->current_frame_host())) {
    // Filter out windows from the same virtual browsing context group.
    if (rfh->virtual_browsing_context_group() ==
        virtual_browsing_context_group) {
      continue;
    }

    // Every virtual browsing context group should have an associated virtual
    // CoopRelatedGroup.
    CHECK(bcg_to_coop_group_map.find(rfh->virtual_browsing_context_group()) !=
          bcg_to_coop_group_map.end());
    CHECK(bcg_to_coop_group_map.find(virtual_browsing_context_group) !=
          bcg_to_coop_group_map.end());

    bool is_in_same_virtual_coop_related_group =
        bcg_to_coop_group_map[rfh->virtual_browsing_context_group()] ==
        bcg_to_coop_group_map[virtual_browsing_context_group];
    out.emplace_back(rfh->frame_tree_node(),
                     is_in_same_virtual_coop_related_group);
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
  std::vector<std::pair<FrameTreeNode*, bool>> other_main_frames =
      CollectOtherWindowForCoopAccess(frame);

  // Fenced frame roots are in their own browsing context group in a separate
  // coop related group and shouldn't be joined with any other main frames.
  DCHECK(!frame->IsInFencedFrameTree() || other_main_frames.empty());

  CrossOriginOpenerPolicyAccessReportManager* access_manager_frame =
      frame->current_frame_host()->coop_access_report_manager();

  for (const std::pair<FrameTreeNode*, bool>& other : other_main_frames) {
    FrameTreeNode* other_ftn = other.first;
    bool is_in_same_virtual_coop_related_group = other.second;
    CrossOriginOpenerPolicyAccessReportManager* access_manager_other =
        other_ftn->current_frame_host()->coop_access_report_manager();

    // If the current frame has a reporter, install the access monitors to
    // monitor the accesses between this frame and the other frame.
    access_manager_frame->MonitorAccesses(
        frame, other_ftn, is_in_same_virtual_coop_related_group);
    access_manager_frame->MonitorAccesses(
        other_ftn, frame, is_in_same_virtual_coop_related_group);

    // If the other frame has a reporter, install the access monitors to monitor
    // the accesses between this frame and the other frame.
    access_manager_other->MonitorAccesses(
        frame, other_ftn, is_in_same_virtual_coop_related_group);
    access_manager_other->MonitorAccesses(
        other_ftn, frame, is_in_same_virtual_coop_related_group);
  }
}

void CrossOriginOpenerPolicyAccessReportManager::MonitorAccesses(
    FrameTreeNode* accessing_node,
    FrameTreeNode* accessed_node,
    bool is_in_same_virtual_coop_related_group) {
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

  std::optional<blink::FrameToken> accessed_window_token =
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
                                               accessing_node, accessed_node),
          is_in_same_virtual_coop_related_group);
}

// static
int CrossOriginOpenerPolicyAccessReportManager::
    GetNewVirtualBrowsingContextGroup() {
  std::map<int, int>& bcg_to_coop_group_map =
      GetVirtualBrowsingContextGroupToCoopRelatedGroupMap();

  // Assign the newly created virtual browsing context group to a new virtual
  // CoopRelatedGroup.
  int virtual_browsing_context_group_id = NextVirtualBrowsingContextGroup();
  bcg_to_coop_group_map[virtual_browsing_context_group_id] =
      NextVirtualCoopRelatedGroup();
  return virtual_browsing_context_group_id;
}

// static
int CrossOriginOpenerPolicyAccessReportManager::GetVirtualBrowsingContextGroup(
    CoopSwapResult enforce_result,
    CoopSwapResult report_only_result,
    int current_virtual_browsing_context_group) {
  // This function should only ever be called if we require a different virtual
  // browsing context group.
  CHECK(enforce_result != CoopSwapResult::kNoSwap ||
        report_only_result != CoopSwapResult::kNoSwap);

  int next_browsing_context_group_id = NextVirtualBrowsingContextGroup();
  std::map<int, int>& bcg_to_coop_group_map =
      GetVirtualBrowsingContextGroupToCoopRelatedGroupMap();

  // If a swap in a different CoopRelatedGroup would be required, simply create
  // a new virtual browsing context group in a new virtual CoopRelatedGroup.
  if (enforce_result == CoopSwapResult::kSwap ||
      report_only_result == CoopSwapResult::kSwap) {
    bcg_to_coop_group_map[next_browsing_context_group_id] =
        NextVirtualCoopRelatedGroup();
    return next_browsing_context_group_id;
  }

  // If a swap in the same CoopRelatedGroup would be required, create a new
  // virtual browsing context group in the current virtual CoopRelatedGroup.
  // TODO(crbug.com/40276687): This is not strictly correct, because
  // browsing context groups can be reused when navigating in the same
  // CoopRelatedGroup. Pass in the isolation information to make it as close
  // to reality as possible.
  int current_virtual_coop_related_group =
      bcg_to_coop_group_map[current_virtual_browsing_context_group];
  bcg_to_coop_group_map[next_browsing_context_group_id] =
      current_virtual_coop_related_group;
  return next_browsing_context_group_id;
}

// static
int CrossOriginOpenerPolicyAccessReportManager::
    NextVirtualBrowsingContextGroup() {
  static int id = -1;
  return ++id;
}

// static
int CrossOriginOpenerPolicyAccessReportManager::NextVirtualCoopRelatedGroup() {
  static int id = -1;
  return ++id;
}

}  // namespace content
