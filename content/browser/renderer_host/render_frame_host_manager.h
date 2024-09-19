// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "content/browser/renderer_host/browsing_context_group_swap.h"
#include "content/browser/renderer_host/browsing_context_state.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/should_swap_browsing_instance.h"
#include "content/browser/renderer_host/stored_page.h"
#include "content/browser/security/coop/cross_origin_opener_policy_status.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom-forward.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/referrer.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-forward.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace blink {
struct FramePolicy;
}  // namespace blink

namespace content {
class BatchedProxyIPCSender;
class FrameTree;
class FrameTreeNode;
class NavigationControllerImpl;
class NavigationEntry;
class NavigationRequest;
class NavigatorTest;
class RenderFrameHostManagerTest;
class RenderFrameProxyHost;
class RenderViewHost;
class RenderViewHostImpl;
class RenderWidgetHostViewBase;
class RenderWidgetHostViewChildFrame;
class TestWebContents;

using PageBroadcastMethodCallback =
    base::RepeatingCallback<void(RenderViewHostImpl*)>;

using RemoteFramesBroadcastMethodCallback =
    base::RepeatingCallback<void(RenderFrameProxyHost*)>;

// Reasons that `GetFrameHostForNavigation()` might fail.
enum class GetFrameHostForNavigationFailed {
  // Failed to reinitialize the main frame, for whatever reason.
  // TODO(crbug.com/40250311): This adds a tremendous amount of failure
  // plumbing *everywhere* and might be unnecessary.
  kCouldNotReinitializeMainFrame,
  // The speculative RenderFrameHost is pending commit and cannot be discarded.
  // This blocks navigations that could reuse the current RenderFrameHost
  // (because they cannot discard the theoretically-unnecessary speculative
  // RenderFrameHost) and navigations that need a different speculative
  // RenderFrameHost (because the pre-existing unsuitable speculative
  // RenderFrameHost cannot be discarded).
  kBlockedByPendingCommit,
  // Intentionally defer the creation of the RenderFrameHost to prioritize
  // initiating the network request instead.
  // Please refer to the comments of features:kDeferSpeculativeRFHCreation
  // in contents/common/features.cc for more details.
  kIntentionalDefer,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(DeferSpeculativeRFHAction)
enum class DeferSpeculativeRFHAction {
  kNotDeferred = 0,
  kDeferredWithRenderProcessWarmUp = 1,
  kDeferredWithoutRenderProcessWarmUp = 2,
  kMaxValue = kDeferredWithoutRenderProcessWarmUp,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:DeferSpeculativeRFHAction)

// Manages RenderFrameHosts for a FrameTreeNode. It maintains a
// current_frame_host() which is the content currently visible to the user. When
// a frame is told to navigate to a different web site (as determined by
// SiteInstance), it will replace its current RenderFrameHost with a new
// RenderFrameHost dedicated to the new SiteInstance, possibly in a new process.
//
// Cross-process navigation works like this:
//
// - RFHM::Navigate determines whether the destination is cross-site, and if so,
//   it creates a pending_render_frame_host_.
//
// - The pending RFH is created in the "navigations suspended" state, meaning no
//   navigation messages are sent to its renderer until the beforeunload handler
//   has a chance to run in the current RFH.
//
// - The current RFH runs its beforeunload handler. If it returns false, we
//   cancel all the pending logic. Otherwise we allow the pending RFH to send
//   the navigation request to its renderer.
//
// - ResourceDispatcherHost receives a ResourceRequest on the IO thread for the
//   main resource load from the pending RFH. It creates a
//   CrossSiteResourceHandler to check whether a process transfer is needed when
//   the request is ready to commit.
//
// - When RDH receives a response, the MimeTypeResourceHandler determines
//   whether it is a navigation type that doesn't commit (e.g. download, 204 or
//   error page). If so, it sends a message to the new renderer causing it to
//   cancel the request, and the request (e.g. the download) proceeds. In this
//   case, the pending RFH will never become the current RFH, but it remains
//   until the next DidNavigate event for this WebContentsImpl.
//
// - After RDH receives a response and determines that it is safe and not a
//   download, the CrossSiteResourceHandler checks whether a transfer for a
//   redirect is needed. If so, it pauses the network response and starts an
//   identical navigation in a new pending RFH. When the identical request is
//   later received by RDH, the response is transferred and unpaused.
//
// - Otherwise, the network response commits in the pending RFH's renderer,
//   which sends a DidCommitProvisionalLoad message back to the browser process.
//
// - RFHM::CommitPending makes visible the new RFH, and initiates the unload
//   handler in the old RFH. The unload handler will complete in the background.
//
// - RenderFrameHostManager may keep the previous RFH alive as a
//   RenderFrameProxyHost, to be used (for example) if the user goes back. The
//   process only stays live if another tab is using it, but if so, the existing
//   frame relationships will be maintained.
class CONTENT_EXPORT RenderFrameHostManager {
 public:
  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in WebContentsImpl
  // that are required to run this class. The design should probably be better
  // such that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // WebContentsImpl are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  class CONTENT_EXPORT Delegate {
   public:
    // Initializes the given renderer if necessary and creates the view ID
    // corresponding to this view host. If this method is not called and the
    // process is not shared, then the WebContentsImpl will act as though the
    // renderer is not running (i.e., it will render "sad tab"). This method is
    // automatically called from LoadURL.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host,
        const std::optional<blink::FrameToken>& opener_frame_token,
        RenderFrameProxyHost* proxy_host) = 0;
    virtual void CreateRenderWidgetHostViewForRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed,
        bool* proceed_to_fire_unload) = 0;
    virtual void CancelModalDialogsForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager(
        RenderFrameHostImpl* old_frame,
        RenderFrameHostImpl* new_frame) = 0;
    // Notifies that we are swapping to a `new_frame` when there is no
    // `old_frame` available from which to take fallback content.
    // TODO(crbug.com/40052076): Remove this once CommitPending has more
    // explicit shutdown, both for successful and failed navigations.
    virtual void NotifySwappedFromRenderManagerWithoutFallbackContent(
        RenderFrameHostImpl* new_frame) = 0;
    // TODO(nasko): This should be removed once extensions no longer use
    // NotificationService. See https://crbug.com/462682.
    //
    // TODO(https://crbug.com/338233133): The extensions process manager does
    // not use NotificationService; clean this up.
    virtual void NotifyMainFrameSwappedFromRenderManager(
        RenderFrameHostImpl* old_frame,
        RenderFrameHostImpl* new_frame) = 0;

