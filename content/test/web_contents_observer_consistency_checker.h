// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEB_CONTENTS_OBSERVER_CONSISTENCY_CHECKER_H_
#define CONTENT_TEST_WEB_CONTENTS_OBSERVER_CONSISTENCY_CHECKER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

// If your test framework enables a ContentBrowserConsistencyChecker, this
// consistency checker is automatically installed on all WebContentses during
// your test.
//
// WebContentsObserverConsistencyChecker is a WebContentsObserver that checks
// the consistency of observer calls, and CHECK()s if they are inconsistent.
// These checks are test-only code designed to find bugs in the implementation
// of the content layer by validating the contract between WebContents and its
// observers.
//
// For example, WebContentsObserver::RenderFrameCreated announces the existence
// of a new RenderFrameHost, so that method call must occur before the
// RenderFrameHost is referenced by some other WebContentsObserver method.
class WebContentsObserverConsistencyChecker
    : public WebContentsObserver,
      public base::SupportsUserData::Data {
 public:
  WebContentsObserverConsistencyChecker(
      const WebContentsObserverConsistencyChecker&) = delete;
  WebContentsObserverConsistencyChecker& operator=(
      const WebContentsObserverConsistencyChecker&) = delete;

  ~WebContentsObserverConsistencyChecker() override;

  // Enables these checks on |web_contents|. Usually
  // ContentBrowserConsistencyChecker should call this for you.
  static void Enable(WebContents* web_contents);

  // WebContentsObserver implementation.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void FrameDeleted(FrameTreeNodeId frame_tree_node_id) override;
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(Page& page) override;
  void PrimaryMainDocumentElementAvailable() override;
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;
  void DOMContentLoaded(RenderFrameHost* render_frame_host) override;
  void DidFinishLoad(RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void DidOpenRequestedURL(WebContents* new_contents,
                           RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId& id) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  void DidStartLoading() override;
  void DidStopLoading() override;

 private:
  class TestInputEventObserver;

  explicit WebContentsObserverConsistencyChecker(WebContents* web_contents);

  std::string Format(RenderFrameHost* render_frame_host);
  void AssertRenderFrameExists(RenderFrameHost* render_frame_host);
  void AssertMainFrameExists();

  bool NavigationIsOngoing(NavigationHandle* navigation_handle);

  void EnsureStableParentValue(RenderFrameHost* render_frame_host);
  bool HasAnyChildren(RenderFrameHost* render_frame_host);

  void AddInputEventObserver(RenderFrameHost* render_frame_host);
  void RemoveInputEventObserver(RenderFrameHost* render_frame_host);

  std::map<int64_t, raw_ptr<RenderFrameHost, CtnExperimental>>
      ready_to_commit_hosts_;
  std::set<GlobalRoutingID> current_hosts_;
  std::set<GlobalRoutingID> live_routes_;
  std::set<GlobalRoutingID> deleted_routes_;

  std::set<raw_ptr<NavigationHandle, SetExperimental>> ongoing_navigations_;
  std::vector<MediaPlayerId> active_media_players_;

  std::map<RenderFrameHost*, std::unique_ptr<TestInputEventObserver>>
      input_observer_map_;

  // Used for checking if observer calls for navigation run in the same task.
  class TaskChecker {
   public:
    TaskChecker();

    void BindCurrentTask();

    // Returns true if the current task is the same as the task bound by
    // BindCurrentTask().
    bool IsRunningInSameTask();

   private:
    std::optional<int> GetSequenceNumberOfCurrentTask();

    // In some tests, the current task is not set. In that case, `sequence_num`
    // is std::nullopt.
    std::optional<int> sequence_num_;
  };
  TaskChecker task_checker_for_prerendered_page_activation_;

  // Remembers parents to make sure RenderFrameHost::GetParent() never changes.
  std::map<GlobalRoutingID, GlobalRoutingID> parent_ids_;

  std::set<FrameTreeNodeId> frame_tree_node_ids_;

  bool is_loading_;

  bool web_contents_destroyed_;
};

}  // namespace content

#endif  // CONTENT_TEST_WEB_CONTENTS_OBSERVER_CONSISTENCY_CHECKER_H_
