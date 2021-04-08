// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_host.h"

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/run_loop.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/features.h"

namespace content {

namespace {
void CollectAllChildren(RenderFrameHostImpl& rfh,
                        std::vector<RenderFrameHostImpl*>* result) {
  result->push_back(&rfh);
  for (size_t i = 0; i < rfh.child_count(); ++i) {
    if (RenderFrameHostImpl* speculative_frame_host =
            rfh.child_at(i)->render_manager()->speculative_frame_host()) {
      result->push_back(speculative_frame_host);
    }
    CollectAllChildren(*(rfh.child_at(i)->current_frame_host()), result);
  }
}

// Iterate over RenderFrameHostImpl::children_ rather than FrameTree::Nodes()
// because the |rfh| root node will not have a current_frame_host value. The
// root node is set to null in MPArch prerender activation when generating a
// BackForwardCacheImpl::Entry.
std::vector<RenderFrameHostImpl*> AllDescendantActiveRenderFrameHosts(
    RenderFrameHostImpl& rfh) {
  std::vector<RenderFrameHostImpl*> result;
  CollectAllChildren(rfh, &result);
  return result;
}

struct ActivateResult {
  ActivateResult(PrerenderHost::FinalStatus status,
                 std::unique_ptr<BackForwardCacheImpl::Entry> entry)
      : status(status), entry(std::move(entry)) {}

  PrerenderHost::FinalStatus status = PrerenderHost::FinalStatus::kActivated;
  std::unique_ptr<BackForwardCacheImpl::Entry> entry;
};

}  // namespace

class PrerenderHost::PageHolderInterface {
 public:
  PageHolderInterface() = default;
  virtual ~PageHolderInterface() = default;
  virtual NavigationController& GetNavigationController() = 0;
  virtual RenderFrameHostImpl* GetMainFrame() = 0;
  virtual WebContents* GetWebContents() = 0;
  virtual ActivateResult Activate(
      RenderFrameHostImpl& current_render_frame_host,
      NavigationRequest& navigation_request) = 0;
  virtual void WaitForLoadCompletionForTesting() = 0;  // IN-TEST
};

class PrerenderHost::MPArchPageHolder
    : public PrerenderHost::PageHolderInterface,
      public FrameTree::Delegate,
      public NavigationControllerDelegate {
 public:
  explicit MPArchPageHolder(WebContentsImpl& web_contents)
      : web_contents_(web_contents),
        frame_tree_(
            std::make_unique<FrameTree>(web_contents.GetBrowserContext(),
                                        this,
                                        this,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents,
                                        &web_contents)) {
    DCHECK(blink::features::IsPrerenderMPArchEnabled());

    frame_tree_->Init(
        SiteInstance::Create(web_contents.GetBrowserContext()).get(),
        /*renderer_initiated_creation=*/false,
        /*main_frame_name=*/"", FrameTree::Type::kPrerender);

    // TODO(https://crbug.com/1164280): This should be moved to FrameTree::Init
    web_contents_.NotifySwappedFromRenderManager(
        /*old_frame=*/nullptr,
        frame_tree_->root()->render_manager()->current_frame_host(),
        /*is_main_frame=*/true);
  }

  ~MPArchPageHolder() override {
    if (frame_tree_)
      frame_tree_->Shutdown();
  }

  // FrameTree::Delegate

  // TODO(https://crbug.com/1164280): Correctly handle load events. Ignored for
  // now as it confuses WebContentsObserver instances because they can not
  // distinguish between the different FrameTrees.

  void DidStartLoading(FrameTreeNode* frame_tree_node,
                       bool to_different_document) override {}

  void DidStopLoading() override {
    if (on_stopped_loading_for_tests_) {
      std::move(on_stopped_loading_for_tests_).Run();
    }
  }

  void DidChangeLoadProgress() override {}
  bool IsHidden() override { return true; }

  // NavigationControllerDelegate
  void NotifyNavigationStateChanged(InvalidateTypes changed_flags) override {}
  void Stop() override { frame_tree_->StopLoading(); }
  bool IsBeingDestroyed() override { return false; }
  void NotifyBeforeFormRepostWarningShow() override {}
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {}
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override {}
  void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) override {}
  void NotifyNavigationEntriesDeleted() override {}
  void ActivateAndShowRepostFormWarningDialog() override {}
  bool ShouldPreserveAbortedURLs() override { return false; }
  WebContents* GetWebContents() override { return &web_contents_; }
  void UpdateOverridingUserAgent() override {}

