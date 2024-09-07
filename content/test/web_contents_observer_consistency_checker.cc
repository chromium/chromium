// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_contents_observer_consistency_checker.h"
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/pending_task.h"
#include "base/strings/stringprintf.h"
#include "base/task/common/task_annotator.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"

namespace content {

namespace {

const char kWebContentsObserverConsistencyCheckerKey[] =
    "WebContentsObserverConsistencyChecker";

using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;

GlobalRoutingID GetRoutingPair(RenderFrameHost* host) {
  if (!host)
    return GlobalRoutingID(0, 0);
  return GlobalRoutingID(host->GetProcess()->GetID(), host->GetRoutingID());
}

}  // namespace

// static
void WebContentsObserverConsistencyChecker::Enable(WebContents* web_contents) {
  if (web_contents->GetUserData(&kWebContentsObserverConsistencyCheckerKey))
    return;
  web_contents->SetUserData(
      &kWebContentsObserverConsistencyCheckerKey,
      base::WrapUnique(
          new WebContentsObserverConsistencyChecker(web_contents)));
}

void WebContentsObserverConsistencyChecker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  bool frame_exists = !live_routes_.insert(routing_pair).second;
  deleted_routes_.erase(routing_pair);

  if (frame_exists) {
    CHECK(false) << "RenderFrameCreated called more than once for routing pair:"
                 << Format(render_frame_host);
  }

  CHECK(render_frame_host->GetProcess()->IsInitializedAndNotDead())
      << "RenderFrameCreated was called for a RenderFrameHost whose render "
         "process is not currently live, so there's no way for the RenderFrame "
         "to have been created.";
  CHECK(render_frame_host->IsRenderFrameLive())
      << "RenderFrameCreated called on for a RenderFrameHost that thinks it is "
         "not alive.";

  EnsureStableParentValue(render_frame_host);
  CHECK(!HasAnyChildren(render_frame_host));
  if (render_frame_host->GetParent()) {
    // It should also be a current host.
    GlobalRoutingID parent_routing_pair =
        GetRoutingPair(render_frame_host->GetParent());

    CHECK(current_hosts_.count(parent_routing_pair))
        << "RenderFrameCreated called for a RenderFrameHost whose parent was "
        << "not a current RenderFrameHost. Only the current frame should be "
        << "spawning children.";
  }
  AddInputEventObserver(render_frame_host);
}

void WebContentsObserverConsistencyChecker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  CHECK(!render_frame_host->IsRenderFrameLive())
      << "RenderFrameDeleted was called for a RenderFrameHost that is"
         "still live.";

  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  bool was_live = !!live_routes_.erase(routing_pair);
  bool was_dead_already = !deleted_routes_.insert(routing_pair).second;

  if (was_dead_already) {
    CHECK(false) << "RenderFrameDeleted called more than once for routing pair "
                 << Format(render_frame_host);
  } else if (!was_live) {
    CHECK(false) << "RenderFrameDeleted called for routing pair "
                 << Format(render_frame_host)
                 << " for which RenderFrameCreated was never called";
  }

  EnsureStableParentValue(render_frame_host);
  CHECK(!HasAnyChildren(render_frame_host));
  if (render_frame_host->GetParent())
    AssertRenderFrameExists(render_frame_host->GetParent());

  // All players should have been paused by this point.
  for (const auto& id : active_media_players_)
    CHECK_NE(RenderFrameHost::FromID(id.frame_routing_id), render_frame_host);
  RemoveInputEventObserver(render_frame_host);
}

void WebContentsObserverConsistencyChecker::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  CHECK(new_host);
  CHECK_NE(new_host, old_host);
  CHECK(GetRoutingPair(old_host) != GetRoutingPair(new_host));

  if (old_host) {
    CHECK(base::Contains(frame_tree_node_ids_, new_host->GetFrameTreeNodeId()));
    EnsureStableParentValue(old_host);
    CHECK_EQ(old_host->GetParent(), new_host->GetParent());
    GlobalRoutingID routing_pair = GetRoutingPair(old_host);
    // If the navigation requires a new RFH, IsActive on old host should be
    // false.
    CHECK(!old_host->IsActive());
    bool old_did_exist = !!current_hosts_.erase(routing_pair);
    if (!old_did_exist) {
      CHECK(false)
          << "RenderFrameHostChanged called with old host that did not exist:"
          << Format(old_host);
    }
  } else {
    CHECK(frame_tree_node_ids_.insert(new_host->GetFrameTreeNodeId()).second);
  }

  auto* new_host_impl = static_cast<RenderFrameHostImpl*>(new_host);
  CHECK(new_host_impl->lifecycle_state() == LifecycleStateImpl::kActive ||
        new_host_impl->lifecycle_state() == LifecycleStateImpl::kPrerendering);
  EnsureStableParentValue(new_host);
  if (new_host->GetParent()) {
    AssertRenderFrameExists(new_host->GetParent());
    // RenderFrameCreated should be called before RenderFrameHostChanged for all
    // the subframes except for those which are the outer delegates for:
    //  - Fenced frames based specifically on MPArch
    // This is because those special-case frames do not have live RenderFrames
    // in the renderer process.
    bool is_render_frame_created_needed_for_child =
        new_host->GetFrameOwnerElementType() !=
        blink::FrameOwnerElementType::kFencedframe;
    if (is_render_frame_created_needed_for_child) {
      AssertRenderFrameExists(new_host);
    }
    CHECK(current_hosts_.count(GetRoutingPair(new_host->GetParent())))
        << "Parent of frame being committed must be current.";
  }

  GlobalRoutingID routing_pair = GetRoutingPair(new_host);
  current_hosts_.insert(routing_pair);
}

