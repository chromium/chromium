// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INFO_H_
#define CONTENT_BROWSER_SITE_INFO_H_

#include "content/browser/agent_cluster_key.h"
#include "content/browser/url_info.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class IsolationContext;
class StoragePartitionConfig;
struct UrlInfo;

// SiteInfo represents the principal of a SiteInstance. All documents and
// workers within a SiteInstance are considered part of this principal and will
// share a renderer process. Any two documents within the same browsing context
// group (i.e., BrowsingInstance) that are allowed to script each other *must*
// have the same SiteInfo principal, so that they end up in the same renderer
// process.
//
// As a result, SiteInfo is primarily defined in terms of "site URL," which is
// often the scheme plus the eTLD+1 of a URL. This allows same-site URLs to
// always share a process even when document.domain is modified. However, some
// site URLs can be finer grained (e.g., origins) or coarser grained (e.g.,
// file://). See |site_url()| for more considerations.
//
// In the future, we may add more information to SiteInfo for cases where the
// site URL is not sufficient to identify which process a document belongs in.
// For example, origin isolation (https://crbug.com/1067389) will introduce a
// 'keying' bit ('site' or 'origin') to avoid an ambiguity between sites and
// origins, and it will be possible for two SiteInstances with different keying
// values to have the same site URL. It is important that any extra members of
// SiteInfo do not cause two documents that can script each other to end up in
// different SiteInfos and thus different processes.
class CONTENT_EXPORT SiteInfo {
 public:
  // Helper to create a SiteInfo that will be used for an error page.  This is
  // used only when error page isolation is enabled.  Note that when site
  // isolation for guests is enabled, an error page SiteInfo may also be
  // associated with a guest. Similarly, when process isolation for fenced
  // frames is enabled, error pages inside fenced frames will be isolated from
  // embedders.
  //
  // `web_exposed_isolation_info` describes the isolation state of the error
  // page. Top-level error pages use a non-isolated WebExposedIsolationInfo,
  // while subframes and embedded content (including fenced frames, protals,
  // etc.) inherit this value from their embedder.
  static SiteInfo CreateForErrorPage(
      const StoragePartitionConfig storage_partition_config,
      bool is_guest,
      bool is_fenced,
      const WebExposedIsolationInfo& web_exposed_isolation_info,
      WebExposedIsolationLevel web_exposed_isolation_level);

  // Helper to create a SiteInfo for default SiteInstances.  Default
  // SiteInstances are used for non-isolated sites on platforms without strict
  // site isolation, such as on Android.  They may also be used on desktop
  // platforms when strict site isolation is explicitly turned off (e.g., via
  // switches::kDisableSiteIsolation).
  static SiteInfo CreateForDefaultSiteInstance(
      const IsolationContext& isolation_context,
      const StoragePartitionConfig storage_partition_config,
      const WebExposedIsolationInfo& web_exposed_isolation_info);

  // Helper to create a SiteInfo for a <webview> guest.  This helper can be
  // used for a new guest associated with a specific StoragePartitionConfig
  // (prior to navigations).
  static SiteInfo CreateForGuest(
      BrowserContext* browser_context,
      const StoragePartitionConfig& partition_config);

  // This function returns a SiteInfo with the appropriate site_url and
  // process_lock_url computed. This function can only be called on the UI
  // thread because it must be able to compute an effective URL.
  static SiteInfo Create(const IsolationContext& isolation_context,
                         const UrlInfo& url_info);

  // Similar to the function above, but this method can only be called on the
  // IO thread. All fields except for the site_url should be the same as
  // the other method. The site_url field will match the process_lock_url
  // in the object returned by this function. This is because we cannot compute
  // the effective URL from the IO thread.
  //
  // `url_info` MUST contain a StoragePartitionConfig because we can't ask the
  // embedder which StoragePartitionConfig to use from the IO thread.
  //
  // NOTE: Do not use this method unless there is a very clear and good reason
  // to do so. It primarily exists to facilitate the creation of ProcessLocks
  // from any thread. ProcessLocks do not rely on the site_url field so the
  // difference between this method and Create() does not cause problems for
  // that usecase.
  static SiteInfo CreateOnIOThread(const IsolationContext& isolation_context,
                                   const UrlInfo& url_info);

  // Method to make creating SiteInfo objects for tests easier. It is a thin
  // wrapper around Create() that uses UrlInfo::CreateForTesting(),
  // and WebExposedIsolationInfo::CreateNonIsolated() to generate the
  // information that is not provided.
  static SiteInfo CreateForTesting(const IsolationContext& isolation_context,
                                   const GURL& url);

