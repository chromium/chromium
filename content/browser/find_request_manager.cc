// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/find_request_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/browser/find_in_page_client.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

// The following functions allow traversal over all RenderFrameHosts, including
// those across WebContentses.
//
// An inner WebContents may be embedded in an outer WebContents via an inner
// WebContentsTreeNode of the outer WebContents's WebContentsTreeNode.
std::vector<RenderFrameHostImpl*> GetChildren(RenderFrameHostImpl* rfh) {
  std::vector<RenderFrameHostImpl*> children;
  children.reserve(rfh->child_count());
  for (size_t i = 0; i != rfh->child_count(); ++i) {
    if (auto* contents = static_cast<WebContentsImpl*>(
            WebContentsImpl::FromOuterFrameTreeNode(rfh->child_at(i)))) {
      // If the child is used for an inner WebContents then add the inner
      // WebContents.
      children.push_back(
          contents->GetPrimaryFrameTree().root()->current_frame_host());
    } else {
      children.push_back(rfh->child_at(i)->current_frame_host());
    }
  }

  return children;
}

// Returns the first child RenderFrameHostImpl under |rfh|, if |rfh| has a
// child, or nullptr otherwise.
RenderFrameHostImpl* GetFirstChild(RenderFrameHostImpl* rfh) {
  auto children = GetChildren(rfh);
  if (!children.empty())
    return children.front();
  return nullptr;
}

// Returns the last child RenderFrameHostImpl under |rfh|, if |rfh| has a
// child, or nullptr otherwise.
RenderFrameHostImpl* GetLastChild(RenderFrameHostImpl* rfh) {
  auto children = GetChildren(rfh);
  if (!children.empty())
    return children.back();
  return nullptr;
}

// Returns the deepest last child frame under |rfh| in the frame tree.
RenderFrameHostImpl* GetDeepestLastChild(RenderFrameHostImpl* rfh) {
  while (RenderFrameHostImpl* last_child = GetLastChild(rfh))
    rfh = last_child;
  return rfh;
}

// Returns the parent RenderFrameHost of |rfh|, if |rfh| has a parent, or
// nullptr otherwise.
RenderFrameHostImpl* GetAncestor(RenderFrameHostImpl* rfh) {
  if (!rfh)
    return nullptr;

  return rfh->GetParentOrOuterDocumentOrEmbedder();
}

// Returns the previous sibling RenderFrameHostImpl of |rfh|, if one exists,
// or nullptr otherwise.
RenderFrameHostImpl* GetPreviousSibling(RenderFrameHostImpl* rfh) {
  if (rfh->PreviousSibling()) {
    return rfh->PreviousSibling()->current_frame_host();
  }

  // The previous sibling may be in another WebContents.
  if (RenderFrameHostImpl* parent = GetAncestor(rfh)) {
    auto children = GetChildren(parent);
    auto it = base::ranges::find(children, rfh);
    // It is odd that this rfh may not be a child of its parent, but this is
    // actually possible during teardown, hence the need for the check for
    // "it != children.end()".
    if (it != children.end() && it != children.begin())
      return *--it;
  }

  return nullptr;
}

// Returns the next sibling RenderFrameHostImpl of |rfh|, if one exists, or
// nullptr otherwise.
RenderFrameHostImpl* GetNextSibling(RenderFrameHostImpl* rfh) {
  if (rfh->NextSibling())
    return rfh->NextSibling()->current_frame_host();

  // The next sibling may be in another WebContents.
  if (RenderFrameHostImpl* parent = GetAncestor(rfh)) {
    auto children = GetChildren(parent);
    auto it = base::ranges::find(children, rfh);
    // It is odd that this RenderFrameHost may not be a child of its parent, but
    // this is actually possible during teardown, hence the need for the check
    // for "it != children.end()".
    if (it != children.end() && ++it != children.end())
      return *it;
  }

  return nullptr;
}

// Returns the RenderFrameHostImpl directly after |rfh| in the rfh tree in
// search order, or nullptr if one does not exist. If |wrap| is set, then
// wrapping between the first and last frames is permitted. Note that this
// traversal follows the same ordering as in
// blink::FrameTree::traverseNextWithWrap().
RenderFrameHostImpl* TraverseNext(RenderFrameHostImpl* rfh, bool wrap) {
  if (RenderFrameHostImpl* first_child = GetFirstChild(rfh))
    return first_child;

  RenderFrameHostImpl* sibling = GetNextSibling(rfh);
  while (!sibling) {
    RenderFrameHostImpl* parent = GetAncestor(rfh);
    if (!parent)
      return wrap ? rfh : nullptr;
    rfh = parent;
    sibling = GetNextSibling(rfh);
  }
  return sibling;
}