  // PageHolder
  NavigationControllerImpl& GetNavigationController() override {
    return frame_tree_->controller();
  }

  RenderFrameHostImpl* GetMainFrame() override {
    return frame_tree_->root()->current_frame_host();
  }

  ActivateResult Activate(RenderFrameHostImpl& current_render_frame_host,
                          NavigationRequest& navigation_request) override {
    if (frame_tree_->root()->HasNavigation()) {
      // We do not yet support activation if there is an ongoing navigation in
      // the main frame as the code assumes that NavigationRequest is associated
      // with the fixed frame tree node. Ongoing navigations in frames are
      // supported experimentally and require more investigation to ensure that
      // these NavigationRequests can be transferred to a new
      // NavigationController and that new NavigationEntries will be correctly
      // created for them.
      // TODO(https://crbug.com/1190644): Make sure sub-frame navigations are
      // fine.
      return ActivateResult(FinalStatus::kInProgressNavigation, nullptr);
    }

    // NOTE: TakePrerenderedPage() clears the current_frame_host value of
    // frame_tree_->root(). Do not add any code between here and
    // frame_tree_.reset() that calls into observer functions to minimize the
    // duration of current_frame_host being null.
    //
    // TODO(https://crbug.com/1176148): Investigate how to combine taking the
    // prerendered page and frame_tree_ destruction.
    std::unique_ptr<BackForwardCacheImpl::Entry> page =
        frame_tree_->root()->render_manager()->TakePrerenderedPage();

    std::unique_ptr<NavigationEntryImpl> nav_entry =
        GetNavigationController()
            .GetEntryWithUniqueID(page->render_frame_host->nav_entry_id())
            ->Clone();

    navigation_request.SetPrerenderNavigationEntry(std::move(nav_entry));

    FrameTree* target_frame_tree = web_contents_.GetFrameTree();
    DCHECK_EQ(target_frame_tree, current_render_frame_host.frame_tree());

    page->render_frame_host->SetFrameTreeNode(*(target_frame_tree->root()));
    // TODO(https://crbug.com/1170277): Add testing for cross-origin iframes.
    for (auto& it : page->proxy_hosts) {
      it.second->set_frame_tree_node(*(target_frame_tree->root()));
    }

    // Iterate over root RenderFrameHost and all of its descendant frames and
    // updates the associated frame tree. Note that subframe proxies don't need
    // their FrameTrees independently updated, since their FrameTreeNodes don't
    // change, and FrameTree references in those FrameTreeNodes will be updated
    // through RenderFrameHosts.
    //
    // TODO(https://crbug.com/1170277): Need to investigate if and how
    // pending delete RenderFrameHost objects should be handled if prerendering
    // runs all of the unload handlers; they are not currently handled here.
    // This is because pending delete RenderFrameHosts can still receive and
    // process some messages while the RenderFrameHost FrameTree and
    // FrameTreeNode are stale.
    for (auto* rfh : AllDescendantActiveRenderFrameHosts(
             *(page->render_frame_host.get()))) {
      rfh->frame_tree_node()->SetFrameTree(*target_frame_tree);
      rfh->SetFrameTree(*target_frame_tree);
      rfh->render_view_host()->SetFrameTree(*target_frame_tree);
      if (rfh->GetRenderWidgetHost()) {
        rfh->GetRenderWidgetHost()->SetFrameTree(*target_frame_tree);
      }
    }

    frame_tree_->Shutdown();
    frame_tree_.reset();

    return ActivateResult(FinalStatus::kActivated, std::move(page));
  }