    // Returns true if the location bar should be focused by default rather than
    // the page contents. The view calls this function when the tab is focused
    // to see what it should do.
    virtual bool FocusLocationBarByDefault() = 0;

    // If the delegate is an inner WebContents, reattach it to the outer
    // WebContents.
    virtual void ReattachOuterDelegateIfNeeded() = 0;

    // Called when a FrameTreeNode is destroyed.
    virtual void OnFrameTreeNodeDestroyed(FrameTreeNode* node) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Calling `IsNavigationSameSite()` many times is expensive
  // (https://crbug.com/1380942). This struct will lazily cache the output of
  // `IsNavigationSameSite()`. If there is no cached value, Get() will cache
  // the output of `IsNavigationSameSite()`, and will return the cached value in
  // subsequent calls.
  //
  // This struct is used by passing it as a parameter throughout a callstack
  // that contains `IsNavigationSameSite()`. It is only used for a given
  // navigation event (for which `IsNavigationSameSite()` will not change), and
  // should not be stored or used for other events in the same navigation
  // (e.g., after redirects) or for other navigations.
  struct IsSameSiteGetter {
   public:
    IsSameSiteGetter();
    explicit IsSameSiteGetter(bool is_same_site);

    IsSameSiteGetter(const IsSameSiteGetter&) = delete;

    // Returns the (possibly cached) value of
    // render_frame_host->IsNavigationSameSite(url_info). (For cached results,
    // this includes DCHECKs that the value hasn't changed, so the optimization
    // only reduces the number of calls in release builds without DCHECKs.)
    bool Get(const RenderFrameHostImpl& render_frame_host,
             const UrlInfo& url_info);

   private:
    std::optional<bool> is_same_site_;
  };

  // The delegate pointer must be non-null and is not owned by this class. It
  // must outlive this class.
  //
  // You must call one of the Init*() methods before using this class.
  RenderFrameHostManager(FrameTreeNode* frame_tree_node, Delegate* delegate);

  RenderFrameHostManager(const RenderFrameHostManager&) = delete;
  RenderFrameHostManager& operator=(const RenderFrameHostManager&) = delete;

  ~RenderFrameHostManager();

  // Initialize this frame as the root of a new FrameTree.
  void InitRoot(SiteInstanceImpl* site_instance,
                bool renderer_initiated_creation,
                blink::FramePolicy initial_main_frame_policy,
                const std::string& name,
                const base::UnguessableToken& devtools_frame_token);

  // Initialize this frame as the child of another frame.
  void InitChild(SiteInstanceImpl* site_instance,
                 int32_t frame_routing_id,
                 mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
                 const blink::LocalFrameToken& frame_token,
                 const blink::DocumentToken& document_token,
                 const base::UnguessableToken& devtools_frame_token,
                 blink::FramePolicy frame_policy,
                 std::string frame_name,
                 std::string frame_unique_name);

  // Returns the currently active RenderFrameHost.
  //
  // This will be non-null between Init() and Shutdown(), but may be null
  // briefly during shutdown, after RenderFrameHostManager's destructor
  // clears `render_frame_host_`.  Hence, this does not need to be null-checked
  // except for rare cases reachable during shutdown.  For example, observer
  // methods like RenderProcessExited could be dispatched after this has
  // already been cleared.
  RenderFrameHostImpl* current_frame_host() const {
    return render_frame_host_.get();
  }

  // Returns the view associated with the current RenderViewHost, or null if
  // there is no current one.
  RenderWidgetHostViewBase* GetRenderWidgetHostView() const;

  // Returns whether this manager is a main frame and belongs to a FrameTreeNode
  // that belongs to an inner WebContents or inner FrameTree.
  bool IsMainFrameForInnerDelegate();

  // If this is a RenderFrameHostManager for a main frame, this method returns
  // the FrameTreeNode for the frame in the outer WebContents (if any) that
  // contains the inner WebContents.
  FrameTreeNode* GetOuterDelegateNode() const;

  // Return a proxy for this frame in the parent frame's SiteInstance.  Returns
  // nullptr if this is a main frame or if such a proxy does not exist (for
  // example, if this frame is same-site with its parent OR if this frame will
  // be deleted soon and we are just waiting for the frame's unload handler).
  RenderFrameProxyHost* GetProxyToParent();

  // If this is a RenderFrameHostManager for a main frame, returns the proxy
  // representing this main frame to its outer document's SiteInstance. Returns
  // nullptr if this is not the main frame of an inner frame tree.
  RenderFrameProxyHost* GetProxyToOuterDelegate();

  // If this is a main frame for an inner delegate, return the
  // GetProxyToOuterDelegate, otherwise return GetProxyToParent.
  RenderFrameProxyHost* GetProxyToParentOrOuterDelegate();

  // If this is a RenderFrameHostManager for a main frame, removes the
  // FrameTreeNode in the outer WebContents that represents this FrameTreeNode.
  // TODO(lazyboy): This does not belong to RenderFrameHostManager, move it to
  // somewhere else.
  void RemoveOuterDelegateFrame();

  // Returns the speculative RenderFrameHost, or null if there is no speculative
  // one.
  RenderFrameHostImpl* speculative_frame_host() const {
    return speculative_render_frame_host_.get();
  }

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Confirms whether we should close the page. |proceed| indicates whether the
  // user chose to proceed. This is called in one of the two *distinct*
  // scenarios below:
  //   1- The tab/window is closed after allowing the appropriate renderer to
  //      show the beforeunload prompt.
  //   2- The FrameTreeNode is being prepared for attaching an inner Delegate,
  //      in which case beforeunload is triggered in the current frame. This
  //      only happens for child frames.
  void BeforeUnloadCompleted(bool proceed);

  // Called when a renderer's frame navigates.
  void DidNavigateFrame(RenderFrameHostImpl* render_frame_host,
                        bool was_caused_by_user_gesture,
                        bool is_same_document_navigation,
                        bool clear_proxies_on_commit,
                        const blink::FramePolicy& frame_policy,
                        bool allow_paint_holding);