// Returns the RenderFrameHostImpl directly before |rfh| in the frame tree in
// search order, or nullptr if one does not exist. If |wrap| is set, then
// wrapping between the first and last frames is permitted. Note that this
// traversal follows the same ordering as in
// blink::FrameTree::traversePreviousWithWrap().
RenderFrameHostImpl* TraversePrevious(RenderFrameHostImpl* rfh, bool wrap) {
  if (RenderFrameHostImpl* previous_sibling = GetPreviousSibling(rfh))
    return GetDeepestLastChild(previous_sibling);
  if (RenderFrameHostImpl* parent = GetAncestor(rfh))
    return parent;
  return wrap ? GetDeepestLastChild(rfh) : nullptr;
}

// The same as either TraverseNext() or TraversePrevious(), depending on
// |forward|.
RenderFrameHostImpl* TraverseFrame(RenderFrameHostImpl* rfh,
                                   bool forward,
                                   bool wrap) {
  return forward ? TraverseNext(rfh, wrap) : TraversePrevious(rfh, wrap);
}

bool IsFindInPageDisabled(RenderFrameHost* rfh) {
  return rfh && GetContentClient()->browser()->IsFindInPageDisabledForOrigin(
                    rfh->GetLastCommittedOrigin());
}

bool IsUnattachedGuestView(RenderFrameHost* rfh) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(rfh));
  if (!web_contents->IsGuest())
    return false;

  return !web_contents->GetOuterWebContents();
}

// kMinKeystrokesWithoutDelay should be high enough that script in the page
// can't provide every possible search result at the same time.
constexpr int kMinKeystrokesWithoutDelay = 4;

// The delay for very short queries, before sending find requests. This should
// be higher than the duration in between two keystrokes. This is based on
// WebCore.FindInPage.DurationBetweenKeystrokes metrics, this is higher than
// 90% of them.
constexpr int kDelayMs = 400;

}  // namespace

// Observes searched WebContentses for RenderFrameHost state updates, including
// deletion and loads.
class FindRequestManager::FrameObserver : public WebContentsObserver {
 public:
  FrameObserver(WebContents* web_contents, FindRequestManager* manager)
      : WebContentsObserver(web_contents), manager_(manager) {}

  FrameObserver(const FrameObserver&) = delete;
  FrameObserver& operator=(const FrameObserver&) = delete;

  ~FrameObserver() override = default;

  void RenderFrameDeleted(RenderFrameHost* rfh) override {
    manager_->RemoveFrame(rfh);
  }