  void WaitForLoadCompletionForTesting() override {
    if (!frame_tree_->IsLoading())
      return;

    base::RunLoop loop;
    on_stopped_loading_for_tests_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  // WebContents where this prerenderer is embedded.
  WebContentsImpl& web_contents_;
  // Frame tree created for the prerenderer to load the page and prepare it for
  // a future activation. During activation, the prerendered page will be taken
  // out from |frame_tree_| and moved over to |web_contents_|'s primary frame
  // tree, while |frame_tree_| will be deleted.
  std::unique_ptr<FrameTree> frame_tree_;
  base::OnceClosure on_stopped_loading_for_tests_;
};

class PrerenderHost::WebContentsPageHolder
    : public PrerenderHost::PageHolderInterface {
 public:
  explicit WebContentsPageHolder(BrowserContext* browser_context) {
    DCHECK(blink::features::IsPrerenderWebContentsEnabled());

    // Create a new WebContents for prerendering.
    WebContents::CreateParams web_contents_params(browser_context);
    web_contents_params.is_prerendering = true;
    // TODO(https://crbug.com/1132746): Set up other fields of
    // `web_contents_params` as well, and add tests for them.
    web_contents_ = WebContents::Create(web_contents_params);
    DCHECK(static_cast<WebContentsImpl*>(web_contents_.get())
               ->GetFrameTree()
               ->is_prerendering());
  }

  ~WebContentsPageHolder() override = default;

  // PageHolder
  NavigationController& GetNavigationController() override {
    return web_contents_->GetController();
  }

  RenderFrameHostImpl* GetMainFrame() override {
    return static_cast<RenderFrameHostImpl*>(web_contents_->GetMainFrame());
  }

  WebContents* GetWebContents() override { return web_contents_.get(); }

  ActivateResult Activate(RenderFrameHostImpl& current_render_frame_host,
                          NavigationRequest& navigation_request) override {
    auto* current_web_contents =
        WebContents::FromRenderFrameHost(&current_render_frame_host);
    DCHECK(current_web_contents);

    // Merge browsing history.
    GetNavigationController().CopyStateFromAndPrune(
        &current_web_contents->GetController(), /*replace_entry=*/false);

    // Activate the prerendered contents.
    WebContentsDelegate* delegate = current_web_contents->GetDelegate();
    DCHECK(delegate);
    DCHECK(GetMainFrame()->frame_tree()->is_prerendering());
    GetMainFrame()->frame_tree()->ActivatePrerenderedFrameTree();

    // Tentatively use Portal's activation function.
    // TODO(https://crbug.com/1132746): Replace this with the MPArch.
    std::unique_ptr<WebContents> predecessor_web_contents =
        delegate->ActivatePortalWebContents(current_web_contents,
                                            std::move(web_contents_));

    // Stop loading on the predecessor WebContents.
    predecessor_web_contents->Stop();

    return ActivateResult(FinalStatus::kActivated, nullptr);
  }

  void WaitForLoadCompletionForTesting() override {
    if (!web_contents_ || !static_cast<WebContentsImpl*>(web_contents_.get())
                               ->GetFrameTree()
                               ->IsLoading()) {
      return;
    }

    base::RunLoop loop;
    LoadStopObserver observer(web_contents_.get(), loop.QuitClosure());
    loop.Run();
  }

 private:
  class LoadStopObserver : public WebContentsObserver {
   public:
    LoadStopObserver(WebContents* web_contents,
                     base::OnceClosure on_stopped_loading)
        : WebContentsObserver(web_contents),
          on_stopped_loading_(std::move(on_stopped_loading)) {}

    void DidStopLoading() override {
      std::move(on_stopped_loading_).Run();
      Observe(nullptr);
    }

   private:
    base::OnceClosure on_stopped_loading_;
  };

  std::unique_ptr<WebContents> web_contents_;
};

PrerenderHost::PrerenderHost(blink::mojom::PrerenderAttributesPtr attributes,
                             RenderFrameHostImpl& initiator_render_frame_host)
    : attributes_(std::move(attributes)),
      initiator_origin_(initiator_render_frame_host.GetLastCommittedOrigin()),
      initiator_process_id_(initiator_render_frame_host.GetProcess()->GetID()),
      initiator_frame_token_(initiator_render_frame_host.GetFrameToken()) {
  DCHECK(blink::features::IsPrerender2Enabled());
  auto* web_contents =
      WebContents::FromRenderFrameHost(&initiator_render_frame_host);
  DCHECK(web_contents);
  CreatePageHolder(*static_cast<WebContentsImpl*>(web_contents));
}

// TODO(https://crbug.com/1132746): Abort ongoing prerendering and notify the
// mojo capability controller in the destructor.
PrerenderHost::~PrerenderHost() {
  for (auto& observer : observers_)
    observer.OnHostDestroyed();

  if (!final_status_)
    RecordFinalStatus(FinalStatus::kDestroyed);
}

// TODO(https://crbug.com/1132746): Inspect diffs from the current
// no-state-prefetch implementation. See PrerenderContents::StartPrerendering()
// for example.
void PrerenderHost::StartPrerendering() {
  TRACE_EVENT0("navigation", "PrerenderHost::StartPrerendering");

  // Observe events about the prerendering contents.
  Observe(page_holder_->GetWebContents());

  // Start prerendering navigation.
  NavigationController::LoadURLParams load_url_params(attributes_->url);
  load_url_params.initiator_origin = initiator_origin_;
  load_url_params.initiator_process_id = initiator_process_id_;
  load_url_params.initiator_frame_token = initiator_frame_token_;

  // Just use the referrer from attributes, as NoStatePrefetch does.
  // TODO(crbug.com/1176054): For cross-origin prerender, follow the spec steps
  // for "sufficiently-strict speculative navigation referrer policies".
  if (attributes_->referrer)
    load_url_params.referrer = Referrer(*attributes_->referrer);

  // TODO(https://crbug.com/1132746): Set up other fields of `load_url_params`
  // as well, and add tests for them.
  page_holder_->GetNavigationController().LoadURLWithParams(load_url_params);
}

// TODO(https://crbug.com/1170277): Does not work with MPArch as we get
// navigation events for all FrameTrees.
void PrerenderHost::DidFinishNavigation(NavigationHandle* navigation_handle) {
  // The prerendered contents are considered ready for activation when it
  // reaches DidFinishNavigation.
  DCHECK(!is_ready_for_activation_);
  is_ready_for_activation_ = true;

  // Stop observing the events about the prerendered contents.
  Observe(nullptr);
}

std::unique_ptr<BackForwardCacheImpl::Entry>
PrerenderHost::ActivatePrerenderedContents(
    RenderFrameHostImpl& old_render_frame_host,
    NavigationRequest& navigation_request) {
  TRACE_EVENT1("navigation", "PrerenderHost::ActivatePrerenderedContents",
               "render_frame_host", old_render_frame_host);

  DCHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  ActivateResult result =
      page_holder_->Activate(old_render_frame_host, navigation_request);

  if (result.status != FinalStatus::kActivated) {
    RecordFinalStatus(result.status);
    return nullptr;
  }

  for (auto& observer : observers_)
    observer.OnActivated();

  RecordFinalStatus(FinalStatus::kActivated);
  // NOTE: for activation with multiple WebContents, `entry` is null.
  return std::move(result.entry);
}

RenderFrameHostImpl* PrerenderHost::GetPrerenderedMainFrameHost() {
  return page_holder_->GetMainFrame();
}

void PrerenderHost::RecordFinalStatus(base::PassKey<PrerenderHostRegistry>,
                                      FinalStatus status) {
  RecordFinalStatus(status);
}

void PrerenderHost::CreatePageHolder(WebContentsImpl& web_contents) {
  switch (blink::features::kPrerender2ImplementationParam.Get()) {
    case blink::features::Prerender2Implementation::kWebContents: {
      page_holder_ = std::make_unique<WebContentsPageHolder>(
          web_contents.GetBrowserContext());
      break;
    }
    case blink::features::Prerender2Implementation::kMPArch: {
      page_holder_ = std::make_unique<MPArchPageHolder>(web_contents);
      break;
    }
  }

  frame_tree_node_id_ = page_holder_->GetMainFrame()->GetFrameTreeNodeId();
}

void PrerenderHost::WaitForLoadStopForTesting() {
  page_holder_->WaitForLoadCompletionForTesting();  // IN-TEST
}

FrameTree* PrerenderHost::GetPrerenderedFrameTree() {
  DCHECK(page_holder_);
  return page_holder_->GetMainFrame()->frame_tree();
}

void PrerenderHost::RecordFinalStatus(FinalStatus status) {
  DCHECK(!final_status_);
  final_status_ = status;
  base::UmaHistogramEnumeration(
      "Prerender.Experimental.PrerenderHostFinalStatus", status);
}

const GURL& PrerenderHost::GetInitialUrl() const {
  return attributes_->url;
}

void PrerenderHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrerenderHost::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrerenderHost::IsAssociatedWith(const WebContentsImpl& web_contents) {
  return page_holder_->GetWebContents() == &web_contents;
}

}  // namespace content