  // Called when this frame's opener is changed to the frame specified by
  // |opener_frame_token| in |source_site_instance_group|'s process.  This
  // change could come from either the current RenderFrameHost or one of the
  // proxies (e.g., window.open that targets a RemoteFrame by name).  The
  // updated opener will be forwarded to any other RenderFrameProxies and
  // RenderFrames for this FrameTreeNode.
  void DidChangeOpener(
      const std::optional<blink::LocalFrameToken>& opener_frame_token,
      SiteInstanceGroup* source_site_instance_group);

  // Creates and initializes a RenderFrameHost. If |for_early_commit| is true
  // then this RenderFrameHost and its RenderFrame will be prepared knowing that
  // it will be committed immediately. If false the it will be committed later,
  // following the usual navigation path. |browsing_context_state| is the
  // BrowsingContextState that will be stored in the speculative
  // RenderFrameHost.
  std::unique_ptr<RenderFrameHostImpl> CreateSpeculativeRenderFrame(
      SiteInstanceImpl* instance,
      bool for_early_commit,
      const scoped_refptr<BrowsingContextState>& browsing_context_state);

  // Helper method to create and initialize a `RenderFrameProxyHost`.
  // `browsing_context_state` is the `BrowsingContextState` in which the newly
  // created `RenderFrameProxyHost` will be stored. If
  // `batched_proxy_ipc_sender` is not null, then proxy creation will be
  // delayed, and batched created later when
  // `BatchedProxyIPCSender::CreateAllProxies()` is called. The only
  // case where `batched_proxy_ipc_sender` is not null is when called by
  // `FrameTree::CreateProxiesForSiteInstance()`.
  void CreateRenderFrameProxy(
      SiteInstanceGroup* group,
      const scoped_refptr<BrowsingContextState>& browsing_context_state,
      BatchedProxyIPCSender* batched_proxy_ipc_sender = nullptr);

  // Similar to `CreateRenderFrameProxy` but also creates the minimal ancestor
  // chain of proxies in `group` to support a subframe. This only exists to
  // support CoopRelatedGroup proxy creation and should not be used for other
  // cases. It is CHECKed that `group` must be cross-BrowsingInstance.
  void CreateRenderFrameProxyAndAncestorChainIfNeeded(SiteInstanceGroup* group);

  // Creates proxies for a new child frame at FrameTreeNode |child| in all
  // SiteInstances for which the current frame has proxies.  This method is
  // called on the parent of a new child frame before the child leaves the
  // SiteInstance.
  void CreateProxiesForChildFrame(FrameTreeNode* child);

  // If |render_frame_host| is on the pending deletion list, this deletes it.
  // Returns whether it was deleted.
  bool DeleteFromPendingList(RenderFrameHostImpl* render_frame_host);

  // BackForwardCache/Prerender:
  // During a history navigation, unfreezes and swaps in a document from the
  // BackForwardCache, making it active. This mechanism is also used for
  // activating prerender page.
  void RestorePage(std::unique_ptr<StoredPage> stored_page);

  // Temporary method to allow reusing back-forward cache activation for
  // prerender activation. Similar to RestoreFromBackForwardCache(), but cleans
  // up the speculative RFH prior to activation.
  // TODO(crbug.com/40174053). This method might not be needed if we do
  // not create the speculative RFH in the first place for Prerender
  // activations.
  void ActivatePrerender(std::unique_ptr<StoredPage>);

  void ClearRFHsPendingShutdown();
  void ClearWebUIInstances();

  // Returns true if the current, or speculative, RenderFrameHost has a commit
  // pending for a cross-document navigation.
  bool HasPendingCommitForCrossDocumentNavigation() const;

  // Returns the routing id for a RenderFrameHost or RenderFrameProxyHost
  // that has the given SiteInstanceGroup and is associated with this
  // RenderFrameHostManager. Returns MSG_ROUTING_NONE if none is found.
  int GetRoutingIdForSiteInstanceGroup(SiteInstanceGroup* site_instance_group);

  // Returns the frame token for a RenderFrameHost or RenderFrameProxyHost
  // that has the given SiteInstanceGroup and is associated with this
  // RenderFrameHostManager. Returns std::nullopt if none is found. Note that
  // the FrameToken will internally be either a LocalFrameToken (if the frame is
  // a RenderFrameHost in the given |site_instance_group|) or a RemoteFrameToken
  // (if it is a RenderFrameProxyHost).
  std::optional<blink::FrameToken> GetFrameTokenForSiteInstanceGroup(
      SiteInstanceGroup* site_instance_group);

  // Notifies the RenderFrameHostManager that a new NavigationRequest has been
  // created and set in the FrameTreeNode so that it can speculatively create a
  // new RenderFrameHost (and potentially a new process) if needed.
  void DidCreateNavigationRequest(NavigationRequest* request);

  // Called (possibly several times) during a navigation to select or create an
  // appropriate RenderFrameHost for the provided URL.
  //
  // On success, returns a non-null pointer to a RenderFrameHost to use for the
  // navigation. The returned pointer always refers to either the current or the
  // speculative RenderFrameHost owned by `this`.
  //
  // Otherwise, on failure, returns an enum value denoting the reason for
  // failure.
  //
  // `reason` is an optional out-parameter that will be populated with
  // engineer-readable information describing the reason for the method
  // behavior.  The returned `reason` should fit into
  // base::debug::CrashKeySize::Size256.
  base::expected<RenderFrameHostImpl*, GetFrameHostForNavigationFailed>
  GetFrameHostForNavigation(
      NavigationRequest* request,
      BrowsingContextGroupSwap* browsing_context_group_swap,
      std::string* reason = nullptr);

  // Discards `speculative_render_frame_host_` if it exists, even if there are
  // NavigationRequests associated with it, including pending commit
  // navigations.
  // TODO(crbug.com/40186427): Don't allow this to be called when there
  // are pending commit cross-document navigations except for FrameTreeNode
  // detach or when the renderer process is gone, so that we don't have to
  // "undo" the commit that already happens in the renderer.
  void DiscardSpeculativeRFH(NavigationDiscardReason reason);