  void RenderFrameHostStateChanged(
      RenderFrameHost* rfh,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override {
    if (manager_->current_session_id_ == kInvalidId ||
        IsFindInPageDisabled(rfh)) {
      return;
    }

    if (new_state == RenderFrameHost::LifecycleState::kActive) {
      // Add the RFH to the current find-in-page session when its status
      // changes to active since this is when the document becomes part of the
      // primary page (i.e prerendered pages getting activated, or pages in
      // BackForwardCache getting restored), so that we can get the results from
      // all frames in the primary page.
      manager_->AddFrame(rfh, false /* force */);
    } else if (old_state == RenderFrameHost::LifecycleState::kActive) {
      // Remove the RFH from the current find-in-page session if it stops being
      // part of the primary page.
      manager_->RemoveFrame(rfh);
    }
  }

  void DidFinishLoad(RenderFrameHost* rfh, const GURL& validated_url) override {
    if (manager_->current_session_id_ == kInvalidId)
      return;

    manager_->RemoveFrame(rfh);
    // Make sure RenderFrameDeleted will be called to clean up
    DCHECK(rfh->IsRenderFrameLive());

    if (IsFindInPageDisabled(rfh))
      return;

    manager_->AddFrame(rfh, /*force=*/true);
  }

 private:
  // The FindRequestManager that owns this FrameObserver.
  const raw_ptr<FindRequestManager> manager_;
};

bool FindRequestManager::RunDelayedFindTaskForTesting() {
  if (!delayed_find_task_.IsCancelled()) {
    delayed_find_task_.callback().Run();
    return true;
  }
  return false;
}

FindRequestManager::FindRequest::FindRequest() = default;

FindRequestManager::FindRequest::FindRequest(
    int id,
    const std::u16string& search_text,
    blink::mojom::FindOptionsPtr options)
    : id(id), search_text(search_text), options(std::move(options)) {}

FindRequestManager::FindRequest::FindRequest(const FindRequest& request)
    : id(request.id),
      search_text(request.search_text),
      options(request.options.Clone()) {}

FindRequestManager::FindRequest::~FindRequest() = default;

FindRequestManager::FindRequest& FindRequestManager::FindRequest::operator=(
    const FindRequest& request) {
  id = request.id;
  search_text = request.search_text;
  options = request.options.Clone();
  return *this;
}

#if BUILDFLAG(IS_ANDROID)
FindRequestManager::ActivateNearestFindResultState::
ActivateNearestFindResultState() = default;
FindRequestManager::ActivateNearestFindResultState::
    ActivateNearestFindResultState(float x, float y)
    : current_request_id(GetNextID()), point(x, y) {}
FindRequestManager::ActivateNearestFindResultState::
    ~ActivateNearestFindResultState() = default;

FindRequestManager::FrameRects::FrameRects() = default;
FindRequestManager::FrameRects::FrameRects(const std::vector<gfx::RectF>& rects,
                                           int version)
    : rects(rects), version(version) {}
FindRequestManager::FrameRects::~FrameRects() = default;

FindRequestManager::FindMatchRectsState::FindMatchRectsState() = default;
FindRequestManager::FindMatchRectsState::~FindMatchRectsState() = default;
#endif

// static
const int FindRequestManager::kInvalidId = -1;

FindRequestManager::FindRequestManager(WebContentsImpl* web_contents)
    : contents_(web_contents) {}

FindRequestManager::~FindRequestManager() = default;

void FindRequestManager::Find(int request_id,
                              const std::u16string& search_text,
                              blink::mojom::FindOptionsPtr options,
                              bool skip_delay) {
  // Every find request must have a unique ID, and these IDs must strictly
  // increase so that newer requests always have greater IDs than older
  // requests.
  DCHECK_GT(request_id, current_request_.id);
  DCHECK_GT(request_id, current_session_id_);

  if (skip_delay) {
    delayed_find_task_.Cancel();
    EmitFindRequest(request_id, search_text, std::move(options));
    return;
  }

  if (!options->new_session) {
    // If the user presses enter while we are waiting for a delayed find, then
    // run the find now to improve responsiveness.
    if (!delayed_find_task_.IsCancelled()) {
      delayed_find_task_.callback().Run();
    } else {
      EmitFindRequest(request_id, search_text, std::move(options));
    }
    return;
  }

  if (search_text.length() < kMinKeystrokesWithoutDelay) {
    delayed_find_task_.Reset(base::BindOnce(
        &FindRequestManager::EmitFindRequest, weak_factory_.GetWeakPtr(),
        request_id, search_text, std::move(options)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, delayed_find_task_.callback(), base::Milliseconds(kDelayMs));
    return;
  }

  // If we aren't going to delay, then clear any previous attempts to delay.
  delayed_find_task_.Cancel();

  EmitFindRequest(request_id, search_text, std::move(options));
}

void FindRequestManager::EmitFindRequest(int request_id,
                                         const std::u16string& search_text,
                                         blink::mojom::FindOptionsPtr options) {
  // If this is a new find session, clear any queued requests from last session.
  if (options->new_session)
    find_request_queue_ = base::queue<FindRequest>();

  find_request_queue_.emplace(request_id, search_text, std::move(options));
  if (find_request_queue_.size() == 1)
    FindInternal(find_request_queue_.front());
  if (request_id == current_session_id_)
    find_request_queue_.pop();
}

void FindRequestManager::ForEachAddedFindInPageRenderFrameHost(
    base::FunctionRef<void(RenderFrameHostImpl*)> func_ref) {
  contents_->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this, func_ref](RenderFrameHostImpl* rfh) {
        if (!CheckFrame(rfh))
          return;
        DCHECK(rfh->IsRenderFrameLive());
        DCHECK(rfh->IsActive());
        func_ref(rfh);
      });
}