  // Returns the site of a given |origin|.  Unlike Create(), this does
  // not utilize effective URLs, isolated origins, or other special logic.  It
  // only translates an origin into a site (i.e., scheme and eTLD+1) and is
  // used internally by GetSiteForURLInternal().  For making process model
  // decisions, Create() should be used instead.
  static GURL GetSiteForOrigin(const url::Origin& origin);

  // Returns the site URL derived from an opaque data: origin. This has the form
  // data:<serialized nonce>. This is only to be called for data: URLs with
  // opaque origins, and will crash otherwise, e.g. in LoadDataWithBaseURL,
  // where the base URL is not an opaque origin.
  static GURL GetOriginBasedSiteURLForDataURL(const url::Origin& origin);

  // Returns a StoragePartitionConfig for the specified URL. Note that the URL
  // can be both a site URL that was generated by a SiteInfo or a regular
  // user-provided URL.
  //
  // Note: New callers of this method should be discouraged. New code should
  // have access to a SiteInfo object and call GetStoragePartitionConfig() on
  // that. For cases where code just needs the StoragePartition for a user
  // provided URL or origin, it should use
  // BrowserContext::GetStoragePartitionForUrl() instead of directly calling
  // this method.
  static StoragePartitionConfig GetStoragePartitionConfigForUrl(
      BrowserContext* browser_context,
      const GURL& site_or_regular_url);

  // Computes the web-exposed cross-origin isolation capability that should be
  // used for a SiteInfo with the given WebExposedIsolationInfo and UrlInfo.
  // This will be the same as the BrowsingInstance's WebExposedIsolationInfo
  // except for agents that are cross-origin to an "isolated application"
  // BrowsingInstance.
  //
  // See ProcessLock::GetWebExposedIsolationLevel() for more information.
  static WebExposedIsolationLevel ComputeWebExposedIsolationLevel(
      const WebExposedIsolationInfo& web_exposed_isolation_info,
      const UrlInfo& url_info);

  // Computes the web-exposed cross-origin isolation capability that should be
  // used for a SiteInfo with the given WebExposedIsolationInfo that isn't
  // locked to a site.
  static WebExposedIsolationLevel ComputeWebExposedIsolationLevelForEmptySite(
      const WebExposedIsolationInfo& web_exposed_isolation_info);

  // Initializes |storage_partition_config_| with a value appropriate for
  // |browser_context|.
  explicit SiteInfo(BrowserContext* browser_context);
  // The SiteInfo constructor should take in all values needed for comparing two
  // SiteInfos, to help ensure all creation sites are updated accordingly when
  // new values are added. The private function MakeSecurityPrincipalKey()
  // should be updated accordingly.
  SiteInfo(const GURL& site_url,
           const GURL& process_lock_url,
           bool requires_origin_keyed_process,
           bool requires_origin_keyed_process_by_default,
           bool is_sandboxed,
           int unique_sandbox_id,
           const StoragePartitionConfig storage_partition_config,
           const WebExposedIsolationInfo& web_exposed_isolation_info,
           WebExposedIsolationLevel web_exposed_isolation_level,
           bool is_guest,
           bool does_site_request_dedicated_process_for_coop,
           bool is_jit_disabled,
           bool are_v8_optimizations_disabled,
           bool is_pdf,
           bool is_fenced,
           const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
               cross_origin_isolation_key);
  SiteInfo() = delete;
  SiteInfo(const SiteInfo& rhs);
  ~SiteInfo();

  // This function returns a new SiteInfo which is equivalent to the original,
  // except that (1) is_origin_keyed is false, and (2) the remaining SiteInfo
  // state is used to compute a new SiteInfo from a UrlInfo reconstructed from
  // the original SiteInfo, minus any OAC opt-in request.
  SiteInfo GetNonOriginKeyedEquivalentForMetrics(
      const IsolationContext& isolation_context) const;

  // Returns the site URL associated with all of the documents and workers in
  // this principal, as described above.
  //
  // NOTE: In most cases, code should be performing checks against the origin
  // returned by |RenderFrameHost::GetLastCommittedOrigin()|. In contrast, the
  // GURL returned by |site_url()| should not be considered authoritative
  // because:
  // - A SiteInstance can host pages from multiple sites if "site per process"
  //   is not enabled and the SiteInstance isn't hosting pages that require
  //   process isolation (e.g. WebUI or extensions).
  // - Even with site per process, the site URL is not an origin: while often
  //   derived from the origin, it only contains the scheme and the eTLD + 1,
  //   i.e. an origin with the host "deeply.nested.subdomain.example.com"
  //   corresponds to a site URL with the host "example.com".
  // - When origin isolation is in use, there may be multiple SiteInstance with
  //   the same site_url() but that differ in other properties.
  const GURL& site_url() const { return site_url_; }