  // Determines whether any active navigations are associated with
  // `speculative_render_frame_host_` and if not, discards it.
  void DiscardSpeculativeRFHIfUnused(NavigationDiscardReason reason);

  // Clears the speculative RFH when a navigation is cancelled (for example, by
  // being replaced by a new navigation), returning ownership of the
  // `RenderFrameHost` to the caller for disposal.
  std::unique_ptr<RenderFrameHostImpl> UnsetSpeculativeRenderFrameHost(
      NavigationDiscardReason reason);

  // Used for FrameTreeNode teardown. This releases any pending views from the
  // speculative RFH (if any) to its respective RenderProcessHost before
  // discarding it. Unlike `UnsetSpeculativeRenderFrameHost()`, this does not
  // send any IPC to the renderer to delete the corresponding RenderFrame. The
  // caller must ensure that the RenderFrame has been or will be cleaned up.
  void DiscardSpeculativeRenderFrameHostForShutdown();

  // Called when the client changes whether the frame's owner element in the
  // embedder document should be collapsed, that is, remove from the layout as
  // if it did not exist. Never called for main frames. Only has an effect for
  // <iframe> owner elements.
  void OnDidChangeCollapsedState(bool collapsed);

  // Called on a frame to notify it that its out-of-process parent frame
  // changed a property (such as allowFullscreen) on its <iframe> element.
  // Sends updated FrameOwnerProperties to the RenderFrame and to all proxies,
  // skipping the parent process.
  void OnDidUpdateFrameOwnerProperties(
      const blink::mojom::FrameOwnerProperties& properties);

  void EnsureRenderViewInitialized(RenderViewHostImpl* render_view_host,
                                   SiteInstanceGroup* group);

  // Creates RenderFrameProxies and inactive RenderViewHosts for this frame's
  // FrameTree and for its opener chain in the given SiteInstanceGroup. This
  // allows other tabs to send cross-process JavaScript calls to their opener(s)
  // and to any other frames in the opener's FrameTree (e.g., supporting calls
  // like window.opener.opener.frames[x][y]).  Does not create proxies for the
  // subtree rooted at |skip_this_node| (e.g., if a node is being navigated, it
  // can be passed here to prevent proxies from being created for it, in
  // case it is in the same FrameTree as another node on its opener chain).
  // |browsing_context_state| is the BrowsingContextState that is used in the
  // speculative RenderFrameHost for cross browsing-instance navigations.
  void CreateOpenerProxies(
      SiteInstanceGroup* group,
      FrameTreeNode* skip_this_node,
      const scoped_refptr<BrowsingContextState>& browsing_context_state);

  // Ensure that this frame has proxies in all SiteInstances that can discover
  // this frame by name (e.g., via window.open("", "frame_name")).  See
  // https://crbug.com/511474.
  // |browsing_context_state| is the BrowsingContextState that is used in the
  // speculative RenderFrameHost for cross browsing-instance navigations.
  void CreateProxiesForNewNamedFrame(
      const scoped_refptr<BrowsingContextState>& browsing_context_state);

  // Returns a blink::FrameToken for the current FrameTreeNode's opener
  // node in the given SiteInstanceGroup.  May return a frame token of either a
  // RenderFrameHost (if opener's current or pending RFH has SiteInstanceGroup
  // |group|) or a RenderFrameProxyHost.  Returns std::nullopt if there is
  // no opener, or if the opener node doesn't have a proxy for |group|.
  std::optional<blink::FrameToken> GetOpenerFrameToken(
      SiteInstanceGroup* group);

  // Tells the |render_frame_host|'s renderer that its RenderFrame is being
  // swapped for a frame in another process, and that it should create a
  // `blink::RemoteFrame` to replace it using the |proxy| RenderFrameProxyHost.
  void SwapOuterDelegateFrame(RenderFrameHostImpl* render_frame_host,
                              RenderFrameProxyHost* proxy);

  // Sets the child RenderWidgetHostView for this frame, which must be part of
  // an inner FrameTree.
  void SetRWHViewForInnerFrameTree(RenderWidgetHostViewChildFrame* child_rwhv);

  // Executes a PageBroadcast Mojo method to every `blink::WebView` in the
  // FrameTree. This should only be called in the top-level
  // RenderFrameHostManager. The `callback` is called synchronously and the
  // `group_to_skip` won't be referenced after this method returns.
  void ExecutePageBroadcastMethod(PageBroadcastMethodCallback callback,
                                  SiteInstanceGroup* group_to_skip = nullptr);

  // Executes a RemoteMainFrame Mojo method to every instance in |proxy_hosts|.
  // This should only be called in the top-level RenderFrameHostManager.
  // The |callback| is called synchronously and the |group_to_skip| won't
  // be referenced after this method returns.
  void ExecuteRemoteFramesBroadcastMethod(
      RemoteFramesBroadcastMethodCallback callback,
      SiteInstanceGroup* group_to_skip = nullptr);

  // Returns a const reference to the map of proxy hosts. The keys are
  // SiteInstanceGroup IDs, the values are RenderFrameProxyHosts.
  const BrowsingContextState::RenderFrameProxyHostMap&
  GetAllProxyHostsForTesting() const {
    return render_frame_host_->browsing_context_state()->proxy_hosts();
  }

  // Called when the render process is gone for
  // `speculative_render_frame_host_`. Cancels the navigation and cleans up the
  // speculative RenderFrameHost because there is no longer a render process for
  // the navigation to commit into.
  void CleanupSpeculativeRfhForRenderProcessGone();

