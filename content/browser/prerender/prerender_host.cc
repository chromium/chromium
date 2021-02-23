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
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class PrerenderHost::PageHolderInterface {
 public:
  PageHolderInterface() = default;
  virtual ~PageHolderInterface() = default;
  virtual NavigationController& GetNavigationController() = 0;
  virtual RenderFrameHostImpl* GetMainFrame() = 0;
  virtual WebContents* GetWebContents() = 0;
  virtual bool Activate(RenderFrameHostImpl& current_render_frame_host) = 0;
  virtual void WaitForLoadCompletionForTesting() = 0;  // IN-TEST
};

class PrerenderHost::MPArchPageHolder
    : public PrerenderHost::PageHolderInterface,
      public FrameTree::Delegate,
      public NavigationControllerDelegate {
 public:
  explicit MPArchPageHolder(WebContentsImpl& web_contents)
      : web_contents_(web_contents),
        frame_tree_(web_contents.GetBrowserContext(),
                    this,
                    this,
                    &web_contents,
                    &web_contents,
                    &web_contents,
                    &web_contents,
                    &web_contents) {
    frame_tree_.Init(
        SiteInstance::Create(web_contents.GetBrowserContext()).get(),
        /*renderer_initiated_creation=*/false,
        /*main_frame_name=*/"", /*is_prerendering=*/true);

    // TODO(https://crbug.com/1164280): This should be moved to FrameTree::Init
    web_contents_.NotifySwappedFromRenderManager(
        /*old_frame=*/nullptr,
        frame_tree_.root()->render_manager()->current_frame_host(),
        /*is_main_frame=*/true);
  }

  // TODO(https://crbug.com/1176148): Mostly copied from ~WebContentsImpl. Move
  // to ~FrameTree or some common place.
  ~MPArchPageHolder() override {
    for (FrameTreeNode* node : frame_tree_.Nodes()) {
      node->render_manager()->ClearRFHsPendingShutdown();
      // TODO(https://crbug.com/1164280): Ban WebUI instance in Prerender pages.
      node->render_manager()->ClearWebUIInstances();
    }

    GetMainFrame()->ResetChildren();
    RenderFrameHostManager* root = frame_tree_.root()->render_manager();

    root->ResetProxyHosts();

    GetNavigationController().GetBackForwardCache().Shutdown();

    root->current_frame_host()->RenderFrameDeleted();
    root->current_frame_host()->ResetNavigationRequests();

    frame_tree_.root()->ResetNavigationRequest(true);
    if (root->speculative_frame_host()) {
      root->speculative_frame_host()->DeleteRenderFrame(
          mojom::FrameDeleteIntention::kSpeculativeMainFrameForShutdown);
      root->speculative_frame_host()->RenderFrameDeleted();
      root->speculative_frame_host()->ResetNavigationRequests();
    }

    web_contents_.OnFrameTreeNodeDestroyed(frame_tree_.root());
    web_contents_.RenderViewDeleted(
        root->current_frame_host()->render_view_host());
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
  void Stop() override { frame_tree_.StopLoading(); }
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
    return frame_tree_.controller();
  }

  RenderFrameHostImpl* GetMainFrame() override {
    return frame_tree_.root()->current_frame_host();
  }

  bool Activate(RenderFrameHostImpl& current_render_frame_host) override {
    NOTREACHED();
    return false;
  }

  // TODO(https://crbug.com/1164280): Once we dispatch load events for prerender
  // pages this method will no longer be needed and should go away.
  void WaitForLoadCompletionForTesting() override {
    if (!frame_tree_.IsLoading())
      return;

    base::RunLoop loop;
    on_stopped_loading_for_tests_ = loop.QuitClosure();
    loop.Run();
  }

 private:
  // WebContents where this prerenderer is embedded.
  WebContentsImpl& web_contents_;
  FrameTree frame_tree_;
  base::OnceClosure on_stopped_loading_for_tests_;
};

class PrerenderHost::WebContentsPageHolder
    : public PrerenderHost::PageHolderInterface {
 public:
  explicit WebContentsPageHolder(BrowserContext* browser_context) {
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

  bool Activate(RenderFrameHostImpl& current_render_frame_host) override {
    auto* current_web_contents =
        WebContents::FromRenderFrameHost(&current_render_frame_host);
    if (!current_web_contents)
      return false;

    // Merge browsing history.
    web_contents_->GetController().CopyStateFromAndPrune(
        &current_web_contents->GetController(), /*replace_entry=*/false);

    // Activate the prerendered contents.
    WebContentsDelegate* delegate = current_web_contents->GetDelegate();
    DCHECK(delegate);
    DCHECK(web_contents_);
    DCHECK(GetMainFrame()->frame_tree()->is_prerendering());
    GetMainFrame()->frame_tree()->ActivatePrerenderedFrameTree();

    // Tentatively use Portal's activation function.
    // TODO(https://crbug.com/1132746): Replace this with the MPArch.
    std::unique_ptr<WebContents> predecessor_web_contents =
        delegate->ActivatePortalWebContents(current_web_contents,
                                            std::move(web_contents_));

    // Stop loading on the predecessor WebContents.
    predecessor_web_contents->Stop();
    return true;
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
                             const url::Origin& initiator_origin,
                             WebContentsImpl& web_contents)
    : attributes_(std::move(attributes)), initiator_origin_(initiator_origin) {
  DCHECK(blink::features::IsPrerender2Enabled());
  CreatePageHolder(web_contents);
}

// TODO(https://crbug.com/1132746): Abort ongoing prerendering and notify the
// mojo capability controller in the destructor.
PrerenderHost::~PrerenderHost() {
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

bool PrerenderHost::ActivatePrerenderedContents(
    RenderFrameHostImpl& current_render_frame_host) {
  TRACE_EVENT1("navigation", "PrerenderHost::ActivatePrerenderedContents",
               "render_frame_host",
               base::trace_event::ToTracedValue(&current_render_frame_host));

  DCHECK(is_ready_for_activation_);
  is_ready_for_activation_ = false;

  // TODO(https://crbug.com/1142658): Notify renderer processes that the
  // contents get activated.

  bool success = page_holder_->Activate(current_render_frame_host);

  // TODO(https://crbug.com/1126305): Record the final status for activation
  // failure.
  if (success)
    RecordFinalStatus(FinalStatus::kActivated);

  return success;
}

RenderFrameHostImpl* PrerenderHost::GetPrerenderedMainFrameHostForTesting() {
  return page_holder_->GetMainFrame();
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

}  // namespace content