  // Returns the AgentClusterKey of the execution contexts within this SiteInfo.
  const std::optional<AgentClusterKey>& agent_cluster_key() const {
    return agent_cluster_key_;
  }

  // Returns the URL which should be used in a SetProcessLock call for this
  // SiteInfo's process.  This is the same as |site_url_| except for cases
  // involving effective URLs, such as hosted apps.  In those cases, this URL is
  // a site URL that is computed without the use of effective URLs.
  //
  // NOTE: This URL is currently set even in cases where this SiteInstance's
  //       process is *not* going to be locked to it.  Callers should be careful
  //       to consider this case when comparing lock URLs;
  //       ShouldLockProcessToSite() may be used to determine whether the
  //       process lock will actually be used.
  //
  // TODO(alexmos): See if we can clean this up and not set |process_lock_url_|
  //                if the SiteInstance's process isn't going to be locked.
  const GURL& process_lock_url() const { return process_lock_url_; }

  // Returns whether this SiteInfo requires an origin-keyed process, such as for
  // an OriginAgentCluster response header. This resolves an ambiguity of
  // whether a process with a lock_url() like "https://foo.example" is allowed
  // to include "https://sub.foo.example" or not. In opt-in isolation, it is
  // possible for example.com to be isolated, and sub.example.com not be
  // isolated. In contrast, if command-line isolation is used to isolate
  // example.com, then sub.example.com is also (automatically) isolated.
  // Also note that opt-in isolated origins will include ports (if non-default)
  // in their site urls.
  bool requires_origin_keyed_process() const {
    return requires_origin_keyed_process_;
  }

  // If requires_origin_keyed_process() is true, this function indicates if the
  // origin-keyed process is being used by default (e.g., via
  // kOriginKeyedProcessesByDefault), rather than due to an opt-in OAC header.
  bool requires_origin_keyed_process_by_default() const {
    return requires_origin_keyed_process_by_default_;
  }

  // The following accessor is for the `is_sandboxed` flag, which is true when
  // this SiteInfo is for an origin-restricted-sandboxed iframe.
  bool is_sandboxed() const { return is_sandboxed_; }

  // Returns either kInvalidUniqueSandboxId or the unique sandbox id provided
  // when this SiteInfo was created. The latter case only occurs when
  // `is_sandboxed` is true, and kIsolateSandboxedIframes was specified with
  // the per-document grouping parameter.
  int unique_sandbox_id() const { return unique_sandbox_id_; }

  // Returns the web-exposed isolation mode of the BrowsingInstance hosting
  // SiteInstances with this SiteInfo. The level of isolation which a page
  // opts-into has implications for the set of other pages which can live in
  // this SiteInstance, process allocation decisions, and API exposure in the
  // page's JavaScript context.
  const WebExposedIsolationInfo& web_exposed_isolation_info() const {
    return web_exposed_isolation_info_;
  }

  // Returns the web-exposed isolation capability of agents with this SiteInfo,
  // ignoring the 'cross-origin-isolated' permissions policy. This should be
  // used in conjunction with permissions policy to determine whether a frame
  // can access APIs gated behind cross-origin isolation.
  //
  // This may return a lower isolation level than
  // `web_exposed_isolation_info_` because "Isolated Application" cannot be
  // delegated cross-origin.
  WebExposedIsolationLevel web_exposed_isolation_level() const {
    return web_exposed_isolation_level_;
  }

  bool is_guest() const { return is_guest_; }
  bool is_error_page() const;
  bool is_jit_disabled() const { return is_jit_disabled_; }
  bool are_v8_optimizations_disabled() const {
    return are_v8_optimizations_disabled_;
  }
  bool is_pdf() const { return is_pdf_; }
  bool is_fenced() const { return is_fenced_; }

  // See comments on `does_site_request_dedicated_process_for_coop_` for more
  // details.
  bool does_site_request_dedicated_process_for_coop() const {
    return does_site_request_dedicated_process_for_coop_;
  }

  // Returns true if the site_url() is empty.
  bool is_empty() const { return site_url().possibly_invalid_spec().empty(); }

  SiteInfo& operator=(const SiteInfo& rhs);

  // Determine whether one SiteInfo represents the same security principal as
  // another SiteInfo.  Note that this does not necessarily translate to an
  // equality comparison of all the fields in SiteInfo (see comments in the
  // implementation).
  bool IsSamePrincipalWith(const SiteInfo& other) const;

