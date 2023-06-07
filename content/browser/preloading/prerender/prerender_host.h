// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_controller_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "url/gurl.h"

namespace blink {
class EnabledClientHints;
}  // namespace blink

namespace network::mojom {
enum class WebClientHintsType;
}  // namespace network::mojom

namespace content {

class DevToolsPrerenderAttempt;
class FrameTreeNode;
class PrerenderCancellationReason;
class PrerenderHostRegistry;
class RenderFrameHostImpl;
class WebContentsImpl;

// Prerender2:
// PrerenderHost creates a new FrameTree in WebContents associated with the page
// that triggered prerendering and starts prerendering. Then NavigationRequest
// is expected to find this host from PrerenderHostRegistry and activate the
// prerendered page upon navigation. This is created per request from a renderer
// process via SpeculationHostImpl or will directly be created for
// browser-initiated prerendering (this code path is not implemented yet). This
// is owned by PrerenderHostRegistry.
class CONTENT_EXPORT PrerenderHost : public FrameTree::Delegate,
                                     public NavigationControllerDelegate {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This enum corresponds to
  // PrerenderActivationNavigationParamsMatch in
  // tools/metrics/histograms/enums.xml
  enum class ActivationNavigationParamsMatch {
    kOk = 0,
    kInitiatorFrameToken = 1,
    kHttpRequestHeader = 2,
    kCacheLoadFlags = 3,
    kLoadFlags = 4,
    kSkipServiceWorker = 5,
    kMixedContentContextType = 6,
    kIsFormSubmission = 7,
    kSearchableFormUrl = 8,
    kSearchableFormEncoding = 9,
    kTrustTokenParams = 10,
    kWebBundleToken = 11,
    kRequestContextType = 12,
    kImpressionHasValue = 13,
    kInitiatorOrigin = 14,
    kTransition = 15,
    kNavigationType = 16,
    kBaseUrlForDataUrl = 17,
    kPostData = 18,
    kStartedFromContextMenu = 19,
    kInitiatorOriginTrialFeature = 20,
    kHrefTranslate = 21,
    kIsHistoryNavigationInNewChildFrame = 22,
    // kReferrerPolicy = 23,  Obsolete
    kRequestDestination = 24,
    kMaxValue = kRequestDestination,
  };

  // Observes a triggered prerender. Note that the observer should overlive the
  // prerender host instance, or be removed properly upon destruction.
  class Observer : public base::CheckedObserver {
   public:
    // Called on the page activation.
    virtual void OnActivated() {}

    // Called from the PrerenderHost's destructor. The observer should drop any
    // reference to the host.
    virtual void OnHostDestroyed(PrerenderFinalStatus status) {}
  };

  // Returns the PrerenderHost that the given `frame_tree_node` is in, if it is
  // being prerendered.
  static PrerenderHost* GetFromFrameTreeNodeIfPrerendering(
      FrameTreeNode& frame_tree_node);
  // Similar to GetPrerenderHostFromFrameTreeNode() but `frame_tree_node` must
  // be in prerendering.
  static PrerenderHost& GetFromFrameTreeNode(FrameTreeNode& frame_tree_node);

  // Checks whether two headers are the same in a case-insensitive and
  // order-insensitive way.
  // TODO(https://crbug.com/1443922): Migrate this method into
  // `HttpRequestHeaders`.
  static bool IsActivationHeaderMatch(
      const net::HttpRequestHeaders& potential_activation_headers,
      const net::HttpRequestHeaders& prerender_headers);

  PrerenderHost(const PrerenderAttributes& attributes,
                WebContentsImpl& web_contents,
                base::WeakPtr<PreloadingAttempt> attempt,
                std::unique_ptr<DevToolsPrerenderAttempt> devtools_attempt);
  ~PrerenderHost() override;

  PrerenderHost(const PrerenderHost&) = delete;
  PrerenderHost& operator=(const PrerenderHost&) = delete;
  PrerenderHost(PrerenderHost&&) = delete;
  PrerenderHost& operator=(PrerenderHost&&) = delete;

  // FrameTree::Delegate

  // TODO(https://crbug.com/1199682): Correctly handle load events. Ignored for
  // now as it confuses WebContentsObserver instances because they can not
  // distinguish between the different FrameTrees.

  void LoadingStateChanged(LoadingState new_state) override {}
  void DidStartLoading(FrameTreeNode* frame_tree_node) override {}
  void DidStopLoading() override;
  bool IsHidden() override;
  FrameTree* LoadingTree() override;
  int GetOuterDelegateFrameTreeNodeId() override;
  RenderFrameHostImpl* GetProspectiveOuterDocument() override;
  bool IsPortal() override;
  void SetFocusedFrame(FrameTreeNode* node, SiteInstanceGroup* source) override;

  // NavigationControllerDelegate
  void NotifyNavigationStateChangedFromController(
      InvalidateTypes changed_flags) override {}
  void NotifyBeforeFormRepostWarningShow() override {}
  void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {}
  void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) override {}
  void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) override {}
  void NotifyNavigationEntriesDeleted() override {}
  void ActivateAndShowRepostFormWarningDialog() override;
  bool ShouldPreserveAbortedURLs() override;
  WebContents* DeprecatedGetWebContents() override;
  void UpdateOverridingUserAgent() override {}

  NavigationControllerImpl& GetNavigationController() {
    return frame_tree_->controller();
  }

  // Returns false if prerendering hasn't been started.
  bool StartPrerendering();

  // Called from PrerenderHostRegistry::DidStartNavigation(). It may reset
  // `is_ready_for_activation_` flag when the main frame navigation happens in
  // a prerendered page.
  void DidStartNavigation(NavigationHandle* navigation_handle);

  // Called from PrerenderHostRegistry::DidFinishNavigation(). If the navigation
  // request is for the main frame and doesn't have an error, then the host will
  // be ready for activation.
  void DidFinishNavigation(NavigationHandle* navigation_handle);

  // Activates the prerendered page and returns StoredPage containing the page.
  // This must be called after this host gets ready for activation.
  std::unique_ptr<StoredPage> Activate(NavigationRequest& navigation_request);

  // Returns true if the navigation params that were used in the initial
  // prerender navigation (i.e., in StartPrerendering()) match the navigation
  // params in `navigation_request`. This function can be used to determine
  // whether `navigation_request` may be eligible to activate this
  // PrerenderHost.
  bool AreInitialPrerenderNavigationParamsCompatibleWithNavigation(
      NavigationRequest& navigation_request);

  bool IsFramePolicyCompatibleWithPrimaryFrameTree();

  // Returns the main RenderFrameHost of the prerendered page.
  // This must be called after StartPrerendering() and before Activate().
  RenderFrameHostImpl* GetPrerenderedMainFrameHost();

  // Returns the frame tree for the prerendered page `this` is hosting.
  FrameTree& GetPrerenderFrameTree();

  // Tells the reason of the destruction of this host. PrerenderHostRegistry
  // uses this before abandoning the host. Exposed to PrerenderHostRegistry
  // only.
  void RecordFailedFinalStatus(base::PassKey<PrerenderHostRegistry>,
                               const PrerenderCancellationReason& reason);

  // Called by PrerenderHostRegistry to report that this prerender host is
  // successfully activated.
  void RecordActivation(NavigationRequest& navigation_request);

  enum class LoadingOutcome {
    kLoadingCompleted,
    kPrerenderingCancelled,
  };

  // Waits until the page load finishes. Returns the loading status indicating
  // how the operation was finished.
  LoadingOutcome WaitForLoadStopForTesting();

  const GURL& GetInitialUrl() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // The initial navigation is set by the PrerenderNavigationThrottle
  // when the PrerenderHost is first navigated.
  void SetInitialNavigation(NavigationRequest* navigation);
  absl::optional<int64_t> GetInitialNavigationId() const;

  // Returns true if the given `url` indicates the same destination to the
  // initial_url.
  bool IsUrlMatch(const GURL& url) const;

  // Called when the prerender pages asks the client to change the Accept Client
  // Hints. The instruction applies to the prerendering page before activation,
  // and will be persisted to the global setting upon activation.
  void OnAcceptClientHintChanged(
      const url::Origin& origin,
      const std::vector<network::mojom::WebClientHintsType>& client_hints_type);

  // Updates the given `client_hints`.
  void GetAllowedClientHintsOnPage(
      const url::Origin& origin,
      blink::EnabledClientHints* client_hints) const;

  // Returns absl::nullopt iff prerendering is initiated by the browser (not by
  // a renderer using Speculation Rules API).
  absl::optional<url::Origin> initiator_origin() const {
    return attributes_.initiator_origin;
  }

  absl::optional<base::UnguessableToken> initiator_devtools_navigation_token()
      const {
    return attributes_.initiator_devtools_navigation_token;
  }

  const GURL& prerendering_url() const { return attributes_.prerendering_url; }

  bool IsBrowserInitiated() { return attributes_.IsBrowserInitiated(); }

  int frame_tree_node_id() const { return frame_tree_node_id_; }

  base::WeakPtr<WebContents> initiator_web_contents() {
    return attributes_.initiator_web_contents;
  }

  int initiator_frame_tree_node_id() const {
    return attributes_.initiator_frame_tree_node_id;
  }

  int initiator_ukm_id() const { return attributes_.initiator_ukm_id; }

  bool is_ready_for_activation() const { return is_ready_for_activation_; }

  const absl::optional<PrerenderFinalStatus>& final_status() const {
    return final_status_;
  }

  PrerenderTriggerType trigger_type() const { return attributes_.trigger_type; }
  const std::string& embedder_histogram_suffix() const {
    return attributes_.embedder_histogram_suffix;
  }

  base::WeakPtr<PreloadingAttempt> preloading_attempt() { return attempt_; }

 private:
  void RecordFailedFinalStatusImpl(const PrerenderCancellationReason& reason);

  // Asks the registry to cancel prerendering.
  void Cancel(PrerenderFinalStatus status);

  // Sets the PreloadingTriggeringOutcome, PreloadingEligibility,
  // PreloadingFailureReason for PreloadingAttempt associated with this
  // PrerenderHost.
  void SetTriggeringOutcome(PreloadingTriggeringOutcome outcome);
  void SetFailureReason(PrerenderFinalStatus status);

  ActivationNavigationParamsMatch
  AreBeginNavigationParamsCompatibleWithNavigation(
      const blink::mojom::BeginNavigationParams& potential_activation);
  ActivationNavigationParamsMatch
  AreCommonNavigationParamsCompatibleWithNavigation(
      const blink::mojom::CommonNavigationParams& potential_activation);

  const PrerenderAttributes attributes_;

  // Indicates if this PrerenderHost is ready for activation.
  bool is_ready_for_activation_ = false;

  // The ID of the root node of the frame tree for the prerendered page `this`
  // is hosting. Since PrerenderHost has 1:1 correspondence with FrameTree,
  // this is also used for the ID of this PrerenderHost.
  int frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;

  absl::optional<PrerenderFinalStatus> final_status_;

  base::ObserverList<Observer> observers_;

  // Stores the attempt corresponding to this prerender to log various metrics.
  // We use a WeakPtr here to avoid inadvertent UAF. `attempt_` can get deleted
  // before `PrerenderHostRegistry::DeleteAbandonedHosts` is scheduled.
  base::WeakPtr<PreloadingAttempt> attempt_;
  std::unique_ptr<DevToolsPrerenderAttempt> devtools_attempt_;
  // Navigation parameters for the navigation which loaded the main document of
  // the prerendered page, copied immediately after BeginNavigation when
  // throttles are created. They will be compared with the navigation parameters
  // of the potential activation when attempting to reserve the prerender host
  // for a navigation.
  blink::mojom::BeginNavigationParamsPtr begin_params_;
  blink::mojom::CommonNavigationParamsPtr common_params_;

  // Stores the client hints type that applies to this page.
  base::flat_map<url::Origin, std::vector<network::mojom::WebClientHintsType>>
      client_hints_type_;

  // Holds the navigation ID for the main frame initial navigation.
  absl::optional<int64_t> initial_navigation_id_;

  // WebContents where this prerenderer is embedded. Keeping a reference is safe
  // as WebContentsImpl owns PrerenderHostRegistry, which in turn owns
  // PrerenderHost.
  const raw_ref<WebContentsImpl> web_contents_;

  // Used for testing, this closure is only set when waiting a page to be either
  // loaded for prerendering. |frame_tree_| provides us with a trigger for when
  // the page is loaded.
  base::OnceCallback<void(PrerenderHost::LoadingOutcome)>
      on_wait_loading_finished_;

  // Frame tree created for the prerenderer to load the page and prepare it for
  // a future activation. During activation, the prerendered page will be taken
  // out from |frame_tree_| and moved over to |web_contents_|'s primary frame
  // tree, while |frame_tree_| will be deleted.
  std::unique_ptr<FrameTree> frame_tree_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_H_