void FindRequestManager::StopFinding(StopFindAction action) {
  // Cancel any delayed find-in-page requests
  delayed_find_task_.Cancel();

  ForEachAddedFindInPageRenderFrameHost([action](RenderFrameHostImpl* rfh) {
    rfh->GetFindInPage()->StopFinding(
        // TODO(dcheng): Use typemapping or use the Mojo enum directly.
        static_cast<blink::mojom::StopFindAction>(action));
  });

  current_session_id_ = kInvalidId;
#if BUILDFLAG(IS_ANDROID)
  // It is important that these pending replies are cleared whenever a find
  // session ends, so that subsequent replies for the old session are ignored.
  activate_.pending_replies.clear();
  match_rects_.pending_replies.clear();
#endif
}

bool FindRequestManager::ShouldIgnoreReply(RenderFrameHostImpl* rfh,
                                           int request_id) {
  // Ignore stale replies from abandoned find sessions or dead frames.
  return current_session_id_ == kInvalidId ||
         request_id < current_session_id_ || !CheckFrame(rfh);
}

void FindRequestManager::HandleFinalUpdateForFrame(RenderFrameHostImpl* rfh,
                                                   int request_id) {
  // This is the final update for this frame for the current find operation.
  pending_initial_replies_.erase(rfh);
  if (request_id == current_session_id_ && !pending_initial_replies_.empty()) {
    NotifyFindReply(request_id, false /* final_update */);
    return;
  }

  // This is the final update for all frames for the current find operation.
  if (request_id == current_request_.id && request_id != current_session_id_) {
    DCHECK(!current_request_.options->new_session);
    DCHECK_EQ(pending_find_next_reply_, rfh);
    pending_find_next_reply_ = nullptr;
  }

  FinalUpdateReceived(request_id, rfh);
}

void FindRequestManager::UpdatedFrameNumberOfMatches(RenderFrameHostImpl* rfh,
                                                     unsigned int old_count,
                                                     unsigned int new_count) {
  if (old_count == new_count)
    return;

  // Change the number of matches for this frame in the global count.
  number_of_matches_ -= old_count;
  number_of_matches_ += new_count;

  // All matches may have been removed since the last find reply.
  if (rfh == active_frame_ && !new_count)
    relative_active_match_ordinal_ = 0;

  // The active match ordinal may need updating since the number of matches
  // before the active match may have changed.
  UpdateActiveMatchOrdinal();
}

void FindRequestManager::SetActiveMatchRect(
    const gfx::Rect& active_match_rect) {
  selection_rect_ = active_match_rect;
}

void FindRequestManager::SetActiveMatchOrdinal(RenderFrameHostImpl* rfh,
                                               int request_id,
                                               int active_match_ordinal) {
  if (active_match_ordinal > 0) {
    // Call SetFocusedFrame on the WebContents associated with |rfh| (which
    // might not be the same as |contents_|, as a WebContents might have
    // inner WebContents). We need to focus on the frame where the active
    // match is in, which should be in the |rfh|'s associated WebContents.
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(rfh));
    // Do not focus inactive RenderFrameHost.
    if (!rfh->IsActive())
      return;
    web_contents->SetFocusedFrame(rfh->frame_tree_node(),
                                  rfh->GetSiteInstance()->group());
  }
  if (rfh == active_frame_) {
    active_match_ordinal_ +=
        active_match_ordinal - relative_active_match_ordinal_;
    relative_active_match_ordinal_ = active_match_ordinal;
  } else {
    if (active_frame_) {
      // The new active match is in a different frame than the previous, so
      // the previous active frame needs to be informed (to clear its active
      // match highlighting).
      ClearActiveFindMatch();
    }
    active_frame_ = rfh;
    relative_active_match_ordinal_ = active_match_ordinal;
    UpdateActiveMatchOrdinal();
  }
  if (pending_active_match_ordinal_ && request_id == current_request_.id)
    pending_active_match_ordinal_ = false;
  AdvanceQueue(request_id);
}

