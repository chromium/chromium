// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_handle_observer.h"

#include "content/public/browser/navigation_handle.h"

namespace content {

NavigationHandleObserver::NavigationHandleObserver(
    WebContents* web_contents,
    const GURL& expected_start_url)
    : WebContentsObserver(web_contents),
      page_transition_(ui::PAGE_TRANSITION_LINK),
      expected_start_url_(expected_start_url) {}

NavigationHandleObserver::~NavigationHandleObserver() {}

void NavigationHandleObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (handle_ || navigation_handle->GetURL() != expected_start_url_)
    return;

  handle_ = navigation_handle;
  has_committed_ = false;
  is_error_ = false;
  page_transition_ = ui::PAGE_TRANSITION_LINK;
  last_committed_url_ = GURL();

  is_main_frame_ = navigation_handle->IsInMainFrame();
  is_parent_main_frame_ = navigation_handle->IsParentMainFrame();
  is_renderer_initiated_ = navigation_handle->IsRendererInitiated();
  is_same_document_ = navigation_handle->IsSameDocument();
  was_redirected_ = navigation_handle->WasServerRedirect();
  frame_tree_node_id_ = navigation_handle->GetFrameTreeNodeId();
  navigation_id_ = navigation_handle->GetNavigationId();
  navigation_start_ = navigation_handle->NavigationStart();
}

void NavigationHandleObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle != handle_)
    return;

  DCHECK_EQ(is_main_frame_, navigation_handle->IsInMainFrame());
  DCHECK_EQ(is_parent_main_frame_, navigation_handle->IsParentMainFrame());
  DCHECK_EQ(is_same_document_, navigation_handle->IsSameDocument());
  DCHECK_EQ(is_renderer_initiated_, navigation_handle->IsRendererInitiated());
  DCHECK_EQ(frame_tree_node_id_, navigation_handle->GetFrameTreeNodeId());

  was_redirected_ = navigation_handle->WasServerRedirect();
  net_error_code_ = navigation_handle->GetNetErrorCode();
  is_download_ = navigation_handle->IsDownload();
  auth_challenge_info_ = navigation_handle->GetAuthChallengeInfo();

  if (navigation_handle->HasCommitted()) {
    has_committed_ = true;
    if (!navigation_handle->IsErrorPage()) {
      page_transition_ = navigation_handle->GetPageTransition();
      last_committed_url_ = navigation_handle->GetURL();
    } else {
      is_error_ = true;
    }
  } else {
    has_committed_ = false;
    is_error_ = true;
  }

  handle_ = nullptr;
}

}  // namespace content
