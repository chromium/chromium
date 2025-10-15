// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_INFO_H_
#define CONTENT_BROWSER_URL_INFO_H_

#include <optional>

#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "content/browser/agent_cluster_key.h"
#include "content/browser/origin_agent_cluster_isolation_state.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/process_selection_user_data.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_proto.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// This struct is used to package a GURL together with extra state required to
// make SiteInstance/process allocation decisions, e.g. whether the url's
// origin or site is requesting isolation as determined by response headers in
// the corresponding NavigationRequest. The extra state is generally most
// relevant when navigation to the URL is in progress, since once placed into a
// SiteInstance, the extra state will be available via SiteInfo. Otherwise,
// most callsites requiring a UrlInfo can create with a GURL, specifying kNone
// for |origin_isolation_request|. Some examples of where passing kNone for
// |origin_isolation_request| is safe are:
// * at DidCommitNavigation time, since at that point the SiteInstance has
//   already been picked and the navigation can be considered finished,
// * before a response is received (the only way to request isolation is via
//   response headers), and
// * outside of a navigation.
//
// If UrlInfo::origin_isolation_state_request is nullopt, that does *not* imply
// that the URL's origin will not be isolated, and vice versa.  The isolation
// decision involves both response headers and consistency within a
// BrowsingInstance, and once we decide on the isolation outcome for an origin,
// it won't change for the lifetime of the BrowsingInstance.
//
// To check whether a frame ends up in a site-isolated process, use
// SiteInfo::RequiresDedicatedProcess() on its SiteInstance's SiteInfo.  To
// check whether a frame ends up being origin-isolated in a separate process
// (e.g., due to the Origin-Agent-Cluster header), use
// SiteInfo::agent_cluster_key()::IsOriginKeyed(). To check whether the
// origin-isolation is due to OAC specifically, check SiteInfo::oac_status().
//
// Note: it is not expected that this struct will be exposed in content/public.
class IsolationContext;
class UrlInfoInit;

struct CONTENT_EXPORT UrlInfo {
 public:
  // For isolated sandboxed iframes, when per-document mode is used, we
  // assign each sandboxed SiteInstance a unique identifier to prevent other
  // same-site/same-origin frames from re-using the same SiteInstance. This
  // identifier is used to indicate that the sandbox id is not in use.
  static const int64_t kInvalidUniqueSandboxId;

  UrlInfo();  // Needed for inclusion in SiteInstanceDescriptor.
  UrlInfo(const UrlInfo& other);
  explicit UrlInfo(const UrlInfoInit& init);
  ~UrlInfo();

  // Used to convert GURL to UrlInfo in tests where opt-in isolation is not
  // being tested.
  static UrlInfo CreateForTesting(const GURL& url_in,
                                  std::optional<StoragePartitionConfig>
                                      storage_partition_config = std::nullopt);

  // Returns whether this UrlInfo is requesting an origin-keyed process for
  // `url`'s origin due to the OriginAgentCluster header, or whether it should
  // try to use an origin-keyed process by default within the given `context`,
  // in cases without an explicit header.
  bool RequestsOriginKeyedProcess(const IsolationContext& context) const;

  // Returns whether this UrlInfo is requesting site isolation for its site in
  // response to the Cross-Origin-Opener-Policy header. See
  // https://chromium.googlesource.com/chromium/src/+/main/docs/process_model_and_site_isolation.md#Partial-Site-Isolation
  // for details.
  bool requests_coop_isolation() const { return is_coop_isolation_requested; }

  // Returns whether this UrlInfo is for a page that should be cross-origin
  // isolated.
  bool IsIsolated() const;

  using TraceProto = perfetto::protos::pbzero::UrlInfo;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;

  GURL url;

  // This field tracks the desired OriginAgentClusterIsolationState requested
  // by the document. The OriginAgentClusterIsolationState tracks whether
  // the document should be given an origin-keyed agent cluster in the renderer
  // process (logical isolation), and whether this logical isolation should be
  // backed by actual process isolation. This is only set when the document has
  // an actual OAC header.
  std::optional<OriginAgentClusterIsolationState> oac_header_request;

  // True if the Cross-Origin-Opener-Policy header has triggered a hint to turn
  // on site isolation for `url`'s site.
  bool is_coop_isolation_requested = false;

  // True if this resource is served from the prefetch cache, and its success
  // may have been influenced by cross-site state. Such responses may require
  // special handling to make it harder to detect that this has happened.
  bool is_prefetch_with_cross_site_contamination = false;

  // This allows overriding the origin of |url| for process assignment purposes
  // in certain very special cases.
  // - The navigation to |url| is through loadDataWithBaseURL (e.g., in a
  //   <webview> tag or on Android Webview): this will be the base origin
  //   provided via that API.
  // - For renderer-initiated about:blank navigations: this will be the
  //   initiator's origin that about:blank should inherit.
  // - data: URLs that will be rendered (e.g. not downloads) that do NOT use
  //   loadDataWithBaseURL: this will be the value of the tentative origin to
  //   commit, which we will use to keep the nonce of the opaque origin
  //   consistent across a navigation.
  // - All other cases: this will be nullopt.
  //
  // TODO(alexmos): Currently, this is also used to hold the origin committed
  // by the renderer at DidCommitNavigation() time, for use in commit-time URL
  // and origin checks that require a UrlInfo.  Investigate whether there's a
  // cleaner way to organize these checks.  See https://crbug.com/1320402.
  std::optional<url::Origin> origin;