void FindRequestManager::RemoveFrame(RenderFrameHost* rfh) {
  // If matches are counted for the frame that is being removed, decrement the
  // match total before erasing that entry.
  auto it = find_in_page_clients_.find(rfh);
  if (it != find_in_page_clients_.end()) {
    number_of_matches_ -= it->second->number_of_matches();
    find_in_page_clients_.erase(it);
  } else {
    // If there's no FindInPageClient for `rfh`, the state related to it must
    // have been cleared already.
    return;
  }

  // If this is a primary main frame, then clear the search queue as well, since
  // we shouldn't be dispatching any more requests. Note that if any other frame
  // is removed, we can target any queued requests to the focused frame or
  // primary main frame. However, if the primary main frame is removed we will
  // not have a valid RenderFrameHost to target for the request queue.
  if (rfh->IsInPrimaryMainFrame())
    find_request_queue_ = base::queue<FindRequest>();

  // Update the active match ordinal, since it may have changed.
  if (active_frame_ == rfh) {
    active_frame_ = nullptr;
    relative_active_match_ordinal_ = 0;
    selection_rect_ = gfx::Rect();
  }
  UpdateActiveMatchOrdinal();

#if BUILDFLAG(IS_ANDROID)
  // The removed frame may contain the nearest find result known so far. Note
  // that once all queried frames have responded, if this result was the overall
  // nearest, then no activation will occur.
  if (rfh == activate_.nearest_frame)
    activate_.nearest_frame = nullptr;

  // Match rects in the removed frame are no longer relevant.
  if (match_rects_.frame_rects.erase(rfh) != 0)
    ++match_rects_.known_version;

  // A reply should not be expected from the removed frame.
  RemoveNearestFindResultPendingReply(rfh);
  RemoveFindMatchRectsPendingReply(rfh);
#endif

  if (current_session_id_ == kInvalidId) {
    // Just remove `rfh` from things that might point to it, but don't trigger
    // any extra processing as there is no current find session ongoing.
    pending_initial_replies_.erase(rfh);
    if (pending_find_next_reply_ == rfh) {
      pending_find_next_reply_ = nullptr;
    }
    return;
  }

  // Make sure to always clear the highlighted selection. It is useful in case
  // the user goes back to the same page using the BackForwardCache.
  static_cast<RenderFrameHostImpl*>(rfh)->GetFindInPage()->StopFinding(
      blink::mojom::StopFindAction::kStopFindActionClearSelection);

  // If no pending find replies are expected for the removed frame, then just
  // report the updated results.
  if (!base::Contains(pending_initial_replies_, rfh) &&
      pending_find_next_reply_ != rfh) {
    bool final_update =
        pending_initial_replies_.empty() && !pending_find_next_reply_;
    NotifyFindReply(current_session_id_, final_update);
    return;
  }

  if (pending_initial_replies_.erase(rfh) != 0) {
    // A reply should not be expected from the removed frame.
    if (pending_initial_replies_.empty()) {
      FinalUpdateReceived(current_session_id_, rfh);
    }
  }

  if (pending_find_next_reply_ == rfh) {
    // A reply should not be expected from the removed frame.
    pending_find_next_reply_ = nullptr;
    FinalUpdateReceived(current_request_.id, rfh);
  }
}

void FindRequestManager::ClearActiveFindMatch() {
  active_frame_->GetFindInPage()->ClearActiveFindMatch();
}

#if BUILDFLAG(IS_ANDROID)
void FindRequestManager::ActivateNearestFindResult(float x, float y) {
  if (current_session_id_ == kInvalidId)
    return;

  activate_ = ActivateNearestFindResultState(x, y);

  // Request from each frame the distance to the nearest find result (in that
  // frame) from the point (x, y), defined in find-in-page coordinates.
  ForEachAddedFindInPageRenderFrameHost([this](RenderFrameHostImpl* rfh) {
    activate_.pending_replies.insert(rfh);
    // Lifetime of FindRequestManager > RenderFrameHost > Mojo
    // connection, so it's safe to bind |this| and |rfh|.
    rfh->GetFindInPage()->GetNearestFindResult(
        activate_.point,
        base::BindOnce(&FindRequestManager::OnGetNearestFindResultReply,
                       base::Unretained(this), rfh,
                       activate_.current_request_id));
  });
}

void FindRequestManager::OnGetNearestFindResultReply(RenderFrameHostImpl* rfh,
                                                     int request_id,
                                                     float distance) {
  if (request_id != activate_.current_request_id ||
      !base::Contains(activate_.pending_replies, rfh)) {
    return;
  }

  // Check if this frame has a nearer find result than the current nearest.
  if (distance < activate_.nearest_distance) {
    activate_.nearest_frame = rfh;
    activate_.nearest_distance = distance;
  }

  RemoveNearestFindResultPendingReply(rfh);
}