void WebContentsObserverConsistencyChecker::FrameDeleted(
    FrameTreeNodeId frame_tree_node_id) {
  // A frame can be deleted before RenderFrame in the renderer process is
  // created, so there is not much that can be enforced here.
  CHECK(!web_contents_destroyed_);

  CHECK(frame_tree_node_ids_.erase(frame_tree_node_id));

  RenderFrameHostImpl* render_frame_host =
      FrameTreeNode::GloballyFindByID(frame_tree_node_id)->current_frame_host();

  // Will be nullptr if this is main frame of a non primary FrameTree whose page
  // was moved out (e.g. due Prerender activation).
  if (!render_frame_host) {
    DCHECK(!FrameTreeNode::GloballyFindByID(frame_tree_node_id)
                ->frame_tree()
                .is_primary());
    return;
  }

  EnsureStableParentValue(render_frame_host);

  CHECK(!HasAnyChildren(render_frame_host))
      << "All children should be deleted before a frame is detached.";

  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  CHECK(current_hosts_.erase(routing_pair))
      << "FrameDeleted called with a non-current RenderFrameHost.";

  if (render_frame_host->GetParent())
    AssertRenderFrameExists(render_frame_host->GetParent());
}

void WebContentsObserverConsistencyChecker::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(!NavigationIsOngoing(navigation_handle));

  // Prerendered page activation should run subsequent navigation events in the
  // same task.
  if (navigation_handle->IsPrerenderedPageActivation())
    task_checker_for_prerendered_page_activation_.BindCurrentTask();

  CHECK(!navigation_handle->HasCommitted());
  CHECK(!navigation_handle->IsErrorPage());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());

  ongoing_navigations_.insert(navigation_handle);
}

void WebContentsObserverConsistencyChecker::DidRedirectNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  // DidRedirectionNavigation() should not be called for page activation.
  CHECK(!navigation_handle->IsServedFromBackForwardCache());
  CHECK(!navigation_handle->IsPrerenderedPageActivation());

  CHECK(navigation_handle->GetNetErrorCode() == net::OK);
  CHECK(!navigation_handle->HasCommitted());
  CHECK(!navigation_handle->IsErrorPage());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
}

void WebContentsObserverConsistencyChecker::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  // Prerendered page activation should run navigation events in the same task.
  if (navigation_handle->IsPrerenderedPageActivation()) {
    CHECK(task_checker_for_prerendered_page_activation_.IsRunningInSameTask());
  }

  CHECK(!navigation_handle->HasCommitted());
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());
  CHECK(navigation_handle->GetRenderFrameHost());
  CHECK(navigation_handle->GetRenderFrameHost()->IsRenderFrameLive());

  ready_to_commit_hosts_.insert(
      std::make_pair(navigation_handle->GetNavigationId(),
                     navigation_handle->GetRenderFrameHost()));
}

void WebContentsObserverConsistencyChecker::PrimaryPageChanged(Page& page) {
  CHECK_EQ(&web_contents()->GetPrimaryPage(), &page)
      << "PrimaryPageChanged invoked on non-primary page.";
}