  // Returns true if all fields in `other` match the corresponding fields in
  // this object.
  bool IsExactMatch(const SiteInfo& other) const;

  // Determines how a ProcessLock based on this SiteInfo compares to a
  // ProcessLock based on the `other` SiteInfo. Note that this doesn't just
  // compare all SiteInfo fields, e.g. it doesn't use site_url_ since that
  // may include effective URLs.
  // Returns -1 if `this` < `other`, 1 if `this` > `other`, 0 otherwise.
  int ProcessLockCompareTo(const SiteInfo& other) const;

  // Note: equality operators are defined in terms of IsSamePrincipalWith().
  bool operator==(const SiteInfo& other) const;
  bool operator!=(const SiteInfo& other) const;

  // Defined to allow this object to act as a key for std::map and std::set.
  // Note that the key is determined based on what distinguishes one security
  // principal from another (see IsSamePrincipalWith) and does not necessarily
  // include all the fields in SiteInfo.
  bool operator<(const SiteInfo& other) const;

  // Returns a string representation of this SiteInfo principal.
  std::string GetDebugString() const;

  // Returns true if pages loaded with this SiteInfo ought to be handled only
  // by a renderer process isolated from other sites. If --site-per-process is
  // used, like it is on desktop platforms, then this is true for all sites. In
  // other site isolation modes, only a subset of sites will require dedicated
  // processes.
  bool RequiresDedicatedProcess(
      const IsolationContext& isolation_context) const;

  // Returns true if a process for this SiteInfo should be locked to a
  // ProcessLock whose is_locked_to_site() method returns true. Returning true
  // here also implies that this SiteInfo requires a dedicated process. However,
  // the converse does not hold: this might still return false for certain
  // special cases where a site specific process lock can't be applied even when
  // this SiteInfo requires a dedicated process (e.g., with
  // --site-per-process). Examples of those cases include <webview> guests,
  // single-process mode, or extensions where a process is currently allowed to
  // be reused for different extensions.  Most of these special cases should
  // eventually be removed, and this function should become equivalent to
  // RequiresDedicatedProcess().
  bool ShouldLockProcessToSite(const IsolationContext& isolation_context) const;

  // Returns whether the process-per-site model is in use (globally or just for
  // the current site), in which case we should ensure there is only one
  // RenderProcessHost per site for the entire browser context.
  bool ShouldUseProcessPerSite(BrowserContext* browser_context) const;