void FindRequestManager::RequestFindMatchRects(int current_version) {
  match_rects_.pending_replies.clear();
  match_rects_.request_version = current_version;
  match_rects_.active_rect = gfx::RectF();

  // Request the latest find match rects from each frame.
  ForEachAddedFindInPageRenderFrameHost([this](RenderFrameHostImpl* rfh) {
    match_rects_.pending_replies.insert(rfh);
    auto it = match_rects_.frame_rects.find(rfh);
    int version = (it != match_rects_.frame_rects.end()) ? it->second.version
                                                         : kInvalidId;
    // Lifetime of FindRequestManager > RenderFrameHost > Mojo
    // connection, so it's safe to bind |this| and |rfh|.
    rfh->GetFindInPage()->FindMatchRects(
        version, base::BindOnce(&FindRequestManager::OnFindMatchRectsReply,
                                base::Unretained(this), rfh));
  });
}

void FindRequestManager::OnFindMatchRectsReply(
    RenderFrameHost* rfh,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  auto it = match_rects_.frame_rects.find(rfh);
  if (it == match_rects_.frame_rects.end() || it->second.version != version) {
    // New version of rects has been received, so update the data.
    match_rects_.frame_rects[rfh] = FrameRects(rects, version);
    ++match_rects_.known_version;
  }
  if (!active_rect.IsEmpty())
    match_rects_.active_rect = active_rect;
  RemoveFindMatchRectsPendingReply(rfh);
}
#endif

void FindRequestManager::Reset(const FindRequest& initial_request) {
  current_session_id_ = initial_request.id;
  current_request_ = initial_request;
  pending_initial_replies_.clear();
  pending_find_next_reply_ = nullptr;
  pending_active_match_ordinal_ = true;
  find_in_page_clients_.clear();
  number_of_matches_ = 0;
  active_frame_ = nullptr;
  relative_active_match_ordinal_ = 0;
  active_match_ordinal_ = 0;
  selection_rect_ = gfx::Rect();
  last_reported_id_ = kInvalidId;
  frame_observers_.clear();
#if BUILDFLAG(IS_ANDROID)
  activate_ = ActivateNearestFindResultState();
  match_rects_.pending_replies.clear();
#endif
}

void FindRequestManager::FindInternal(const FindRequest& request) {
  DCHECK_GT(request.id, current_request_.id);
  DCHECK_GT(request.id, current_session_id_);

  if (!request.options->new_session) {
    // This is a find next operation.

    // This implies that there is an ongoing find session with the same search
    // text.
    DCHECK_GE(current_session_id_, 0);
    DCHECK_EQ(request.search_text, current_request_.search_text);

    // The find next request will be directed at the focused frame if there is
    // one, or the first frame with matches otherwise.
    RenderFrameHost* target_rfh =
        contents_->GetFocusedWebContents()->GetFocusedFrame();
    if (!target_rfh || !CheckFrame(target_rfh))
      target_rfh = GetInitialFrame(request.options->forward);

    SendFindRequest(request, target_rfh);
    current_request_ = request;
    pending_active_match_ordinal_ = true;
    return;
  }

  // This is an initial find operation.
  Reset(request);

  // Add and observe eligible RFHs in the WebContents. And, use
  // ForEachRenderFrameHost instead of ForEachAddedFindInPageRenderFrameHost
  // because that calls CheckFrame() which will only be true if we've called
  // AddFrame() for the frame.
  contents_->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [this](RenderFrameHostImpl* rfh) {
        auto* wc = WebContents::FromRenderFrameHost(rfh);
        // Make sure each WebContents is only added once.
        if (rfh->IsInPrimaryMainFrame()) {
          frame_observers_.push_back(std::make_unique<FrameObserver>(wc, this));
        }
        if (IsFindInPageDisabled(rfh))
          return;
        AddFrame(rfh, false /* force */);
      });
}

void FindRequestManager::AdvanceQueue(int request_id) {
  if (find_request_queue_.empty() ||
      request_id != find_request_queue_.front().id) {
    return;
  }

  find_request_queue_.pop();
  if (!find_request_queue_.empty())
    FindInternal(find_request_queue_.front());
}

void FindRequestManager::SendFindRequest(const FindRequest& request,
                                         RenderFrameHost* rfh) {
  DCHECK(CheckFrame(rfh));
  DCHECK(rfh->IsRenderFrameLive());
  DCHECK(rfh->IsActive());

  if (request.options->new_session)
    pending_initial_replies_.insert(rfh);
  else
    pending_find_next_reply_ = rfh;

  static_cast<RenderFrameHostImpl*>(rfh)->GetFindInPage()->Find(
      request.id, base::UTF16ToUTF8(request.search_text),
      request.options.Clone());
}