void WebContentsObserverConsistencyChecker::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  CHECK(NavigationIsOngoing(navigation_handle));

  // Prerendered page activation should run navigation events in the same task.
  if (navigation_handle->IsPrerenderedPageActivation()) {
    CHECK(task_checker_for_prerendered_page_activation_.IsRunningInSameTask());
  }

  CHECK(!(navigation_handle->HasCommitted() &&
          !navigation_handle->IsErrorPage()) ||
        navigation_handle->GetNetErrorCode() == net::OK);
  CHECK_EQ(navigation_handle->GetWebContents(), web_contents());

  CHECK(!navigation_handle->HasCommitted() ||
        navigation_handle->GetRenderFrameHost());

  if (navigation_handle->HasCommitted()) {
    RenderFrameHostImpl* new_rfh = static_cast<RenderFrameHostImpl*>(
        navigation_handle->GetRenderFrameHost());
    CHECK(new_rfh->lifecycle_state() == LifecycleStateImpl::kActive ||
          new_rfh->lifecycle_state() == LifecycleStateImpl::kPrerendering);
  }

  CHECK(!navigation_handle->HasCommitted() ||
        navigation_handle->GetRenderFrameHost()->IsRenderFrameLive());

  // If ReadyToCommitNavigation was dispatched, verify that the
  // |navigation_handle| has the same RenderFrameHost at this time as the one
  // returned at ReadyToCommitNavigation.
  if (base::Contains(ready_to_commit_hosts_,
                     navigation_handle->GetNavigationId())) {
    CHECK_EQ(ready_to_commit_hosts_[navigation_handle->GetNavigationId()],
             navigation_handle->GetRenderFrameHost());
    ready_to_commit_hosts_.erase(navigation_handle->GetNavigationId());
  }

  ongoing_navigations_.erase(navigation_handle);
}

void WebContentsObserverConsistencyChecker::
    PrimaryMainDocumentElementAvailable() {
  AssertMainFrameExists();
}

void WebContentsObserverConsistencyChecker::
    DocumentOnLoadCompletedInPrimaryMainFrame() {
  CHECK(static_cast<PageImpl&>(web_contents()->GetPrimaryPage())
            .is_on_load_completed_in_main_document());
  AssertMainFrameExists();
}

void WebContentsObserverConsistencyChecker::DOMContentLoaded(
    RenderFrameHost* render_frame_host) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverConsistencyChecker::DidFinishLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverConsistencyChecker::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  AssertRenderFrameExists(render_frame_host);
}

void WebContentsObserverConsistencyChecker::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  AssertRenderFrameExists(source_render_frame_host);
}

void WebContentsObserverConsistencyChecker::MediaStartedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id) {
  CHECK(!web_contents_destroyed_);
  CHECK(!base::Contains(active_media_players_, id));
  active_media_players_.push_back(id);
}

void WebContentsObserverConsistencyChecker::MediaStoppedPlaying(
    const MediaPlayerInfo& media_info,
    const MediaPlayerId& id,
    WebContentsObserver::MediaStoppedReason reason) {
  CHECK(!web_contents_destroyed_);
  CHECK(base::Contains(active_media_players_, id));
  std::erase(active_media_players_, id);
}

bool WebContentsObserverConsistencyChecker::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host->IsRenderFrameLive());

  AssertRenderFrameExists(render_frame_host);
  return false;
}

void WebContentsObserverConsistencyChecker::WebContentsDestroyed() {
  CHECK(!web_contents_destroyed_);
  web_contents_destroyed_ = true;
  CHECK(ongoing_navigations_.empty());
  CHECK(active_media_players_.empty());
  CHECK(live_routes_.empty());
  CHECK(frame_tree_node_ids_.empty());
}

void WebContentsObserverConsistencyChecker::DidStartLoading() {
  // TODO(clamy): add checks for the loading state in the rest of observer
  // methods.
  // TODO(crbug.com/40155922): Add back CHECK(!is_loading_). The CHECK was
  // removed because of flaky failures during some browser_tests.
  CHECK(web_contents()->IsLoading());
  is_loading_ = true;
}

void WebContentsObserverConsistencyChecker::DidStopLoading() {
  // TODO(crbug.com/40409075): Add back CHECK(is_loading_). The CHECK was
  // removed because of flaky failures during browser_test shutdown.
  CHECK(!web_contents()->IsLoading());
  is_loading_ = false;
}

WebContentsObserverConsistencyChecker::WebContentsObserverConsistencyChecker(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      is_loading_(false),
      web_contents_destroyed_(false) {}

WebContentsObserverConsistencyChecker::
    ~WebContentsObserverConsistencyChecker() {
  CHECK(web_contents_destroyed_);
  CHECK(ready_to_commit_hosts_.empty());
}

void WebContentsObserverConsistencyChecker::AssertRenderFrameExists(
    RenderFrameHost* render_frame_host) {
  CHECK(!web_contents_destroyed_);
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);

  bool render_frame_created_happened = live_routes_.count(routing_pair) != 0;
  bool render_frame_deleted_happened = deleted_routes_.count(routing_pair) != 0;

  CHECK(render_frame_created_happened)
      << "A RenderFrameHost pointer was passed to a WebContentsObserver "
      << "method, but WebContentsObserver::RenderFrameCreated was never called "
      << "for that RenderFrameHost: " << Format(render_frame_host);
  CHECK(!render_frame_deleted_happened)
      << "A RenderFrameHost pointer was passed to a WebContentsObserver "
      << "method, but WebContentsObserver::RenderFrameDeleted had already been "
      << "called on that frame:" << Format(render_frame_host);
}