  // Get the StoragePartitionConfig, which describes the StoragePartition this
  // SiteInfo is associated with.  For example, this will correspond to a
  // non-default StoragePartition for <webview> guests.
  const StoragePartitionConfig& storage_partition_config() const {
    return storage_partition_config_;
  }

  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue context) const;

 private:
  // Helper that returns a tuple of all the fields that are relevant for
  // comparing one SiteInfo to another, to tell whether they represent the same
  // underlying security principal.   This determines the SiteInfo's key for
  // containers; two SiteInfos that return the same value here will map to the
  // same entry in std::map, etc.
  static auto MakeSecurityPrincipalKey(const SiteInfo& site_info);

  // Helper method containing common logic used by the public
  // Create() and CreateOnIOThread() methods. Most of the parameters simply
  // match the values passed into the caller. `compute_site_url` controls
  // whether the site_url field is computed from an effective URL or simply
  // copied from the `process_lock_url_`. `compute_site_url` is set to false in
  // contexts where it may not be possible to get the effective URL (e.g. on the
  // IO thread).
  static SiteInfo CreateInternal(const IsolationContext& isolation_context,
                                 const UrlInfo& url_info,
                                 bool compute_site_url);

  // Returns the URL to which a process should be locked for the given UrlInfo.
  // This is computed similarly to the site URL but without resolving effective
  // URLs.
  static GURL DetermineProcessLockURL(const IsolationContext& isolation_context,
                                      const UrlInfo& url_info);

  // Returns the site for the given UrlInfo, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the UrlInfo has no host.
  // |should_use_effective_urls| specifies whether to resolve |url| to an
  // effective URL (via ContentBrowserClient::GetEffectiveURL()) before
  // determining the site.
  static GURL GetSiteForURLInternal(const IsolationContext& isolation_context,
                                    const UrlInfo& url,
                                    bool should_use_effective_urls);

  // Helper function for ProcessLockCompareTo(). Returns a std::tie of the
  // SiteInfo elements required for doing a ProcessLock comparison.
  auto MakeProcessLockComparisonKey() const;

  GURL site_url_;

  // The AgentClusterKey for the execution context. This represents the
  // isolation requested through the use of Document-Isolation-Policy. The
  // AgentClusterKey is currently optional and only computed when a navigation
  // has a Document-Isolation-policy header. It should eventually be made
  // non-optional once we compute it properly on each navigation. When this
  // happens, it will replace site_url_ and web_exposed_isolation_info_.
  // TODO(crbug.com/342365078): Origin-Agent-Cluster should also use the
  // AgentClusterKey to represent the isolation it requests.
  // TODO(crbug.com/342365083): Documents crossOriginIsolated through the use of
  // COOP and COEP should also use the AgentClusterKey instead of
  // WebExposedIsolationInfo.
  std::optional<AgentClusterKey> agent_cluster_key_;

  // The URL to use when locking a process to this SiteInstance's site via
  // SetProcessLock(). This is the same as |site_url_| except for cases
  // involving effective URLs, such as hosted apps.  In those cases, this URL is
  // a site URL that is computed without the use of effective URLs.
  GURL process_lock_url_;

  // Indicates whether this SiteInfo is specific to a single origin and requires
  // an origin-keyed process, rather than including all subdomains of that
  // origin. Only used for OriginAgentCluster header opt-ins. In contrast, the
  // site-level URLs that are typically used in SiteInfo include subdomains, as
  // do command-line isolated origins.
  bool requires_origin_keyed_process_ = false;

  // When true, indicates that `requires_origin_keyed_process_` is true because
  // this SiteInfo was created using origin-keyed processes by default, and not
  // due to an opt-in header.
  // Note: This is stored as a separate boolean instead of making
  // requires_origin_keyed_process_ an enum due to complexity from std::tie
  // comparisons, since we want two SiteInfos to be considered equivalent even
  // if they differ in this boolean.
  bool requires_origin_keyed_process_by_default_ = false;

  // When true, indicates this SiteInfo is for a origin-restricted-sandboxed
  // iframe.
  bool is_sandboxed_ = false;

  // When kIsolateSandboxedIframes is active using per-document grouping, each
  // isolated frame gets its own SiteInfo with a unique document identifier,
  // which in practice is the `navigation_id` for the NavigationRequest that led
  // to the creation of the SiteInstance. This value will be used in comparing
  // SiteInfos unless it is kInvalidUniqueSandboxId. It should be noted that the
  // value of `unique_sandbox_id_` will change for any cross-document
  // navigation, even if it's same-origin and/or stays in the same
  // RenderFrameHost.
  int unique_sandbox_id_;

  // The StoragePartitionConfig to use when loading content belonging to this
  // SiteInfo.
  StoragePartitionConfig storage_partition_config_;

  // Indicates the web-exposed isolation mode of the BrowsingInstance that
  // agents with this SiteInfo belongs to. The level of isolation which a page
  // opts-into has implications for the set of other pages which can live in
  // this SiteInstance, process allocation decisions, and API exposure in the
  // page's JavaScript context.
  WebExposedIsolationInfo web_exposed_isolation_info_ =
      WebExposedIsolationInfo::CreateNonIsolated();

  // Indicates the web-exposed isolation capability of agents with this
  // SiteInfo, ignoring the 'cross-origin-isolated' permissions policy. This is
  // a function of `web_exposed_isolation_info_` and the origins belonging to
  // this SiteInstance.
  //
  // This may be a lower isolation level than `web_exposed_isolation_info_`
  // because "Isolated Application" cannot be delegated cross-origin.
  WebExposedIsolationLevel web_exposed_isolation_level_ =
      WebExposedIsolationLevel::kNotIsolated;

  // Indicates this SiteInfo is for a <webview> guest.
  bool is_guest_ = false;

  // Indicates that there is a request to require a dedicated process for this
  // SiteInfo due to a hint from the Cross-Origin-Opener-Policy header.
  bool does_site_request_dedicated_process_for_coop_ = false;

  // Indicates that JIT is disabled for this SiteInfo.
  bool is_jit_disabled_ = false;

  // Indicates that v8 optimizations are disabled for this SiteInfo.
  bool are_v8_optimizations_disabled_ = false;

  // Indicates that this SiteInfo is for PDF content.
  bool is_pdf_ = false;

  // Indicates that this SiteInfo is for content inside a fenced frame. We use
  // just a bool as opposed to a GUID here in order to group same-origin fenced
  // frames together. See more details around fenced frame process isolation
  // here:
  // https://github.com/WICG/fenced-frame/blob/master/explainer/process_isolation.md.
  bool is_fenced_ = false;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const SiteInfo& site_info);

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INFO_H_