  // If url is being loaded in a frame that is in a origin-restricted sandboxed,
  // then this flag will be true.
  bool is_sandboxed = false;

  // Only used when `is_sandboxed` is true, this unique identifier allows for
  // per-document SiteInfo grouping.
  int64_t unique_sandbox_id = kInvalidUniqueSandboxId;

  // The StoragePartitionConfig that should be used when loading content from
  // |url|. If absent, ContentBrowserClient::GetStoragePartitionConfig will be
  // used to determine which StoragePartitionConfig to use.
  //
  // If present, this value will be used as the StoragePartitionConfig in the
  // SiteInfo, regardless of its validity. SiteInstances created from a UrlInfo
  // containing a StoragePartitionConfig that isn't compatible with the
  // BrowsingInstance that the SiteInstance should belong to will lead to a
  // CHECK failure.
  std::optional<StoragePartitionConfig> storage_partition_config;

  // Pages may choose to isolate themselves more strongly than the web's
  // default, thus allowing access to APIs that would be difficult to
  // safely expose otherwise. "Cross-origin isolation", for example, requires
  // assertion of a Cross-Origin-Opener-Policy and
  // Cross-Origin-Embedder-Policy, and unlocks SharedArrayBuffer.
  // When we haven't yet been to the network or inherited properties that are
  // sufficient to know the future isolation state - we are in a speculative
  // state - this member will be empty.
  std::optional<WebExposedIsolationInfo> web_exposed_isolation_info;

  // Indicates that the URL directs to PDF content, which should be isolated
  // from other types of content.  On Android, this can only be true when a PDF
  // NativePage is created for a main frame navigation.
  bool is_pdf = false;

  // The CrossOriginIsolationKey to use for the navigation. This represents the
  // isolation requested by the page itself through the use of COOP, COEP and
  // DIP. Right now, this is only set when DocumentIsolationPolicy is enabled,
  // but it should eventually for COOP and COEP. It will eventually replace
  // WebExposedIsolationInfo.
  std::optional<AgentClusterKey::CrossOriginIsolationKey>
      cross_origin_isolation_key;

  // The ProcessSelectionUserData to use for process selection. This user data
  // is populated by ProcessSelectionDeferringConditions to communicate embedder
  // defined process selection information to the process selection logic.
  //
  // This is a SafeRef to an object owned by NavigationRequest, so a UrlInfo
  // instance must not outlive the NavigationRequest it was created from.
  // A SafeRef is used instead of a WeakPtr to intentionally cause a crash if
  // this lifetime assumption is violated. This is because process selection
  // must always happen during the navigation, so any use of this data after
  // the NavigationRequest is destroyed is a bug.
  std::optional<base::SafeRef<ProcessSelectionUserData>>
      process_selection_user_data;

  // Any new UrlInfo fields should be added to UrlInfoInit as well, and the
  // UrlInfo constructor that takes a UrlInfoInit should be updated as well.
};

class CONTENT_EXPORT UrlInfoInit {
 public:
  UrlInfoInit() = delete;
  explicit UrlInfoInit(const GURL& url);
  explicit UrlInfoInit(const UrlInfo& base);
  ~UrlInfoInit();

  UrlInfoInit& operator=(const UrlInfoInit&) = delete;

  UrlInfoInit& WithOACHeaderRequest(
      std::optional<OriginAgentClusterIsolationState> oac_header_request);
  UrlInfoInit& WithCOOPSiteIsolation(bool requests_coop_isolation);
  UrlInfoInit& WithCrossSitePrefetchContamination(bool contaminated);
  UrlInfoInit& WithOrigin(const url::Origin& origin);
  UrlInfoInit& WithSandbox(bool is_sandboxed);
  UrlInfoInit& WithUniqueSandboxId(int unique_sandbox_id);
  UrlInfoInit& WithStoragePartitionConfig(
      std::optional<StoragePartitionConfig> storage_partition_config);
  UrlInfoInit& WithWebExposedIsolationInfo(
      std::optional<WebExposedIsolationInfo> web_exposed_isolation_info);
  UrlInfoInit& WithIsPdf(bool is_pdf);
  UrlInfoInit& WithCrossOriginIsolationKey(
      const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
          cross_origin_isolation_key);
  UrlInfoInit& WithProcessSelectionUserData(
      base::SafeRef<ProcessSelectionUserData> process_selection_user_data);

  const std::optional<url::Origin>& origin() { return origin_; }

 private:
  UrlInfoInit(UrlInfoInit&);

  friend UrlInfo;

  GURL url_;
  std::optional<OriginAgentClusterIsolationState> oac_header_request_;
  bool requests_coop_isolation_ = false;
  bool is_prefetch_with_cross_site_contamination_ = false;
  std::optional<url::Origin> origin_;
  bool is_sandboxed_ = false;
  int64_t unique_sandbox_id_ = UrlInfo::kInvalidUniqueSandboxId;
  std::optional<StoragePartitionConfig> storage_partition_config_;
  std::optional<WebExposedIsolationInfo> web_exposed_isolation_info_;
  bool is_pdf_ = false;
  std::optional<AgentClusterKey::CrossOriginIsolationKey>
      cross_origin_isolation_key_;
  std::optional<base::SafeRef<ProcessSelectionUserData>>
      process_selection_user_data_;

  // Any new fields should be added to the UrlInfoInit(UrlInfo) constructor.
};  // class UrlInfoInit

}  // namespace content

#endif  // CONTENT_BROWSER_URL_INFO_H_