void FindRequestManager::NotifyFindReply(int request_id, bool final_update) {
  if (request_id == kInvalidId) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // Ensure that replies are not reported with IDs lower than the ID of the
  // latest request we have results from.
  if (request_id < last_reported_id_)
    request_id = last_reported_id_;
  else
    last_reported_id_ = request_id;

  contents_->NotifyFindReply(request_id, number_of_matches_, selection_rect_,
                             active_match_ordinal_, final_update);
}

RenderFrameHost* FindRequestManager::GetInitialFrame(bool forward) const {
  RenderFrameHost* rfh =
      contents_->GetPrimaryFrameTree().root()->current_frame_host();
  if (!forward)
    rfh = GetDeepestLastChild(static_cast<RenderFrameHostImpl*>(rfh));

  return rfh;
}

RenderFrameHost* FindRequestManager::Traverse(RenderFrameHost* from_rfh,
                                              bool forward,
                                              bool matches_only,
                                              bool wrap) const {
  DCHECK(from_rfh);
  // If |from_rfh| is being detached, it might already be removed from
  // its parent's list of children, meaning we can't traverse it correctly.
  // We also don't traverse when |from_rfh| is in back-forward cache or is being
  // prerendered, as we don't allow any updates in these states.
  auto* from_rfh_impl = static_cast<RenderFrameHostImpl*>(from_rfh);
  if (from_rfh_impl->IsPendingDeletion() ||
      from_rfh_impl->IsInBackForwardCache() ||
      from_rfh_impl->lifecycle_state() ==
          RenderFrameHostImpl::LifecycleStateImpl::kPrerendering) {
    return nullptr;
  }

  RenderFrameHostImpl* rfh = from_rfh_impl;
  RenderFrameHostImpl* last_frame = rfh;
  while ((rfh = TraverseFrame(rfh, forward, wrap)) != nullptr) {
    if (!CheckFrame(rfh)) {
      // If we're in the same frame as before, we might got into an infinite
      // loop.
      if (last_frame == rfh)
        break;
      last_frame = rfh;
      continue;
    }
    RenderFrameHost* current_rfh = rfh;
    if (!matches_only ||
        find_in_page_clients_.find(current_rfh)->second->number_of_matches() ||
        base::Contains(pending_initial_replies_, current_rfh)) {
      // Note that if there is still a pending reply expected for this frame,
      // then it may have unaccounted matches and will not be skipped via
      // |matches_only|.
      return rfh;
    }
    if (wrap && rfh == from_rfh)
      return nullptr;
  }

  return nullptr;
}

void FindRequestManager::AddFrame(RenderFrameHost* rfh, bool force) {
  if (!rfh || !rfh->IsRenderFrameLive() || !rfh->IsActive() ||
      IsUnattachedGuestView(rfh)) {
    return;
  }

  // A frame that is already being searched should not normally be added again.
  DCHECK(force || !CheckFrame(rfh));

  DCHECK(!IsFindInPageDisabled(rfh));

  find_in_page_clients_[rfh] =
      CreateFindInPageClient(static_cast<RenderFrameHostImpl*>(rfh));

  FindRequest request = current_request_;
  request.id = current_session_id_;
  request.options->new_session = true;
  request.options->force = force;
  SendFindRequest(request, rfh);
}

bool FindRequestManager::CheckFrame(RenderFrameHost* rfh) const {
  // TODO(crbug.com/40196212): Convert IsFindInPageDisabled to a DCHECK when we
  // replace DidFinishLoad with DidFinishNavigation in FrameObserver.
  if (!rfh || !base::Contains(find_in_page_clients_, rfh) ||
      IsFindInPageDisabled(rfh)) {
    return false;
  }

  DCHECK(rfh->IsActive());
  return true;
}

void FindRequestManager::UpdateActiveMatchOrdinal() {
  active_match_ordinal_ = 0;

  if (!active_frame_ || !relative_active_match_ordinal_) {
    DCHECK(!active_frame_ && !relative_active_match_ordinal_);
    return;
  }

  // Traverse the frame tree backwards (in search order) and count all of the
  // matches in frames before the frame with the active match, in order to
  // determine the overall active match ordinal.
  RenderFrameHost* frame = active_frame_;
  while ((frame = Traverse(frame,
                           false /* forward */,
                           true /* matches_only */,
                           false /* wrap */)) != nullptr) {
    active_match_ordinal_ += find_in_page_clients_[frame]->number_of_matches();
  }
  active_match_ordinal_ += relative_active_match_ordinal_;
}

