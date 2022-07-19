// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/types/pass_key.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/prerender/prerender_attributes.h"
#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/browser/renderer_host/stored_page.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "url/gurl.h"

namespace content {

class FrameTree;
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
class CONTENT_EXPORT PrerenderHost : public WebContentsObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called on the page activation.
    virtual void OnActivated() {}

    // Called from the PrerenderHost's destructor. The observer should drop any
    // reference to the host.
    virtual void OnHostDestroyed() {}
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class FinalStatus {
    kActivated = 0,
    kDestroyed = 1,
    kLowEndDevice = 2,
    kCrossOriginRedirect = 3,
    kCrossOriginNavigation = 4,
    kInvalidSchemeRedirect = 5,
    kInvalidSchemeNavigation = 6,
    kInProgressNavigation = 7,
    // kNavigationRequestFailure = 8,  // No longer used.
    kNavigationRequestBlockedByCsp = 9,
    kMainFrameNavigation = 10,
    kMojoBinderPolicy = 11,
    // kPlugin = 12,  // No longer used.
    kRendererProcessCrashed = 13,
    kRendererProcessKilled = 14,
    kDownload = 15,
    kTriggerDestroyed = 16,
    kNavigationNotCommitted = 17,
    kNavigationBadHttpStatus = 18,
    kClientCertRequested = 19,
    kNavigationRequestNetworkError = 20,
    kMaxNumOfRunningPrerendersExceeded = 21,
    kCancelAllHostsForTesting = 22,
    kDidFailLoad = 23,
    kStop = 24,
    kSslCertificateError = 25,
    kLoginAuthRequested = 26,
    kUaChangeRequiresReload = 27,
    kBlockedByClient = 28,
    kAudioOutputDeviceRequested = 29,
    kMixedContent = 30,
    kTriggerBackgrounded = 31,
    // Break down into kEmbedderTriggeredAndSameOriginRedirected and
    // kEmbedderTriggeredAndCrossOriginRedirected for investigation.
    // kEmbedderTriggeredAndRedirected = 32,
    kEmbedderTriggeredAndSameOriginRedirected = 33,
    kEmbedderTriggeredAndCrossOriginRedirected = 34,
    kEmbedderTriggeredAndDestroyed = 35,
    kMaxValue = kEmbedderTriggeredAndDestroyed,
  };

  PrerenderHost(const PrerenderAttributes& attributes,
                WebContents& web_contents,
                PreloadingAttemptImpl* attempt);
  ~PrerenderHost() override;

  PrerenderHost(const PrerenderHost&) = delete;
  PrerenderHost& operator=(const PrerenderHost&) = delete;
  PrerenderHost(PrerenderHost&&) = delete;
  PrerenderHost& operator=(PrerenderHost&&) = delete;

  // Returns false if prerendering hasn't been started.
  bool StartPrerendering();

  // WebContentsObserver implementation:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(Visibility visibility) override;
  void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) override;

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
  // uses this before abandoning the host.
  void RecordFinalStatus(base::PassKey<PrerenderHostRegistry>,
                         FinalStatus status);

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
  // when the PrerenderHost is first navigated, which happens immediately
  // after creation.
  void SetInitialNavigation(NavigationRequest* navigation);
  absl::optional<int64_t> GetInitialNavigationId() const;

  // Returns true if the given `url` indicates the same destination to the
  // initial_url.
  bool IsUrlMatch(const GURL& url) const;

  // Returns absl::nullopt iff prerendering is initiated by the browser (not by
  // a renderer using Speculation Rules API).
  absl::optional<url::Origin> initiator_origin() const {
    return attributes_.initiator_origin;
  }
  const GURL& initiator_url() const { return attributes_.initiator_url; }

  bool IsBrowserInitiated() { return attributes_.IsBrowserInitiated(); }

  int frame_tree_node_id() const { return frame_tree_node_id_; }

  int initiator_frame_tree_node_id() const {
    return attributes_.initiator_frame_tree_node_id;
  }

  bool is_ready_for_activation() const { return is_ready_for_activation_; }

  const absl::optional<FinalStatus>& final_status() const {
    return final_status_;
  }

  PrerenderTriggerType trigger_type() const { return attributes_.trigger_type; }
  const std::string& embedder_histogram_suffix() const {
    return attributes_.embedder_histogram_suffix;
  }

 private:
  class PageHolder;

  // Records the status to UMA and UKM. `initiator_ukm_id` represents the page
  // that starts prerendering and `prerendered_ukm_id` represents the
  // prerendered page. `prerendered_ukm_id` is valid after the page is
  // activated.
  void RecordFinalStatus(FinalStatus status,
                         ukm::SourceId initiator_ukm_id,
                         ukm::SourceId prerendered_ukm_id);

  void CreatePageHolder(WebContentsImpl& web_contents);

  // Asks the registry to cancel prerendering.
  void Cancel(FinalStatus status);

  // Sets the PreloadingTriggeringOutcome, PreloadingEligibility,
  // PreloadingFailureReason for PreloadingAttempt associated with this
  // PrerenderHost.
  void SetTriggeringOutcome(PreloadingTriggeringOutcome outcome);
  void SetEligibility(PreloadingEligibility eligibility);
  void SetFailureReason(FinalStatus status);

  bool AreBeginNavigationParamsCompatibleWithNavigation(
      const blink::mojom::BeginNavigationParams& potential_activation);
  bool AreCommonNavigationParamsCompatibleWithNavigation(
      const blink::mojom::CommonNavigationParams& potential_activation);

  const PrerenderAttributes attributes_;

  // Indicates if `page_holder_` is ready for activation.
  bool is_ready_for_activation_ = false;

  // The ID of the root node of the frame tree for the prerendered page `this`
  // is hosting. Since PrerenderHost has 1:1 correspondence with FrameTree,
  // this is also used for the ID of this PrerenderHost.
  int frame_tree_node_id_ = RenderFrameHost::kNoFrameTreeNodeId;

  absl::optional<FinalStatus> final_status_;

  std::unique_ptr<PageHolder> page_holder_;

  base::ObserverList<Observer> observers_;

  // Stores the attempt corresponding to this prerender to log various metrics.
  raw_ptr<PreloadingAttemptImpl, DanglingUntriaged> attempt_;

  // Navigation parameters for the navigation which loaded the main document of
  // the prerendered page, copied immediately after BeginNavigation when
  // throttles are created. They will be compared with the navigation parameters
  // of the potential activation when attempting to reserve the prerender host
  // for a navigation.
  blink::mojom::BeginNavigationParamsPtr begin_params_;
  blink::mojom::CommonNavigationParamsPtr common_params_;

  // Holds the navigation ID for the main frame initial navigation.
  absl::optional<int64_t> initial_navigation_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HOST_H_
