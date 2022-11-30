// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_frame_navigation_observer.h"

#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

namespace {

RenderFrameHostImpl* ToRenderFrameHostImpl(const ToRenderFrameHost& frame) {
  return static_cast<RenderFrameHostImpl*>(frame.render_frame_host());
}

}  // namespace

TestFrameNavigationObserver::TestFrameNavigationObserver(
    const ToRenderFrameHost& adapter)
    : WebContentsObserver(
          WebContents::FromRenderFrameHost(ToRenderFrameHostImpl(adapter))),
      frame_tree_node_id_(
          ToRenderFrameHostImpl(adapter)->GetFrameTreeNodeId()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

TestFrameNavigationObserver::~TestFrameNavigationObserver() {}

void TestFrameNavigationObserver::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  wait_for_commit_ = false;
  run_loop_.Run();
}

void TestFrameNavigationObserver::WaitForCommit() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (has_committed_)
    return;

  wait_for_commit_ = true;
  run_loop_.Run();
}

void TestFrameNavigationObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  last_navigation_succeeded_ = false;
  if (navigation_handle->GetFrameTreeNodeId() == frame_tree_node_id_) {
    navigation_started_ = true;
    has_committed_ = false;
  }
}

void TestFrameNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_started_)
    return;

  last_navigation_succeeded_ = !navigation_handle->IsErrorPage();
  last_net_error_code_ = navigation_handle->GetNetErrorCode();
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage() ||
      navigation_handle->GetFrameTreeNodeId() != frame_tree_node_id_) {
    return;
  }

  transition_type_ = navigation_handle->GetPageTransition();
  last_committed_url_ = navigation_handle->GetURL();

  has_committed_ = true;
  if (wait_for_commit_)
    run_loop_.Quit();
}

void TestFrameNavigationObserver::DidStopLoading() {
  if (!navigation_started_)
    return;

  navigation_started_ = false;
  run_loop_.Quit();
}

}  // namespace content
