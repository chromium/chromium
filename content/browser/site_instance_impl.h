// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/observer_list.h"
#include "content/browser/isolation_context.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition_config.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class AgentSchedulingGroupHost;
class BrowsingInstance;
class ProcessLock;
class RenderProcessHostFactory;
class StoragePartitionImpl;

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
// If UrlInfo::origin_isolation_request is kNone, that does *not* imply that
// the URL's origin will not be isolated, and vice versa.  The isolation
// decision involves both response headers and consistency within a
// BrowsingInstance, and once we decide on the isolation outcome for an origin,
// it won't change for the lifetime of the BrowsingInstance.
//
// To check whether a frame ends up in a site-isolated process, use
// SiteInfo::RequiresDedicatedProcess() on its SiteInstance's SiteInfo.  To
// check whether a frame ends up being origin-isolated (e.g., due to the
// Origin-Agent-Cluster header), use SiteInfo::is_origin_keyed().
//
// Note: it is not expected that this struct will be exposed in content/public.
struct CONTENT_EXPORT UrlInfo {
 public:
  // Bitmask representing one or more isolation requests.
  enum OriginIsolationRequest {
    // No isolated has been requested.
    kNone = 0,
    // The Origin-Agent-Cluster header is requesting origin-keyed isolation for
    // `url`'s origin.
    kOriginAgentCluster = (1 << 0),
    // The Cross-Origin-Opener-Policy header has triggered a hint to turn on
    // site isolation for `url`'s site.
    kCOOP = (1 << 1)
  };

  UrlInfo() = default;  // Needed for inclusion in SiteInstanceDescriptor.
  UrlInfo(const GURL& url_in,
          OriginIsolationRequest origin_isolation_request_in)
      : url(url_in),
        origin_isolation_request(origin_isolation_request_in),
        origin(url::Origin::Create(url_in)) {}
  UrlInfo(const GURL& url_in,
          OriginIsolationRequest origin_isolation_request_in,
          const url::Origin& origin_in)
      : url(url_in),
        origin_isolation_request(origin_isolation_request_in),
        origin(origin_in) {}
  static inline UrlInfo CreateForTesting(const GURL& url_in) {
    // Used to convert GURL to UrlInfo in tests where opt-in isolation is not
    // being tested.
    return UrlInfo(url_in, OriginIsolationRequest::kNone);
  }

  // Returns whether this UrlInfo is requesting origin-keyed isolation for
  // `url`'s origin due to the OriginAgentCluster header.
  bool requests_origin_agent_cluster_isolation() const {
    return (origin_isolation_request &
            OriginIsolationRequest::kOriginAgentCluster);
  }

  // Returns whether this UrlInfo is requesting isolation in response to the
  // Cross-Origin-Opener-Policy header.
  bool requests_coop_isolation() const {
    return (origin_isolation_request & OriginIsolationRequest::kCOOP);
  }

  GURL url;

  // This field indicates whether the URL is requesting additional process
  // isolation during the current navigation (e.g., via OriginAgentCluster or
  // COOP response headers).  If URL did not request any isolation, this will
  // be set to kNone. This field is only relevant (1) during a navigation
  // request, (2) up to the point where the origin is placed into a
  // SiteInstance.  Other than these cases, this should be set to kNone.
  OriginIsolationRequest origin_isolation_request;

  // If |url| represents a resource inside another resource (e.g. a resource
  // with a urn: URL in WebBundle), origin of the original resource. Otherwise,
  // this is just the origin of |url|.
  url::Origin origin;
};

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
//
// TODO(wjmaclean): This should eventually move to
// content/public/browser/site_info.h.
class CONTENT_EXPORT SiteInfo {
 public:
  static SiteInfo CreateForErrorPage(
      const WebExposedIsolationInfo& web_exposed_isolation_info);
  static SiteInfo CreateForDefaultSiteInstance(
      BrowserContext* browser_context,
      const WebExposedIsolationInfo& web_exposed_isolation_info);
  static SiteInfo CreateForGuest(BrowserContext* browser_context,
                                 const GURL& guest_site_url);

