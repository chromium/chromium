// Copyright 2017 The Chromium Authors
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
  response_headers_.reset();

  is_main_frame_ = navigation_handle->IsInMainFrame();
  is_renderer_initiated_ = navigation_handle->IsRendererInitiated();
  is_same_document_ = navigation_handle->IsSameDocument();
  was_redirected_ = navigation_handle->WasServerRedirect();
  frame_tree_node_id_ = navigation_handle->GetFrameTreeNodeId();
  navigation_id_ = navigation_handle->GetNavigationId();
  navigation_start_ = navigation_handle->NavigationStart();
  reload_type_ = navigation_handle->GetReloadType();
  next_page_ukm_source_id_ = navigation_handle->GetNextPageUkmSourceId();
}

void NavigationHandleObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle != handle_)
    return;

  DCHECK_EQ(is_main_frame_, navigation_handle->IsInMainFrame());
  DCHECK_EQ(is_same_document_, navigation_handle->IsSameDocument());
  DCHECK_EQ(is_renderer_initiated_, navigation_handle->IsRendererInitiated());
  DCHECK_EQ(frame_tree_node_id_, navigation_handle->GetFrameTreeNodeId());

  was_redirected_ = navigation_handle->WasServerRedirect();
  net_error_code_ = navigation_handle->GetNetErrorCode();
  is_download_ = navigation_handle->IsDownload();
  auth_challenge_info_ = navigation_handle->GetAuthChallengeInfo();
  resolve_error_info_ = navigation_handle->GetResolveErrorInfo();

  if (navigation_handle->HasCommitted()) {
    has_committed_ = true;
    if (!navigation_handle->IsErrorPage()) {
      page_transition_ = navigation_handle->GetPageTransition();
      last_committed_url_ = navigation_handle->GetURL();
      response_headers_ = navigation_handle->GetResponseHeaders();
    } else {
      is_error_ = true;
    }
  } else {
    has_committed_ = false;
    is_error_ = true;
  }

  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();

  handle_ = nullptr;
  content_settings_ = navigation_handle->GetContentSettingsForTesting();
}

std::string NavigationHandleObserver::GetNormalizedResponseHeader(
    const std::string& key) const {
  std::string value;
  response_headers_->GetNormalizedHeader(key, &value);
  return value;
}

}  // namespace content