void WebContentsObserverConsistencyChecker::AssertMainFrameExists() {
  AssertRenderFrameExists(web_contents()->GetPrimaryMainFrame());
}

std::string WebContentsObserverConsistencyChecker::Format(
    RenderFrameHost* render_frame_host) {
  return base::StringPrintf(
      "(%d, %d -> %s)", render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(),
      render_frame_host->GetSiteInstance()->GetSiteURL().spec().c_str());
}

bool WebContentsObserverConsistencyChecker::NavigationIsOngoing(
    NavigationHandle* navigation_handle) {
  return base::Contains(ongoing_navigations_, navigation_handle);
}

void WebContentsObserverConsistencyChecker::EnsureStableParentValue(
    RenderFrameHost* render_frame_host) {
  GlobalRoutingID routing_pair = GetRoutingPair(render_frame_host);
  GlobalRoutingID parent_routing_pair =
      GetRoutingPair(render_frame_host->GetParent());

  auto it = parent_ids_.find(routing_pair);
  if (it == parent_ids_.end()) {
    parent_ids_.insert(std::make_pair(routing_pair, parent_routing_pair));
  } else {
    GlobalRoutingID former_parent_routing_pair = it->second;
    CHECK_EQ(former_parent_routing_pair, parent_routing_pair)
        << "RFH's parent value changed over time! That is really not good!";
  }
}

bool WebContentsObserverConsistencyChecker::HasAnyChildren(
    RenderFrameHost* parent) {
  GlobalRoutingID parent_routing_pair = GetRoutingPair(parent);
  for (auto& entry : parent_ids_) {
    if (entry.second == parent_routing_pair) {
      if (live_routes_.count(entry.first))
        return true;
      if (current_hosts_.count(entry.first))
        return true;
    }
  }
  return false;
}

class WebContentsObserverConsistencyChecker::TestInputEventObserver
    : public RenderWidgetHost::InputEventObserver {
 public:
  explicit TestInputEventObserver(RenderFrameHost& render_frame_host)
      : render_frame_host_wrapper_(&render_frame_host),
        render_widget_host_(static_cast<RenderWidgetHostImpl*>(
                                render_frame_host.GetRenderWidgetHost())
                                ->GetWeakPtr()) {
    render_widget_host_->AddInputEventObserver(this);
  }
  ~TestInputEventObserver() override {
    if (render_widget_host_)
      render_widget_host_->RemoveInputEventObserver(this);
  }

  void OnInputEvent(const blink::WebInputEvent&) override {
    EnsureRenderFrameHostNotPrerendered();
  }
  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override {
    EnsureRenderFrameHostNotPrerendered();
  }

 private:
  void EnsureRenderFrameHostNotPrerendered() {
    if (render_frame_host_wrapper_.IsDestroyed())
      return;

    CHECK_NE(static_cast<RenderFrameHostImpl*>(render_frame_host_wrapper_.get())
                 ->lifecycle_state(),
             RenderFrameHostImpl::LifecycleStateImpl::kPrerendering);
  }

  RenderFrameHostWrapper render_frame_host_wrapper_;
  base::WeakPtr<RenderWidgetHostImpl> render_widget_host_;
};

void WebContentsObserverConsistencyChecker::AddInputEventObserver(
    RenderFrameHost* render_frame_host) {
  auto result = input_observer_map_.insert(std::make_pair(
      render_frame_host,
      std::make_unique<TestInputEventObserver>(*render_frame_host)));
  CHECK(result.second);
}

void WebContentsObserverConsistencyChecker::RemoveInputEventObserver(
    RenderFrameHost* render_frame_host) {
  DCHECK(base::Contains(input_observer_map_, render_frame_host));
  input_observer_map_.erase(render_frame_host);
}

WebContentsObserverConsistencyChecker::TaskChecker::TaskChecker()
    : sequence_num_(GetSequenceNumberOfCurrentTask()) {}

void WebContentsObserverConsistencyChecker::TaskChecker::BindCurrentTask() {
  sequence_num_ = GetSequenceNumberOfCurrentTask();
}

bool WebContentsObserverConsistencyChecker::TaskChecker::IsRunningInSameTask() {
  return sequence_num_ == GetSequenceNumberOfCurrentTask();
}

std::optional<int> WebContentsObserverConsistencyChecker::TaskChecker::
    GetSequenceNumberOfCurrentTask() {
  return base::TaskAnnotator::CurrentTaskForThread()
             ? std::make_optional(
                   base::TaskAnnotator::CurrentTaskForThread()->sequence_num)
             : std::nullopt;
}

}  // namespace content