  // This function returns a SiteInfo with the appropriate site_url and
  // process_lock_url computed. This function can only be called on the UI
  // thread because it must be able to compute an effective URL.
  static SiteInfo Create(
      const IsolationContext& isolation_context,
      const UrlInfo& url_info,
      const WebExposedIsolationInfo& web_exposed_isolation_info);

  // Similar to the function above, but this method can only be called on the
  // IO thread. All fields except for the site_url should be the same as
  // the other method. The site_url field will match the process_lock_url
  // in the object returned by this function. This is because we cannot compute
  // the effective URL from the IO thread.
  //
  // NOTE: Do not use this method unless there is a very clear and good reason
  // to do so. It primarily exists to facilitate the creation of ProcessLocks
  // from any thread. ProcessLocks do not rely on the site_url field so the
  // difference between this method and Create() does not cause problems for
  // that usecase.
  static SiteInfo CreateOnIOThread(
      const IsolationContext& isolation_context,
      const UrlInfo& url_info,
      const WebExposedIsolationInfo& web_exposed_isolation_info);

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

  // Returns a StoragePartitionConfig for the specified URL.
  // If |is_site_url| is set to true, then |url| MUST be a site URL that
  // was generated by a SiteInfo. Otherwise the URL is interpreted as a
  // user-provided URL or origin.
  //
  // Note: New callers of this method should be discouraged. New code should
  // have access to a SiteInfo object and call GetStoragePartitionConfig() on
  // that. For cases where code just needs the StoragePartition for a user
  // provided URL or origin, it should use
  // BrowserContext::GetStoragePartitionForUrl() instead of directly calling
  // this method.
  static StoragePartitionConfig GetStoragePartitionConfigForUrl(
      BrowserContext* browser_context,
      const GURL& url,
      bool is_site_url);

  // The SiteInfo constructor should take in all values needed for comparing two
  // SiteInfos, to help ensure all creation sites are updated accordingly when
  // new values are added. The private function MakeTie() should be updated
  // accordingly.
  SiteInfo(const GURL& site_url,
           const GURL& process_lock_url,
           bool is_origin_keyed,
           const WebExposedIsolationInfo& web_exposed_isolation_info,
           bool is_guest,
           bool does_site_request_dedicated_process_for_coop,
           bool is_jit_disabled);
  SiteInfo();
  SiteInfo(const SiteInfo& rhs);
  ~SiteInfo();

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

  // Returns whether this SiteInfo is specific to an origin rather than a site,
  // such as due to opt-in origin isolation. This resolves an ambiguity of
  // whether a process with a lock_url() like "https://foo.example" is allowed
  // to include "https://sub.foo.example" or not. In opt-in isolation, it is
  // possible for example.com to be isolated, and sub.example.com not be
  // isolated. In contrast, if command-line isolation is used to isolate
  // example.com, then sub.example.com is also (automatically) isolated.
  // Also note that opt-in isolated origins will include ports (if non-default)
  // in their site urls.
  bool is_origin_keyed() const { return is_origin_keyed_; }

  // Returns the web-exposed isolation status of pages hosted by the
  // SiteInstance. The level of isolation which a page opts-into has
  // implications for the set of other pages which can live in this
  // SiteInstance, process allocation decisions, and API exposure in the page's
  // JavaScript context.
  const WebExposedIsolationInfo& web_exposed_isolation_info() const {
    return web_exposed_isolation_info_;
  }

  bool is_guest() const { return is_guest_; }
  bool is_error_page() const;
  bool is_jit_disabled() const { return is_jit_disabled_; }

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

