// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FRAME_NAVIGATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_FRAME_NAVIGATION_OBSERVER_H_

#include "base/run_loop.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"

namespace content {

// Helper for waiting until the navigation in a specific frame tree node (and
// all of its subframes) has completed loading.
class TestFrameNavigationObserver : public WebContentsObserver {
 public:
  // Create and register a new TestFrameNavigationObserver which will track
  // navigations performed in the frame tree node associated with |adapter|.
  // Note that RenderFrameHost associated with |frame| might be destroyed during
  // the navigation (e.g. if the content commits in a new renderer process).
  explicit TestFrameNavigationObserver(const ToRenderFrameHost& adapter);

  TestFrameNavigationObserver(const TestFrameNavigationObserver&) = delete;
  TestFrameNavigationObserver& operator=(const TestFrameNavigationObserver&) =
      delete;

  ~TestFrameNavigationObserver() override;

  ui::PageTransition transition_type() { return transition_type_.value(); }
  const GURL& last_committed_url() { return last_committed_url_; }

  // Runs a nested run loop and blocks until the full load has
  // completed.
  void Wait();

  // Runs a nested run loop and blocks until the navigation in the
  // associated FrameTreeNode has committed.
  void WaitForCommit();

  bool last_navigation_succeeded() const { return last_navigation_succeeded_; }

  net::Error last_net_error_code() const { return last_net_error_code_; }

  bool navigation_started() const { return navigation_started_; }

 private:
  // WebContentsObserver
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;

  // The id of the FrameTreeNode in which navigations are peformed.
  FrameTreeNodeId frame_tree_node_id_;

  // If true the navigation has started.
  bool navigation_started_ = false;

  // If true, the navigation has committed.
  bool has_committed_ = false;

  // If true, this object is waiting for commit only, not for the full load
  // of the document.
  bool wait_for_commit_ = false;

  // True if the last navigation succeeded.
  bool last_navigation_succeeded_ = false;

  // The net error code of the last navigation.
  net::Error last_net_error_code_ = net::OK;

  // Saved parameters from NavigationHandle.
  std::optional<ui::PageTransition> transition_type_;
  GURL last_committed_url_;

  // The RunLoop used to spin the message loop.
  base::RunLoop run_loop_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FRAME_NAVIGATION_OBSERVER_H_
