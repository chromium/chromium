// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_STATUS_H_
#define CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_STATUS_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/security/coop/coop_swap_result.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/mojom/blocked_by_response_reason.mojom.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace net {
class IsolationInfo;
class NetworkAnonymizationKey;
}  // namespace net

namespace content {
class CrossOriginOpenerPolicyReporter;
class FrameTreeNode;
class NavigationRequest;
class StoragePartition;
struct ChildProcessTerminationInfo;

// Helper function that returns whether the BrowsingInstance should change
// following COOP rules defined in:
//
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e#changes-to-navigation
CONTENT_EXPORT CoopSwapResult
ShouldSwapBrowsingInstanceForCrossOriginOpenerPolicy(
    network::mojom::CrossOriginOpenerPolicyValue initiator_coop,
    const url::Origin& initiator_origin,
    bool is_navigation_from_initial_empty_document,
    network::mojom::CrossOriginOpenerPolicyValue destination_coop,
    const url::Origin& destination_origin);

// Groups information used to apply COOP during navigations. This class will be
// used to trigger a number of mechanisms such as BrowsingInstance switch or
// reporting.
class CrossOriginOpenerPolicyStatus : public RenderProcessHostObserver {
 public:
  explicit CrossOriginOpenerPolicyStatus(NavigationRequest* navigation_request);
  ~CrossOriginOpenerPolicyStatus() override;

  // Sanitize the COOP header from the `response`.
  // Return an error, and swap browsing context group when COOP is used on
  // sandboxed popups.
  std::optional<network::mojom::BlockedByResponseReason> SanitizeResponse(
      network::mojom::URLResponseHead* response);

  // Called when receiving a redirect or the final response.
  void EnforceCOOP(
      const network::CrossOriginOpenerPolicy& response_coop,
      const url::Origin& response_origin,
      const net::NetworkAnonymizationKey& network_anonymization_key);

  // Force a browsing instance swap, even if the COOP rules do not require it.
  // Calling this function is safe because it can only tighten security.
  // This is used by _unfencedTop in fenced frames to ensure that navigations
  // leaving the fenced context create a new browsing instance.
  void ForceBrowsingInstanceSwap() {
    browsing_instance_swap_result_ = CoopSwapResult::kSwap;
  }

  CoopSwapResult browsing_instance_swap_result() const {
    return browsing_instance_swap_result_;
  }

  // The virtual browsing context group of the document to commit. Initially,
  // the navigation inherits the virtual browsing context group of the current
  // document. Updated when the report-only COOP of a response would result in
  // a browsing context group swap if enforced.
  int virtual_browsing_context_group() const {
    return virtual_browsing_context_group_;
  }

  // Used to keep track of browsing context group swaps that would happen if
  // COOP had a value of same-origin-allow-popups by default.
  int soap_by_default_virtual_browsing_context_group() const {
    return soap_by_default_virtual_browsing_context_group_;
  }

  // The COOP used when comparing to the COOP and origin of a response. At the
  // beginning of the navigation, it is the COOP of the current document. After
  // receiving any kind of response, including redirects, it is the COOP of the
  // last response.
  const network::CrossOriginOpenerPolicy& current_coop() const {
    return current_coop_;
  }

  const std::vector<base::UnguessableToken>&
  TransientReportingSourcesForTesting() {
    return transient_reporting_sources_;
  }

  std::unique_ptr<CrossOriginOpenerPolicyReporter> TakeCoopReporter();

  // Called when a RenderFrameHost has been created to use its process's
  // storage partition. Until then, the reporter uses the current process
  // storage partition.
  void UpdateReporterStoragePartition(StoragePartition* storage_partition);

 private:
  // If the process crashes/exited before CrossOriginOpenerPolicyStatus is
  // destructed, clean up transient reporting sources.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  void SetReportingEndpoints(const url::Origin& response_origin,
                             StoragePartition* storage_partition,
                             const base::UnguessableToken& reporting_source,
                             const net::IsolationInfo& isolation_info);
  // Remove any transient Reporting-Endpoints endpoint created for COOP
  // reporting during navigation before the document loads.
  // There could be multiple due to redirect chains.
  void ClearTransientReportingSources();
  // Make sure COOP is relevant or clear the COOP headers.
  void SanitizeCoopHeaders(
      const GURL& response_url,
      network::mojom::URLResponseHead* response_head) const;

  // The NavigationRequest which owns this object.
  const raw_ptr<NavigationRequest> navigation_request_;

  // Tracks the FrameTreeNode in which this navigation is taking place.
  raw_ptr<const FrameTreeNode> frame_tree_node_;

  // Track the previous document's RenderProcessHost. This instance acquires
  // reporting endpoints from it, and will use it to release them in its
  // destructor.
  raw_ptr<RenderProcessHost> previous_document_rph_;
  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      previous_document_rph_observation_{this};

  // Tracks whether the new document created by the navigation needs to be
  // created in a different BrowsingContext group. This is updated after every
  // redirect, and after receiving the final response.
  CoopSwapResult browsing_instance_swap_result_ = CoopSwapResult::kNoSwap;

  int virtual_browsing_context_group_;

  // Keeps track of browsing context group switches that would happen if COOP
  // had a value of same-origin-allow-popups-by-default.
  int soap_by_default_virtual_browsing_context_group_;

  // Whether this is a navigation away from the initial empty document. Note
  // that this might be false in case it happens on a initial empty document
  // whose input stream has been opened (e.g. due to document.open()), causing
  // it to no longer be considered as the initial empty document per the HTML
  // specification.
  // For more details, see FrameTreeNode::is_on_initial_empty_document() and
  // https://html.spec.whatwg.org/multipage/origin.html#browsing-context-group-switches-due-to-cross-origin-opener-policy:still-on-its-initial-about:blank-document
  const bool is_navigation_from_initial_empty_document_;

  network::CrossOriginOpenerPolicy current_coop_;

  // The origin used when comparing to the COOP and origin of a response. At
  // the beginning of the navigation, it is the origin of the current document.
  // After receiving any kind of response, including redirects, it is the origin
  // of the last response.
  url::Origin current_origin_;

  // The current URL, to use for reporting. At the beginning of the navigation,
  // it is the URL of the current document. After receiving any kind of
  // response, including redirects, it is the URL of the last response.
  GURL current_url_;

  // Indicates whether to use the reporter in the current RenderFrameHost to
  // send a report for a navigation away from a current response. If false, the
  // |coop_reporter| field from this CrossOriginOpenerPolicyStatus should be
  // used instead.
  bool use_current_document_coop_reporter_ = true;

  // The reporter currently in use by COOP.
  std::unique_ptr<CrossOriginOpenerPolicyReporter> coop_reporter_;

  // Transient reporting sources created for Reporting-Endpoints header during
  // this navigation before it fails or commits.
  std::vector<base::UnguessableToken> transient_reporting_sources_;

  // Whether the current context tracked by this CrossOriginOpenerPolicy is the
  // source of the current navigation. This is updated every time we receive a
  // redirect, as redirects are considered the source of the navigation to the
  // next URL.
  bool is_navigation_source_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SECURITY_COOP_CROSS_ORIGIN_OPENER_POLICY_STATUS_H_