void FindRequestManager::FinalUpdateReceived(int request_id,
                                             RenderFrameHost* rfh) {
  if (!number_of_matches_ ||
      !current_request_.options->find_match ||
      (active_match_ordinal_ && !pending_active_match_ordinal_) ||
      pending_find_next_reply_) {
    // All the find results for |request_id| are in and ready to report. Note
    // that |final_update| will be set to false if there are still pending
    // replies expected from the initial find request.
    NotifyFindReply(request_id,
                    pending_initial_replies_.empty() /* final_update */);
    AdvanceQueue(request_id);
    return;
  }

  // There are matches, but no active match was returned, so another find next
  // request must be sent.

  RenderFrameHost* target_rfh;
  if (request_id == current_request_.id &&
      !current_request_.options->new_session) {
    // If this was a find next operation, then the active match will be in the
    // next frame with matches after this one.
    target_rfh = Traverse(rfh, current_request_.options->forward,
                          true /* matches_only */, true /* wrap */);
  } else if ((target_rfh =
                  contents_->GetFocusedWebContents()->GetFocusedFrame()) !=
             nullptr) {
    // Otherwise, if there is a focused frame, then the active match will be in
    // the next frame with matches after that one.
    target_rfh = Traverse(target_rfh, current_request_.options->forward,
                          true /* matches_only */, true /* wrap */);
  } else {
    // Otherwise, the first frame with matches will have the active match.
    target_rfh = GetInitialFrame(current_request_.options->forward);
    if (!CheckFrame(target_rfh) ||
        !find_in_page_clients_[target_rfh]->number_of_matches()) {
      target_rfh = Traverse(target_rfh, current_request_.options->forward,
                            true /* matches_only */, false /* wrap */);
    }
  }
  if (!target_rfh) {
    // Sometimes when the WebContents is deleted/navigated, we got into this
    // situation. We don't care about this WebContents anyways so it's ok to
    // just not ask for the active match and return immediately.
    // TODO(rakina): Understand what leads to this situation.
    // See: https://crbug.com/884679.
    return;
  }

  // Forward the find reply without |final_update| set because the active match
  // has not yet been found.
  NotifyFindReply(request_id, false /* final_update */);

  current_request_.options->new_session = false;
  SendFindRequest(current_request_, target_rfh);
}

std::unique_ptr<FindInPageClient> FindRequestManager::CreateFindInPageClient(
    RenderFrameHostImpl* rfh) {
  if (create_find_in_page_client_for_testing_)
    return create_find_in_page_client_for_testing_(this, rfh);
  return std::make_unique<FindInPageClient>(this, rfh);
}

#if BUILDFLAG(IS_ANDROID)
void FindRequestManager::RemoveNearestFindResultPendingReply(
    RenderFrameHost* rfh) {
  auto it = activate_.pending_replies.find(rfh);
  if (it == activate_.pending_replies.end())
    return;

  activate_.pending_replies.erase(it);
  if (activate_.pending_replies.empty() &&
      CheckFrame(activate_.nearest_frame)) {
    const auto client_it = find_in_page_clients_.find(activate_.nearest_frame);
    if (client_it != find_in_page_clients_.end())
      client_it->second->ActivateNearestFindResult(current_session_id_,
                                                   activate_.point);
  }
}

void FindRequestManager::RemoveFindMatchRectsPendingReply(
    RenderFrameHost* rfh) {
  auto it = match_rects_.pending_replies.find(rfh);
  if (it == match_rects_.pending_replies.end())
    return;

  match_rects_.pending_replies.erase(it);
  if (!match_rects_.pending_replies.empty())
    return;

  // All replies are in.
  std::vector<gfx::RectF> aggregate_rects;
  if (match_rects_.request_version != match_rects_.known_version) {
    // Request version is stale, so aggregate and report the newer find
    // match rects. The rects should be aggregated in search order.
    for (RenderFrameHost* frame = GetInitialFrame(true /* forward */); frame;
         frame = Traverse(frame, true /* forward */, true /* matches_only */,
                          false /* wrap */)) {
      auto frame_it = match_rects_.frame_rects.find(frame);
      if (frame_it == match_rects_.frame_rects.end())
        continue;

      std::vector<gfx::RectF>& frame_rects = frame_it->second.rects;
      aggregate_rects.insert(aggregate_rects.end(), frame_rects.begin(),
                             frame_rects.end());
    }
  }
  contents_->NotifyFindMatchRectsReply(
      match_rects_.known_version, aggregate_rects, match_rects_.active_rect);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