  // Get the partition ID or StoragePartitionConfig for this object given a
  // specific `browser_context`. The BrowserContext will affect whether the
  // partition is forced to be in memory based on whether it is off-the-record
  // or not.
  StoragePartitionId GetStoragePartitionId(
      BrowserContext* browser_context) const;
  StoragePartitionConfig GetStoragePartitionConfig(
      BrowserContext* browser_context) const;

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
  static SiteInfo CreateInternal(
      const IsolationContext& isolation_context,
      const UrlInfo& url_info,
      const WebExposedIsolationInfo& web_exposed_isolation_info,
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

  GURL site_url_;
  // The URL to use when locking a process to this SiteInstance's site via
  // SetProcessLock(). This is the same as |site_url_| except for cases
  // involving effective URLs, such as hosted apps.  In those cases, this URL is
  // a site URL that is computed without the use of effective URLs.
  GURL process_lock_url_;
  // Indicates whether this SiteInfo is specific to a single origin, rather than
  // including all subdomains of that origin. Only used for opt-in origin
  // isolation. In contrast, the site-level URLs that are typically used in
  // SiteInfo include subdomains, as do command-line isolated origins.
  bool is_origin_keyed_ = false;
  // Indicates the web-exposed isolation status of pages hosted by the
  // SiteInstance. The level of isolation which a page opts-into has
  // implications for the set of other pages which can live in this
  // SiteInstance, process allocation decisions, and API exposure in the page's
  // JavaScript context.
  WebExposedIsolationInfo web_exposed_isolation_info_ =
      WebExposedIsolationInfo::CreateNonIsolated();

  // Indicates this SiteInfo is for a <webview> guest.
  bool is_guest_ = false;

  // Indicates that there is a request to require a dedicated process for this
  // SiteInfo due to a hint from the Cross-Origin-Opener-Policy header.
  bool does_site_request_dedicated_process_for_coop_ = false;

  // Indicates that JIT is disabled for this SiteInfo.
  bool is_jit_disabled_ = false;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const SiteInfo& site_info);

class CONTENT_EXPORT SiteInstanceImpl final : public SiteInstance,
                                              public RenderProcessHostObserver {
 public:
  class CONTENT_EXPORT Observer {
   public:
    // Called when this SiteInstance transitions to having no active frames,
    // as measured by active_frame_count().
    virtual void ActiveFrameCountIsZero(SiteInstanceImpl* site_instance) {}

    // Called when the renderer process of this SiteInstance has exited. Note
    // that GetProcess() still returns the same RenderProcessHost instance. You
    // can reinitialize it by a call to GetProcess()->Init().
    virtual void RenderProcessGone(SiteInstanceImpl* site_instance,
                                   const ChildProcessTerminationInfo& info) {}

    // Called when the RenderProcessHost for this SiteInstance has been
    // destructed. After this, the underlying `process_` is cleared, and calling
    // GetProcess() would assign a different RenderProcessHost to this
    // SiteInstance.
    virtual void RenderProcessHostDestroyed() {}
  };

  // Methods for creating new SiteInstances. The documentation for these methods
  // are on the SiteInstance::Create* methods with the same name.
  static scoped_refptr<SiteInstanceImpl> Create(
      BrowserContext* browser_context);
  // |url_info| contains the GURL for which we want to create a SiteInstance,
  // along with other state relevant to making process allocation decisions.
  // |web_exposed_isolation_info| is not exposed in content/public. It
  // sets the BrowsingInstance web_exposed_isolation_info_ property.
  // Once this property is set it cannot be changed and is used in process
  // allocation decisions.
  // TODO(wjmaclean): absorb |web_exposed_isolation_info| into UrlInfo.
  static scoped_refptr<SiteInstanceImpl> CreateForUrlInfo(
      BrowserContext* browser_context,
      const UrlInfo& url_info,
      const WebExposedIsolationInfo& web_exposed_isolation_info);
  static scoped_refptr<SiteInstanceImpl> CreateForGuest(
      content::BrowserContext* browser_context,
      const GURL& guest_site_url);

  // Creates a SiteInstance that will be use for a service worker.
  // |url| - The script URL for the service worker if |is_guest| is false.
  //         The <webview> guest site URL if |is_guest| is true.
  // |can_reuse_process| - Set to true if the new SiteInstance can use the
  //                       same process as the renderer for |url|.
  // |web_exposed_isolation_info| - Indicates the web-exposed isolation state
  //                                of the main script (note that ServiceWorker
  //                                "cross-origin isolation" does not require
  //                                Cross-Origin-Opener-Policy to be set).
  // |is_guest| - Set to true if the new SiteInstance is for a <webview>
  // guest.
  static scoped_refptr<SiteInstanceImpl> CreateForServiceWorker(
      BrowserContext* browser_context,
      const GURL& url,
      const WebExposedIsolationInfo& web_exposed_isolation_info,
      bool can_reuse_process = false,
      bool is_guest = false);

  // Creates a SiteInstance for |url| like CreateForUrlInfo() would except the
  // instance that is returned has its process_reuse_policy set to
  // REUSE_PENDING_OR_COMMITTED_SITE and the default SiteInstance will never
  // be returned.
  static scoped_refptr<SiteInstanceImpl> CreateReusableInstanceForTesting(
      BrowserContext* browser_context,
      const GURL& url);

  // Creates a SiteInstance for |url| in a new BrowsingInstance for testing
  // purposes. This works similarly to CreateForUrlInfo() but with default
  // parameters that are suitable for most tests.
  static scoped_refptr<SiteInstanceImpl> CreateForTesting(
      BrowserContext* browser_context,
      const GURL& url);

  static bool ShouldAssignSiteForURL(const GURL& url);

  // Use this to get a related SiteInstance during navigations, where UrlInfo
  // may be requesting opt-in isolation. Outside of navigations, callers just
  // looking up an existing SiteInstance based on a GURL can use
  // GetRelatedSiteInstance (overridden from SiteInstance).
  scoped_refptr<SiteInstanceImpl> GetRelatedSiteInstanceImpl(
      const UrlInfo& url_info);
  bool IsSameSiteWithURLInfo(const UrlInfo& url_info);

  // SiteInstance interface overrides.
  int32_t GetId() override;
  int32_t GetBrowsingInstanceId() override;
  bool HasProcess() override;
  RenderProcessHost* GetProcess() override;
  BrowserContext* GetBrowserContext() override;
  const GURL& GetSiteURL() override;
  scoped_refptr<SiteInstance> GetRelatedSiteInstance(const GURL& url) override;
  bool IsRelatedSiteInstance(const SiteInstance* instance) override;
  size_t GetRelatedActiveContentsCount() override;
  bool RequiresDedicatedProcess() override;
  bool IsSameSiteWithURL(const GURL& url) override;
  bool IsGuest() override;
  SiteInstanceProcessAssignment GetLastProcessAssignmentOutcome() override;
  void WriteIntoTrace(perfetto::TracedValue context) override;

  // This is called every time a renderer process is assigned to a SiteInstance
  // and is used by the content embedder for collecting metrics.
  void set_process_assignment(SiteInstanceProcessAssignment assignment) {
    process_assignment_ = assignment;
  }

  // The policy to apply when selecting a RenderProcessHost for the
  // SiteInstance. If no suitable RenderProcessHost for the SiteInstance exists
  // according to the policy, and there are processes with unmatched service
  // workers for the site, the newest process with an unmatched service worker
  // is reused. If still no RenderProcessHost exists a new RenderProcessHost
  // will be created unless the process limit has been reached. When the limit
  // has been reached, the RenderProcessHost reused will be chosen randomly and
  // not based on the site.
  enum class ProcessReusePolicy {
    // In this mode, all instances of the site will be hosted in the same
    // RenderProcessHost.
    PROCESS_PER_SITE,

    // In this mode, the site will be rendered in a RenderProcessHost that is
    // already in use for the site, either for a pending navigation or a
    // committed navigation. If multiple such processes exist, ones that have
    // foreground frames are given priority, and otherwise one is selected
    // randomly.
    REUSE_PENDING_OR_COMMITTED_SITE,

    // In this mode, SiteInstances don't proactively reuse processes. An
    // existing process with an unmatched service worker for the site is reused
    // only for navigations, not for service workers. When the process limit has
    // been reached, a randomly chosen RenderProcessHost is reused as in the
    // other policies.
    DEFAULT,
  };

  void set_process_reuse_policy(ProcessReusePolicy policy) {
    CHECK(!IsDefaultSiteInstance());
    process_reuse_policy_ = policy;
  }
  ProcessReusePolicy process_reuse_policy() const {
    return process_reuse_policy_;
  }

  // Returns true if |has_site_| is true and |site_info_| indicates that the
  // process-per-site model should be used.
  bool ShouldUseProcessPerSite() const;

  // Checks if |current_process| can be reused for this SiteInstance, and
  // sets |process_| to |current_process| if so.
  void ReuseCurrentProcessIfPossible(RenderProcessHost* current_process);

  // Whether the SiteInstance is created for a service worker. If this flag
  // is true, when a new process is created for this SiteInstance or a randomly
  // chosen existing process is reused because of the process limit, the process
  // will be tracked as having an unmatched service worker until reused by
  // another SiteInstance from the same site.
  bool is_for_service_worker() const { return is_for_service_worker_; }

  // Returns the URL which was used to set the |site_info_| for this
  // SiteInstance. May be empty if this SiteInstance does not have a
  // |site_info_|.
  const GURL& original_url() {
    DCHECK(!IsDefaultSiteInstance());
    return original_url_;
  }

  // This is primarily a helper for RenderFrameHostImpl::IsNavigationSameSite();
  // most callers should use that API.
  //
  // Returns true if navigating a frame with (|last_successful_url| and
  // |last_committed_origin|) to |dest_url_info| should stay in the same
  // SiteInstance to preserve scripting relationships. |dest_url_info| carries
  // additional state, e.g. if the destination url requests origin isolation.
  //
  // |for_main_frame| is set to true if the caller is interested in an
  // answer for a main frame. This is set to false for subframe navigations.
  // Note: In some circumstances, like hosted apps, different answers can be
  // returned if we are navigating a main frame instead of a subframe.
  bool IsNavigationSameSite(const GURL& last_successful_url,
                            const url::Origin last_committed_origin,
                            bool for_main_frame,
                            const UrlInfo& dest_url_info);

  // SiteInfo related functions.

  // Returns the SiteInfo principal identifying all documents and workers within
  // this SiteInstance.
  // TODO(wjmaclean): eventually this function will replace const GURL&
  // GetSiteURL().
  const SiteInfo& GetSiteInfo();

  // Called when a RenderViewHost was created with this object. It returns the
  // same information as GetSiteInfo(), but also enables extra checks to ensure
  // that the StoragePartition info for this object does not change when
  // |site_info_| is set. This is important to verify if the SiteInfo has not
  // been explicitly set at the time of this call (e.g. first navigation in a
  // new tab).
  // TODO(acolwell) : Remove once RenderViewHost no longer needs to store a
  // SiteInfo and can store a StoragePartitionConfig instead. Extra verification
  // should be enabled when the config is fetched and |site_info_| has not been
  // set yet.
  const SiteInfo& GetSiteInfoForRenderViewHost();

  // Derives a new SiteInfo based on this SiteInstance's current state, and
  // the information provided in |url_info|. This function is slightly different
  // than SiteInfo::Create() because it takes into account information
  // specific to this SiteInstance, like whether it is a guest or not, and
  // changes its behavior accordingly. |is_related| - Controls the SiteInfo
  // returned for non-guest SiteInstances.
  //  Set to true if the caller wants the SiteInfo for an existing related
  //  SiteInstance associated with |url_info|. This is identical to what you
  //  would get from GetRelatedSiteInstanceImpl(url_info)->GetSiteInfo(). This
  //  may return the SiteInfo for the default SiteInstance so callers must be
  //  prepared to deal with that. If set to false, a SiteInfo created with
  //  SiteInfo::Create() is returned.
  //
  // For guest SiteInstances, |site_info_| is returned because guests are not
  // allowed to derive new guest SiteInfos. All guest navigations must stay in
  // the same SiteInstance with the same SiteInfo.
  SiteInfo DeriveSiteInfo(const UrlInfo& url_info, bool is_related = false);

  // Returns a ProcessLock that can be used with SetProcessLock to lock a
  // process to this SiteInstance's SiteInfo. The ProcessLock relies heavily on
  // the SiteInfo's process_lock_url() for security decisions.
  const ProcessLock GetProcessLock() const;

  // Helper function that returns the storage partition domain for this
  // object.
  // This is a temporary helper function used to verify that
  // the partition domain computed using this SiteInstance's site URL matches
  // the partition domain returned by storage_partition->GetPartitionDomain().
  // If there is a mismatch, we call DumpWithoutCrashing() and return the value
  // computed from the site URL since that is the legacy behavior.
  //
  // TODO(acolwell) : Remove this function and update callers to directly call
  // storage_partition->GetPartitionDomain() once we've verified that this is
  // safe.
  std::string GetPartitionDomain(StoragePartitionImpl* storage_partition);

  // Returns true if this SiteInstance is for a site that has JIT disabled.
  bool IsJitDisabled();

  // Set the web site that this SiteInstance is rendering pages for.
  // This includes the scheme and registered domain, but not the port.  If the
  // URL does not have a valid registered domain, then the full hostname is
  // stored. This method does not convert this instance into a default
  // SiteInstance, but the BrowsingInstance will call this method with
  // |url_info| set to GetDefaultSiteURL(), when it is creating its default
  // SiteInstance.
  void SetSite(const UrlInfo& url_info);

  // Similar to SetSite(), but first attempts to convert this object to a
  // default SiteInstance if |url_info| can be placed inside a default
  // SiteInstance. If conversion is not possible, then the normal SetSite()
  // logic is run.
  void ConvertToDefaultOrSetSite(const UrlInfo& url_info);

  // Returns whether SetSite() has been called.
  bool HasSite() const;

  // Returns whether there is currently a related SiteInstance (registered with
  // BrowsingInstance) for the given SiteInfo.  If so, we should try to avoid
  // dedicating an unused SiteInstance to it (e.g., in a new tab).
  bool HasRelatedSiteInstance(const SiteInfo& site_info);

  // Returns whether this SiteInstance is compatible with and can host the given
  // |url_info|. If not, the browser should force a SiteInstance swap when
  // navigating to the URL in |url_info|.
  bool IsSuitableForUrlInfo(const UrlInfo& url_info);

  // Increase the number of active frames in this SiteInstance. This is
  // increased when a frame is created.
  void IncrementActiveFrameCount();

  // Decrease the number of active frames in this SiteInstance. This is
  // decreased when a frame is destroyed. Decrementing this to zero will notify
  // observers, and may trigger deletion of proxies.
  void DecrementActiveFrameCount();

  // Get the number of active frames which belong to this SiteInstance.  If
  // there are no active frames left, all frames in this SiteInstance can be
  // safely discarded.
  size_t active_frame_count() { return active_frame_count_; }

  // Increase the number of active WebContentses using this SiteInstance. Note
  // that, unlike active_frame_count, this does not count pending RFHs.
  void IncrementRelatedActiveContentsCount();

  // Decrease the number of active WebContentses using this SiteInstance. Note
  // that, unlike active_frame_count, this does not count pending RFHs.
  void DecrementRelatedActiveContentsCount();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Whether GetProcess() method (when it needs to find a new process to
  // associate with the current SiteInstanceImpl) can return a spare process.
  bool CanAssociateWithSpareProcess();

  // Has no effect if the SiteInstanceImpl already has a |process_|.
  // Otherwise, prevents GetProcess() from associating this SiteInstanceImpl
  // with the spare RenderProcessHost - instead GetProcess will either need to
  // create a new, not-yet-initialized/spawned RenderProcessHost or will need to
  // reuse one of existing RenderProcessHosts.
  //
  // See also:
  // - https://crbug.com/840409.
  // - WebContents::CreateParams::desired_renderer_state
  // - SiteInstanceImpl::CanAssociateWithSpareProcess().
  void PreventAssociationWithSpareProcess();

  // Returns the special site URL used by the default SiteInstance.
  static const GURL& GetDefaultSiteURL();

  // Get the effective URL for the given actual URL.  This allows the
  // ContentBrowserClient to override the SiteInstance's site for certain URLs.
  // For example, Chrome uses this to replace hosted app URLs with extension
  // hosts.
  // Only public so that we can make a consistent process swap decision in
  // RenderFrameHostManager.
  static GURL GetEffectiveURL(BrowserContext* browser_context, const GURL& url);

  // Return an ID of the next BrowsingInstance to be created.  This ID is
  // guaranteed to be higher than any ID of an existing BrowsingInstance.
  // This is useful when process model decisions need to be scoped only to
  // future BrowsingInstances.  In particular, this can determine the cutoff in
  // BrowsingInstance IDs when adding a new isolated origin dynamically.
  static BrowsingInstanceId NextBrowsingInstanceId();

  // Return the IsolationContext associated with this SiteInstance.  This
  // specifies context for making process model decisions, such as information
  // about the current BrowsingInstance.
  const IsolationContext& GetIsolationContext();

  // Returns a process suitable for this SiteInstance if the
  // SiteInstanceGroupManager has one available. A null pointer will be returned
  // if this SiteInstance's group does not have a process yet or the
  // SiteInstanceGroupManager does not have a default process that can be reused
  // by this SiteInstance.
  RenderProcessHost* GetSiteInstanceGroupProcessIfAvailable();

  // Returns true if this object was constructed as a default site instance.
  bool IsDefaultSiteInstance() const;

  // Returns true if |site_url| is a site url that the BrowsingInstance has
  // associated with its default SiteInstance.
  bool IsSiteInDefaultSiteInstance(const GURL& site_url) const;

  // Returns true if the SiteInfo for |url_info| matches the SiteInfo for this
  // instance (i.e. GetSiteInfo()). Otherwise returns false.
  bool DoesSiteInfoForURLMatch(const UrlInfo& url_info);

  // Adds |origin| as a non-isolated origin within this BrowsingInstance due to
  // an existing instance at the time of opt-in, so that future instances of it
  // here won't be origin isolated.
  void PreventOptInOriginIsolation(
      const url::Origin& previously_visited_origin);

  // Returns the current AgentSchedulingGroupHost this SiteInstance is
  // associated with. Since the AgentSchedulingGroupHost *must* be assigned (and
  // cleared) together with the RenderProcessHost, calling this method when no
  // AgentSchedulingGroupHost is set will trigger the creation of a new
  // RenderProcessHost (with a new ID).
  AgentSchedulingGroupHost& GetAgentSchedulingGroup();

  // Returns the web-exposed isolation status of the BrowsingInstance this
  // SiteInstance is part of.
  const WebExposedIsolationInfo& GetWebExposedIsolationInfo() const;

  // Simple helper function that returns the is_isolated property of the
  // WebExposedIsolationInfo of this BrowsingInstance.
  bool IsCrossOriginIsolated() const;

 private:
  friend class BrowsingInstance;
  friend class SiteInstanceTestBrowserClient;

  // Friend tests that need direct access to IsSameSite().
  friend class SiteInstanceTest;

  // Create a new SiteInstance.  Only BrowsingInstance should call this
  // directly; clients should use Create() or GetRelatedSiteInstance() instead.
  explicit SiteInstanceImpl(BrowsingInstance* browsing_instance);

  ~SiteInstanceImpl() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // Used to restrict a process' origin access rights. This method gets called
  // when a process gets assigned to this SiteInstance and when the
  // SiteInfo is explicitly set. If the SiteInfo hasn't been set yet and
  // the current process lock is invalid, then this method sets the process
  // to an "allow_any_site" lock. If the SiteInfo gets set to something that
  // restricts access to a specific site, then the lock will be upgraded to a
  // "lock_to_site" lock.
  void LockProcessIfNeeded();

  // If kProcessSharingWithStrictSiteInstances is enabled, this will check
  // whether both a site and a process have been assigned to this SiteInstance,
  // and if this doesn't require a dedicated process, will offer process_ to
  // BrowsingInstance as the default process for SiteInstances that don't need
  // a dedicated process.
  void MaybeSetBrowsingInstanceDefaultProcess();

  // Sets the SiteInfo and other fields so that this instance becomes a
  // default SiteInstance.
  void SetSiteInfoToDefault();

  // Sets |site_info_| with |site_info| and registers this object with
  // |browsing_instance_|. SetSite() calls this method to set the site and lock
  // for a user provided URL. This method should only be called by code that
  // need to set the site and lock directly without any "url to site URL"
  // transformation.
  void SetSiteInfoInternal(const SiteInfo& site_info);

  // Helper method to set the process of this SiteInstance, only in cases
  // where it is safe. It is not generally safe to change the process of a
  // SiteInstance, unless the RenderProcessHost itself is entirely destroyed and
  // a new one later replaces it.
  void SetProcessInternal(RenderProcessHost* process);

  // Returns true if |original_url()| is the same site as
  // |dest_url_info| or this object is a default SiteInstance and can be
  // considered the same site as |dest_url_info|.
  bool IsOriginalUrlSameSite(const UrlInfo& dest_url_info,
                             bool should_compare_effective_urls);

  // Add |site_info| to the set that tracks what sites have been allowed
  // to be handled by this default SiteInstance.
  void AddSiteInfoToDefault(const SiteInfo& site_info);

  // Return whether both UrlInfos must share a process to preserve script
  // relationships.  The decision is based on a variety of factors such as
  // the registered domain of the URLs (google.com, bbc.co.uk), the scheme
  // (https, http), and isolated origins.  Note that if the destination is a
  // blank page, we consider that to be part of the same web site for the
  // purposes for process assignment.  |should_compare_effective_urls| allows
  // comparing URLs without converting them to effective URLs first.  This is
  // useful for avoiding OOPIFs when otherwise same-site URLs may look
  // cross-site via their effective URLs.
  // Note: This method is private because it is an internal detail of this class
  // and there is subtlety around how it can be called because of hosted
  // apps. Most code outside this class should call
  // RenderFrameHostImpl::IsNavigationSameSite() instead.
  static bool IsSameSite(const IsolationContext& isolation_context,
                         const UrlInfo& src_url_info,
                         const UrlInfo& dest_url_info,
                         bool should_compare_effective_urls);

  // True if |url| resolves to an effective URL that is different from |url|.
  // See GetEffectiveURL().  This will be true for hosted apps as well as NTP
  // URLs.
  static bool HasEffectiveURL(BrowserContext* browser_context, const GURL& url);

  // Returns true if |url| and its |site_url| can be placed inside a default
  // SiteInstance.
  //
  // Note: |url| and |site_info| must be consistent with each other. In contexts
  // where the caller only has |url| it can use
  // SiteInfo::Create() to generate |site_info|. This call is
  // intentionally not set as a default value to encourage the caller to reuse
  // a SiteInfo computation if they already have one.
  static bool CanBePlacedInDefaultSiteInstance(
      const IsolationContext& isolation_context,
      const GURL& url,
      const SiteInfo& site_info);

  // An object used to construct RenderProcessHosts.
  static const RenderProcessHostFactory* g_render_process_host_factory_;

  // The next available SiteInstance ID.
  static int32_t next_site_instance_id_;

  // A unique ID for this SiteInstance.
  int32_t id_;

  // The number of active frames in this SiteInstance.
  size_t active_frame_count_;

  // BrowsingInstance to which this SiteInstance belongs.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // Current RenderProcessHost that is rendering pages for this SiteInstance,
  // and AgentSchedulingGroupHost (within the process) this SiteInstance belongs
  // to. Since AgentSchedulingGroupHost is associated with a specific
  // RenderProcessHost, these *must be* changed together to avoid UAF!
  // The |process_| pointer (and hence the |agent_scheduling_group_| pointer as
  // well) will only change once the RenderProcessHost is destructed. They will
  // still remain the same even if the process crashes, since in that scenario
  // the RenderProcessHost remains the same.
  RenderProcessHost* process_;
  AgentSchedulingGroupHost* agent_scheduling_group_;

  // Describes the desired behavior when GetProcess() method needs to find a new
  // process to associate with the current SiteInstanceImpl.  If |false|, then
  // prevents the spare RenderProcessHost from being taken and stored in
  // |process_|.
  bool can_associate_with_spare_process_;

  // The SiteInfo that this SiteInstance is rendering pages for.
  SiteInfo site_info_;

  // Whether SetSite has been called.
  bool has_site_;

  // The URL which was used to set the |site_info_| for this SiteInstance.
  GURL original_url_;

  // The ProcessReusePolicy to use when creating a RenderProcessHost for this
  // SiteInstance.
  ProcessReusePolicy process_reuse_policy_;

  // Whether the SiteInstance was created for a service worker.
  bool is_for_service_worker_;

  // How |this| was last assigned to a renderer process.
  SiteInstanceProcessAssignment process_assignment_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  // Contains the state that is only required for default SiteInstances.
  class DefaultSiteInstanceState;
  std::unique_ptr<DefaultSiteInstanceState> default_site_instance_state_;

  // Keeps track of whether we need to verify that the StoragePartition
  // information does not change when `site_info_` is set.
  bool verify_storage_partition_info_ = false;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
