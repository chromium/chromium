// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigation_or_document_handle.h"

#include <optional>

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/frame_type.h"
#include "url/origin.h"

namespace content {

scoped_refptr<NavigationOrDocumentHandle>
NavigationOrDocumentHandle::CreateForDocument(
    GlobalRenderFrameHostId render_frame_host_id) {
  return base::WrapRefCounted(
      new NavigationOrDocumentHandle(render_frame_host_id));
}

scoped_refptr<NavigationOrDocumentHandle>
NavigationOrDocumentHandle::CreateForNavigation(
    NavigationRequest& navigation_request) {
  return base::WrapRefCounted(
      new NavigationOrDocumentHandle(navigation_request));
}

NavigationOrDocumentHandle::NavigationOrDocumentHandle(
    GlobalRenderFrameHostId render_frame_host_id) {
  auto* render_frame_host_impl =
      RenderFrameHostImpl::FromID(render_frame_host_id);
  if (render_frame_host_impl)
    render_frame_host_ = render_frame_host_impl->GetWeakPtr();
}

NavigationOrDocumentHandle::NavigationOrDocumentHandle(
    NavigationRequest& navigation_request)
    : navigation_request_(navigation_request.GetWeakPtr()) {}

NavigationOrDocumentHandle::~NavigationOrDocumentHandle() = default;

NavigationRequest* NavigationOrDocumentHandle::GetNavigationRequest() const {
  return navigation_request_ ? navigation_request_.get() : nullptr;
}

RenderFrameHost* NavigationOrDocumentHandle::GetDocument() const {
  return render_frame_host_.get();
}

WebContents* NavigationOrDocumentHandle::GetWebContents() const {
  if (auto* navigation_request = GetNavigationRequest()) {
    return WebContentsImpl::FromFrameTreeNode(
        navigation_request->frame_tree_node());
  } else if (auto* rfh = GetDocument()) {
    return WebContents::FromRenderFrameHost(rfh);
  }
  return nullptr;
}

FrameTreeNode* NavigationOrDocumentHandle::GetFrameTreeNode() const {
  if (auto* navigation_request = GetNavigationRequest()) {
    return navigation_request->frame_tree_node();
  } else if (auto* rfh = GetDocument()) {
    return FrameTreeNode::From(rfh);
  }
  return nullptr;
}

std::optional<url::Origin> NavigationOrDocumentHandle::GetTopmostFrameOrigin()
    const {
  if (auto* navigation_request = GetNavigationRequest()) {
    auto* current_rfh =
        navigation_request->frame_tree_node()->current_frame_host();
    return current_rfh->GetOutermostMainFrame()->GetLastCommittedOrigin();
  }
  if (auto* rfh = GetDocument()) {
    return rfh->GetOutermostMainFrame()->GetLastCommittedOrigin();
  }
  return std::nullopt;
}

bool NavigationOrDocumentHandle::IsInPrimaryMainFrame() const {
  auto* navigation_request = GetNavigationRequest();
  if (navigation_request)
    return navigation_request->IsInPrimaryMainFrame();
  auto* render_frame_host = GetDocument();
  if (render_frame_host)
    return render_frame_host->IsInPrimaryMainFrame();
  return false;
}

void NavigationOrDocumentHandle::OnNavigationCommitted(
    NavigationRequest& navigation_request) {
  DCHECK_EQ(navigation_request_->GetNavigationId(),
            navigation_request.GetNavigationId());
  render_frame_host_ = navigation_request.GetRenderFrameHost()->GetWeakPtr();
}

}  // namespace content