  // Updates the user activation state in all proxies of this frame.  For
  // more details, see the comment on FrameTreeNode::user_activation_state_.
  //
  // The |notification_type| parameter is used for histograms, only for the case
  // |update_state == kNotifyActivation|.
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type);

  // Sets up the necessary state for a new RenderViewHost.  If |proxy| is not
  // null, it creates a `blink::RemoteFrame` in the target renderer process
  // which is used to route IPC messages.  Returns early if the RenderViewHost
  // has already been initialized for another RenderFrameHost.
  bool InitRenderView(SiteInstanceGroup* site_instance_group,
                      RenderViewHostImpl* render_view_host,
                      RenderFrameProxyHost* proxy);

  // Returns the SiteInstance that should be used to host the navigation handled
  // by |navigation_request|.
  // Note: the SiteInstance returned by this function may not have an
  // initialized RenderProcessHost. It will only be initialized when
  // GetProcess() is called on the SiteInstance. In particular, calling this
  // function will never lead to a process being created for the navigation.
  //
  // |is_same_site| is a struct to cache the output of `IsNavigationSameSite()`
  // if/when it gets called. See `IsSameSiteGetter` for more details.
  //
  // |reason| is an optional out-parameter that will be populated with
  // engineer-readable information describing the reason for the method
  // behavior.  The returned |reason| should fit into
  // base::debug::CrashKeySize::Size256.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForNavigationRequest(
      NavigationRequest* navigation_request,
      IsSameSiteGetter& is_same_site,
      BrowsingContextGroupSwap* browsing_context_group_swap,
      std::string* reason = nullptr);

  // Calls GetSiteInstanceForNavigationRequest with an IsSameSiteGetter that
  // does not have a cached value.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForNavigationRequest(
      NavigationRequest* navigation_request,
      BrowsingContextGroupSwap* browsing_context_group_swap,
      std::string* reason = nullptr);

  // Helper to initialize the main RenderFrame if it's not initialized.
  // TODO(crbug.com/40615943): Remove this. For now debug URLs and
  // WebView JS execution are an exception to replacing all crashed frames for
  // RenderDocument. This is a no-op if the frame is already initialized.
  bool InitializeMainRenderFrameForImmediateUse();

  // Prepares the FrameTreeNode for attaching an inner WebContents. This step
  // may involve replacing |current_frame_host()| with a new RenderFrameHost
  // in the same SiteInstance as the parent frame. Calling this method will
  // dispatch beforeunload event if necessary.
  void PrepareForInnerDelegateAttach(
      RenderFrameHost::PrepareForInnerWebContentsAttachCallback callback);

  // When true the FrameTreeNode is preparing a RenderFrameHost for attaching an
  // inner Delegate. During this phase new navigation requests are ignored.
  bool is_attaching_inner_delegate() const {
    return attach_to_inner_delegate_state_ != AttachToInnerDelegateState::NONE;
  }

  // Called by the delegate at the end of the attaching process.
  void set_attach_complete() {
    attach_to_inner_delegate_state_ = AttachToInnerDelegateState::ATTACHED;
  }

  Delegate* delegate() { return delegate_; }

  // Collects the current page into StoredPage in preparation
  // for it to be moved to another FrameTree for prerender activation. After
  // this call, |current_frame_host_| will become null, which breaks many
  // invariants in the code, so the caller is responsible for destroying the
  // FrameTree immediately after this call.
  std::unique_ptr<StoredPage> TakePrerenderedPage();

  const blink::mojom::FrameReplicationState& current_replication_state() const {
    return render_frame_host_->browsing_context_state()
        ->current_replication_state();
  }

  // In certain cases, such as when navigating from a non-live (e.g., crashed
  // or initial) RenderFrameHost, the target speculative RenderFrameHost needs
  // to be swapped in and become the current RenderFrameHost before the
  // navigation commit.  This is a helper for performing this early
  // RenderFrameHost swap when necessary.  It should only be called once during
  // `request`'s lifetime.
  //
  // `is_called_after_did_start_navigation` specifies whether this is called
  // after DidStartNavigation has been dispatched to observers and after
  // WillStartRequest navigation throttle events have been processed, vs the
  // legacy call site at the very start of navigation and prior to these events.
  // TODO(crbug.com/40276607): Move the legacy early swaps to also happen after
  // DidStartNavigation and remove the `is_called_after_did_start_navigation`
  // param (i.e., the param should always be true).
  void PerformEarlyRenderFrameHostSwapIfNeeded(
      NavigationRequest* request,
      bool is_called_after_did_start_navigation);

  base::WeakPtr<RenderFrameHostManager> GetWeakPtr();

 private:
  friend class NavigatorTest;
  friend class RenderFrameHostManagerTest;
  friend class RenderFrameHostTester;
  friend class TestWebContents;

  enum class SiteInstanceRelation {
    // A SiteInstance in a different browsing instance from the current.
    UNRELATED,
    // A SiteInstance in a different BrowsingInstance, but in the same
    // CoopRelatedGroup. Only used for COOP: restrict-properties
    // navigations.
    RELATED_IN_COOP_GROUP,
    // A SiteInstance in the same browsing instance as the current.
    RELATED,
    // A pre-existing SiteInstance that might or might not be in the same
    // browsing instance as the current. Only used when |existing_site_instance|
    // is specified.
    PREEXISTING,
  };

  enum class AttachToInnerDelegateState {
    // There is no inner delegate attached through FrameTreeNode and no
    // attaching is in progress.
    NONE,
    // A frame is being prepared for attaching.
    PREPARE_FRAME,
    // An inner delegate attached to the delegate of this manager.
    ATTACHED
  };

  // Stores information regarding a SiteInstance targeted at a specific UrlInfo
  // to allow for comparisons without having to actually create new instances.
  // It can point to an existing one or store the details needed to create a new
  // one.
  struct CONTENT_EXPORT SiteInstanceDescriptor {
    // Constructor used for PREEXISTING relations.
    explicit SiteInstanceDescriptor(SiteInstanceImpl* site_instance);

    // Constructor used for UNRELATED/RELATED_IN_COOP_GROUP/RELATED relations.
    SiteInstanceDescriptor(UrlInfo dest_url_info,
                           SiteInstanceRelation relation_to_current);

    // Set with an existing SiteInstance to be reused.
    raw_ptr<SiteInstanceImpl> existing_site_instance;

    // In case `existing_site_instance` is null, specify a destination URL.
    UrlInfo dest_url_info;

    // Specifies how the new site is related to the current BrowsingInstance.
    // This is PREEXISTING iff `existing_site_instance` is defined.
    SiteInstanceRelation relation;
  };

  // Returns a BrowsingContextGroupSwap describing if we need to change
  // BrowsingInstance for the navigation from `current_effective_url` to
  // `destination_url_info`. This can happen for a variety of reasons, including
  // differences in security level (WebUI pages to regular pages), COOP headers,
  // or to accommodate for the BackForwardCache. The structure also contains
  // extra information about the consequences of such a swap, including the need
  // to clear proxies or the window's name.
  //
  // `source_instance` is the SiteInstance of the frame that initiated the
  // navigation. `current_instance` is the SiteInstance of the frame that is
  // currently navigating. `destination_instance` is a predetermined
  // SiteInstance that will be used for `destination_url_info` if not
  // null - we will swap BrowsingInstances if it's in a different
  // BrowsingInstance than the current one.
  //
  // If there is no current NavigationEntry, then `current_is_view_source_mode`
  // should be the same as `dest_is_view_source_mode`.
  //
  // UrlInfo uses the effective URL here, since that's what is used in the
  // SiteInstance's site and when we later call IsSameSite.  If there is no
  // current NavigationEntry, check the current SiteInstance's site, which might
  // already be committed to a Web UI URL (such as the NTP). Note that we don't
  // pass the effective URL for destination URL here and instead calculate the
  // destination's effective URL within the function because some methods called
  // in the function like IsNavigationSameSite expects a non-effective URL.
  BrowsingContextGroupSwap ShouldSwapBrowsingInstancesForNavigation(
      const GURL& current_effective_url,
      bool current_is_view_source_mode,
      SiteInstanceImpl* source_instance,
      SiteInstanceImpl* current_instance,
      SiteInstanceImpl* destination_instance,
      const UrlInfo& destination_url_info,
      bool destination_is_view_source_mode,
      ui::PageTransition transition,
      NavigationRequest::ErrorPageProcess error_page_process,
      bool is_reload,
      bool is_same_document,
      IsSameSiteGetter& is_same_site,
      CoopSwapResult coop_swap_result,
      bool was_server_redirect,
      bool should_replace_current_entry,
      bool has_rel_opener);

  BrowsingContextGroupSwap ShouldProactivelySwapBrowsingInstance(
      const UrlInfo& destination_url_info,
      bool is_reload,
      IsSameSiteGetter& is_same_site,
      bool should_replace_current_entry,
      bool has_rel_opener);

  // Returns the SiteInstance to use for the navigation.
  //
  // This is a helper function for GetSiteInstanceForNavigationRequest.
  scoped_refptr<SiteInstanceImpl> GetSiteInstanceForNavigation(
      const UrlInfo& dest_url_info,
      SiteInstanceImpl* source_instance,
      SiteInstanceImpl* dest_instance,
      SiteInstanceImpl* candidate_instance,
      ui::PageTransition transition,
      NavigationRequest::ErrorPageProcess error_page_process,
      bool is_reload,
      bool is_same_document,
      IsSameSiteGetter& is_same_site,
      bool dest_is_view_source_mode,
      bool was_server_redirect,
      CoopSwapResult coop_swap_result,
      bool should_replace_current_entry,
      bool force_new_browsing_instance,
      bool has_rel_opener,
      BrowsingContextGroupSwap* browsing_context_group_swap,
      std::string* reason);

  // Returns a descriptor of the appropriate SiteInstance object for the given
  // `dest_url_info`, possibly reusing the current, source or destination
  // SiteInstance. The actual SiteInstance can then be obtained calling
  // ConvertToSiteInstance with the descriptor.
  //
  // `source_instance` is the SiteInstance of the frame that initiated the
  // navigation. `current_instance` is the SiteInstance of the frame that is
  // currently navigating. `dest_instance` is a predetermined SiteInstance that
  // will be used if not null.
  // For example, if you have a parent frame A, which has a child frame B, and
  // A is trying to change the src attribute of B, this will cause a navigation
  // where the source SiteInstance is A and B is the current SiteInstance.
  //
  // This is a helper function for GetSiteInstanceForNavigation.
  SiteInstanceDescriptor DetermineSiteInstanceForURL(
      const UrlInfo& dest_url_info,
      SiteInstanceImpl* source_instance,
      SiteInstanceImpl* current_instance,
      SiteInstanceImpl* dest_instance,
      ui::PageTransition transition,
      NavigationRequest::ErrorPageProcess error_page_process,
      IsSameSiteGetter& is_same_site,
      BrowsingContextGroupSwap browsing_context_group_swap,
      bool was_server_redirect,
      std::string* reason);

  // Returns whether we can use the given `dest_instance` or if it is not
  // suitable anymore.
  //
  // This is a helper function for GetSiteInstanceForNavigation.
  bool CanUseDestinationInstance(
      const UrlInfo& dest_url_info,
      SiteInstanceImpl* current_instance,
      SiteInstanceImpl* dest_instance,
      NavigationRequest::ErrorPageProcess error_page_process,
      const BrowsingContextGroupSwap& browsing_context_group_swap,
      bool was_server_redirect);

  // Returns true if a navigation to |dest_url| that uses the specified
  // PageTransition in the current frame is allowed to swap BrowsingInstances.
  // DetermineSiteInstanceForURL() uses this helper to determine when it is
  // allowed to swap BrowsingInstances to avoid unneeded process sharing.  See
  // https://crbug.com/803367.
  //
  // Note that this is different from
  // ShouldSwapBrowsingInstancesForNavigation(), which identifies cases in
  // which a BrowsingInstance swap is *required* (e.g., for security). This
  // function only identifies cases where a BrowsingInstance swap *may* be
  // performed to optimize process placement.  In particular, this is true for
  // certain browser-initiated transitions for main frame navigations.
  //
  // Returning true here doesn't imply that DetermineSiteInstanceForURL() will
  // swap BrowsingInstances.  For example, this swap will not be done for
  // same-site navigations, for history navigations, or when starting from an
  // uninitialized SiteInstance.
  bool IsBrowsingInstanceSwapAllowedForPageTransition(
      ui::PageTransition transition,
      const GURL& dest_url);

  // Returns true if we can use `source_instance` for `dest_url_info`.
  bool CanUseSourceSiteInstance(
      const UrlInfo& dest_url_info,
      SiteInstanceImpl* source_instance,
      bool was_server_redirect,
      NavigationRequest::ErrorPageProcess error_page_process,
      std::string* reason = nullptr);

  // Converts a SiteInstanceDescriptor to the actual SiteInstance it describes.
  // If a |candidate_instance| is provided (is not nullptr) and it matches the
  // description, it is returned as is.
  scoped_refptr<SiteInstanceImpl> ConvertToSiteInstance(
      const SiteInstanceDescriptor& descriptor,
      SiteInstanceImpl* candidate_instance);

  // Returns true if `candidate` is currently same site with `dest_url_info`.
  // This method is a special case for handling hosted apps in this object. Most
  // code should call IsNavigationSameSite() on `candidate` instead of this
  // method.
  bool IsCandidateSameSite(RenderFrameHostImpl* candidate,
                           const UrlInfo& dest_url_info);

  // Creates a new WebUI object for `request`, which will commit in
  // `dest_site_instance`. `use_current_rfh` will be true if the navigation will
  // reuse the current RFH instead of using a new speculative RFH. If an
  // existing RFH is reused, this function might notify its WebUI (if it exists)
  // that it is being reused.
  void CreateWebUIForNavigationIfNeeded(NavigationRequest* request,
                                        SiteInstanceImpl* dest_site_instance,
                                        bool use_current_rfh);

  // Ensure that we have created all needed proxies for a new RFH with
  // SiteInstance in |new_group|:
  // (1) create RVHs and proxies for the new RFH's opener chain if we are
  // staying in the same BrowsingInstance;
  // (2) Create proxies for the new RFH's SiteInstance's group in its own frame
  // tree.
  // |recovering_without_early_commit| is true if we are reviving a crashed
  // render frame by creating a proxy and committing later rather than doing an
  // immediate commit.
  // |browsing_context_state| is the BrowsingContextState that is used in the
  // speculative RenderFrameHost for cross browsing-instance navigations.
  // TODO(https://crbug.com/40202433): Formalize an invariant that this function
  // is a no-op if |old_group| and |new_group| are the same.
  void CreateProxiesForNewRenderFrameHost(
      SiteInstanceGroup* old_group,
      SiteInstanceGroup* new_group,
      bool recovering_without_early_commit,
      const scoped_refptr<BrowsingContextState>& browsing_context_state);

  // Traverse the opener chain and populate `opener_frame_trees` with
  // all FrameTrees accessible by following frame openers of nodes in the
  // given node's FrameTree. `opener_frame_trees` is ordered so that openers
  // of smaller-indexed entries point to larger-indexed entries (i.e., this
  // node's FrameTree is at index 0, its opener's FrameTree is at index 1,
  // etc). If the traversal encounters a node with an opener pointing to a
  // FrameTree that has already been traversed (such as when there's a cycle),
  // the node is added to `nodes_with_back_links`.
  //
  // This function does not recursively iterate on trees living in a different
  // BrowsingInstance from `site_instance_group`, which may have maintained an
  // opener using COOP: restrict-properties. When such openers are encountered,
  // they are added to `cross_browsing_context_group_openers`. Tests can set
  // `site_instance_group` to null to iterate through all trees.
  void CollectOpenerFrameTrees(
      SiteInstanceGroup* site_instance_group,
      std::vector<FrameTree*>* opener_frame_trees,
      std::unordered_set<FrameTreeNode*>* nodes_with_back_links,
      std::unordered_set<FrameTreeNode*>* cross_browsing_context_group_openers);

  // Create RenderFrameProxies and inactive RenderViewHosts in the given
  // SiteInstanceGroup for the current node's FrameTree. Used as a helper
  // function in CreateOpenerProxies for creating proxies in each FrameTree on
  // the opener chain. Don't create proxies for the subtree rooted at
  // |skip_this_node|. |browsing_context_state| is the BrowsingContextState that
  // is used in the speculative RenderFrameHost for cross browsing-instance
  // navigations.
  void CreateOpenerProxiesForFrameTree(
      SiteInstanceGroup* group,
      FrameTreeNode* skip_this_node,
      const scoped_refptr<BrowsingContextState>& browsing_context_state);

  // The different types of RenderFrameHost creation that can occur.
  // See CreateRenderFrameHost for how these influence creation.
  enum class CreateFrameCase {
    // Adding a child to an existing frame in the tree.
    kInitChild,
    // Creating the first frame in a frame tree.
    kInitRoot,
    // Preparing to navigate to another frame.
    kCreateSpeculative,
  };

  // Creates a RenderFrameHost. This uses an existing a RenderViewHost in the
  // same SiteInstance if it exists or creates a new one (a new one will only be
  // created if this is a root or child local root).
  // The `frame_routing_id` and `frame_remote` are both valid or not together,
  // as they are valid when the renderer-side frame is already created.
  // TODO(crbug.com/40121874): Eliminate or rename
  // renderer_initiated_creation.
  std::unique_ptr<RenderFrameHostImpl> CreateRenderFrameHost(
      CreateFrameCase create_frame_case,
      SiteInstanceImpl* site_instance,
      int32_t frame_routing_id,
      mojo::PendingAssociatedRemote<mojom::Frame> frame_remote,
      const blink::LocalFrameToken& frame_token,
      const blink::DocumentToken& document_token,
      base::UnguessableToken devtools_frame_token,
      bool renderer_initiated_creation,
      scoped_refptr<BrowsingContextState> browsing_context_state);

  // Create and initialize a speculative RenderFrameHost for an ongoing
  // navigation. It might be destroyed and re-created later if the navigation is
  // redirected to a different SiteInstance. |recovering_without_early_commit|
  // is true if we are reviving a crashed render frame by creating a proxy and
  // committing later rather than doing an immediate commit.
  bool CreateSpeculativeRenderFrameHost(SiteInstanceImpl* old_instance,
                                        SiteInstanceImpl* new_instance,
                                        bool recovering_without_early_commit);

  // Initialization for RenderFrameHost uses the same sequence as InitRenderView
  // above.
  bool InitRenderFrame(RenderFrameHostImpl* render_frame_host);

  // Find the `blink::FrameToken` of the frame or proxy that this frame will
  // replace or std::nullopt if there is none. When initializing a new
  // RenderFrame for `render_frame_host`, it may be replacing a RenderFrameProxy
  // or another RenderFrame in the renderer or recovering from a crash.
  // `existing_proxy` is the proxy for `this` in the destination renderer,
  // nullptr if there is no proxy. `render_frame_host` is used only for sanity
  // checking.
  std::optional<blink::FrameToken> GetReplacementFrameToken(
      RenderFrameProxyHost* existing_proxy,
      RenderFrameHostImpl* render_frame_host) const;

  // Helper to reinitialize the RenderFrame, RenderView, and the opener chain
  // for the provided |render_frame_host|.  Used when the |render_frame_host|
  // needs to be reused for a new navigation, but it is not live.
  bool ReinitializeMainRenderFrame(RenderFrameHostImpl* render_frame_host);

  // Sets the |pending_rfh| to be the active one. Called when the pending
  // RenderFrameHost commits.
  //
  // This function is also called when restoring an entry from BackForwardCache.
  // In that case, |pending_rfh| is the RenderFrameHost to be restored, and
  // |pending_stored_page| provides additional state to be restored, such as
  // proxies.
  // |clear_proxies_on_commit| Indicates if the proxies and opener must be
  // removed during the commit. This can happen following some BrowsingInstance
  // swaps, such as those for COOP.
  // |allow_paint_holding| Indicates whether paint holding is allowed.
  void CommitPending(std::unique_ptr<RenderFrameHostImpl> pending_rfh,
                     std::unique_ptr<StoredPage> pending_stored_page,
                     bool clear_proxies_on_commit,
                     bool allow_paint_holding);

  // Helper to call CommitPending() in all necessary cases.
  void CommitPendingIfNecessary(RenderFrameHostImpl* render_frame_host,
                                bool was_caused_by_user_gesture,
                                bool is_same_document_navigation,
                                bool clear_proxies_on_commit,
                                bool allow_paint_holding);

  // Runs the unload handler in the old RenderFrameHost, after the new
  // RenderFrameHost has committed.  |old_render_frame_host| will either be
  // deleted or put on the pending delete list during this call.
  void UnloadOldFrame(
      std::unique_ptr<RenderFrameHostImpl> old_render_frame_host);

  // Discards a RenderFrameHost that was never made active (for active ones
  // UnloadOldFrame is used instead).
  void DiscardUnusedFrame(
      std::unique_ptr<RenderFrameHostImpl> render_frame_host);

  // Helper method to set the active RenderFrameHost. Returns the old
  // RenderFrameHost and updates counts.
  std::unique_ptr<RenderFrameHostImpl> SetRenderFrameHost(
      std::unique_ptr<RenderFrameHostImpl> render_frame_host);

  // After a renderer process crash we'd have marked the host as invisible, so
  // we need to set the visibility of the new View to the correct value here
  // after reload.
  void EnsureRenderFrameHostVisibilityConsistent();

  // Similarly to visibility, we need to ensure RenderWidgetHost and
  // RenderWidget know about page focus.
  void EnsureRenderFrameHostPageFocusConsistent();

  // When current RenderFrameHost is not in its parent SiteInstance, this method
  // will destroy the frame and replace it with a new RenderFrameHost in the
  // parent frame's SiteInstance. Either way, this will eventually invoke
  // |attach_inner_delegate_callback_| with a pointer to |render_frame_host_|
  // which is then safe for use with WebContents::AttachToOuterWebContentsFrame.
  void CreateNewFrameForInnerDelegateAttachIfNecessary();

  // Called when the result of preparing the FrameTreeNode for attaching an
  // inner delegate is known. When successful, |render_frame_host_| can be used
  // for attaching the inner Delegate.
  void NotifyPrepareForInnerDelegateAttachComplete(bool success);

  NavigationControllerImpl& GetNavigationController();

  void PrepareForCollectingPage(
      RenderFrameHostImpl* main_render_frame_host,
      StoredPage::RenderViewHostImplSafeRefSet* render_view_hosts,
      BrowsingContextState::RenderFrameProxyHostMap* proxy_hosts);

  // Collects all of the page-related state currently owned by
  // RenderFrameHostManager (including relevant RenderViewHosts and
  // RenderFrameProxyHosts) into a StoredPage object to be
  // stored in back-forward cache or to activate the prerenderer.
  std::unique_ptr<StoredPage> CollectPage(
      std::unique_ptr<RenderFrameHostImpl> main_render_frame_host);

  // Update `render_frame_host`'s opener in the renderer process in response to
  // the opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener(RenderFrameHostImpl* render_frame_host);

  // For use in creating RenderFrameHosts.
  raw_ptr<FrameTreeNode> frame_tree_node_;

  // Our delegate, not owned by us. Guaranteed non-null.
  raw_ptr<Delegate> delegate_;

  // Our RenderFrameHost which is responsible for all communication with a child
  // RenderFrame instance.
  // For now, RenderFrameHost keeps a RenderViewHost in its SiteInstance alive.
  // Eventually, RenderViewHost will be replaced with a page context.
  std::unique_ptr<RenderFrameHostImpl> render_frame_host_;

  // A set of RenderFrameHosts waiting to shut down after swapping out.
  using RFHPendingDeleteSet =
      std::set<std::unique_ptr<RenderFrameHostImpl>, base::UniquePtrComparator>;
  RFHPendingDeleteSet pending_delete_hosts_;

  // Stores a speculative RenderFrameHost which is created early in a navigation
  // so a renderer process can be started in parallel, if needed.
  // This is purely a performance optimization and is not required for correct
  // behavior. The speculative RenderFrameHost might be discarded later on if
  // the final URL's SiteInstance isn't compatible with the one used to create
  // it.
  std::unique_ptr<RenderFrameHostImpl> speculative_render_frame_host_;

  // After being set in RestoreFromBackForwardCache() or ActivatePrerenderer(),
  // the stored page is immediately consumed in CommitPending().
  std::unique_ptr<StoredPage> stored_page_to_restore_;

  // This callback is used when attaching an inner Delegate to |delegate_|
  // through |frame_tree_node_|.
  RenderFrameHost::PrepareForInnerWebContentsAttachCallback
      attach_inner_delegate_callback_;
  AttachToInnerDelegateState attach_to_inner_delegate_state_ =
      AttachToInnerDelegateState::NONE;

  base::WeakPtrFactory<RenderFrameHostManager> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_MANAGER_H_
