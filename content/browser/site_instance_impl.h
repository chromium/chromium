// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/observer_list.h"
#include "content/browser/isolation_context.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class AgentSchedulingGroupHost;
class BrowsingInstance;
class ProcessLock;
class RenderProcessHostFactory;
class StoragePartitionImpl;

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
  static SiteInfo CreateForErrorPage();
  static SiteInfo CreateForDefaultSiteInstance(
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>&
          coop_coep_cross_origin_isolated_origin);

  // The SiteInfo constructor should take in all values needed for comparing two
  // SiteInfos, to help ensure all creation sites are updated accordingly when
  // new values are added. The private function MakeTie() should be updated
  // accordingly.
  SiteInfo(const GURL& site_url,
           const GURL& process_lock_url,
           bool is_origin_keyed,
           bool is_coop_coep_cross_origin_isolated,
           const base::Optional<url::Origin>&
               coop_coep_cross_origin_isolated_origin);
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
  //       to consider this case when comparing lock URLs; ShouldLockProcess()
  //       may be used to determine whether the process lock will actually be
  //       used.
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

  // Returns true if this SiteInfo is part of a CoopCoepCrossOriginIsolated
  // BrowsingInstance.
  bool is_coop_coep_cross_origin_isolated() const {
    return is_coop_coep_cross_origin_isolated_;
  }

  // If is_coop_coep_cross_origin_isolated() returns true, this returns the
  // origin shared across all top level frames in the
  // CoopCoepCrossOriginIsolated BrowsingInstance.
  base::Optional<url::Origin> coop_coep_cross_origin_isolated_origin() const {
    return coop_coep_cross_origin_isolated_origin_;
  }

  // Returns false if the site_url() is empty.
  bool is_empty() const { return site_url().possibly_invalid_spec().empty(); }

  SiteInfo& operator=(const SiteInfo& rhs);

  bool operator==(const SiteInfo& other) const;
  bool operator!=(const SiteInfo& other) const;

  // Defined to allow this object to act as a key for std::map and std::set.
  bool operator<(const SiteInfo& other) const;

  // Returns a string representation of this SiteInfo principal.
  std::string GetDebugString() const;

 private:
  static auto MakeTie(const SiteInfo& site_info);

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

  // Indicates if this SiteInfo is part of a CoopCoepCrossOriginIsolated
  // BrowsingInstance. (i.e. A page that has a cross-origin-opener-policy of
  // same-origin and a cross-origin-embedder-policy of require-corp.)
  bool is_coop_coep_cross_origin_isolated_ = false;

  // If |is_coop_coep_cross_origin_isolated_| returns true, this returns the
  // origin shared across all top level frames in the
  // CoopCoepCrossOriginIsolated BrowsingInstance.
  base::Optional<url::Origin> coop_coep_cross_origin_isolated_origin_;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const SiteInfo& site_info);

// This struct is used to package a GURL together with extra state required to
// make SiteInstance/process allocation decisions, e.g. whether the url's origin
// is requesting isolation as determined by response headers in the
// corresponding navigation request. The extra state is generally most relevant
// when navigation to the URL is in progress, since once placed into a
// SiteInstance, the extra state will be available via SiteInfo. Otherwise, most
// callsites requiring a UrlInfo can create with a GURL, specifying false for
// |origin_requests_isolation|. Some examples of where passing false for
// |origin_requests_isolation| is safe are:
// * at DidCommitNavigation time, since at that point the SiteInstance has
//   already been picked and the navigation can be considered finished,
// * before a response is received (the only way to request isolation is via
//   response headers), and
// * outside of a navigation.
//
// If UrlInfo::origin_requests_isolation is false, that does *not* imply that
// the url will not be origin-isolated, and vice versa.  The origin isolation
// decision involves both response headers and consistency within a
// BrowsingInstance, and once we decide on the isolation outcome for an origin,
// it won't change for the lifetime of the BrowsingInstance.  To check whether
// or not a frame is origin-isolated, see SiteInfo::is_origin_keyed() on its
// SiteInstance.
//
// Note: it is not expected that this struct will be exposed in content/public.
struct CONTENT_EXPORT UrlInfo {
 public:
  UrlInfo() = default;  // Needed for inclusion in SiteInstanceDescriptor.
  UrlInfo(const GURL& url_in, bool origin_requests_isolation_in)
      : url(url_in), origin_requests_isolation(origin_requests_isolation_in) {}
  static inline UrlInfo CreateForTesting(const GURL& url_in) {
    // Used to convert GURL to UrlInfo in tests where opt-in isolation is not
    // being tested.
    return UrlInfo(url_in, false);
  }

  GURL url;
  // This flag is only relevant (1) during a navigation request, (2) up to the
  // point where the origin is placed into a SiteInstance, thus determining the
  // opt-in isolation status of the origin. Other than these cases, this should
  // be set to false.
  bool origin_requests_isolation;
};

class CONTENT_EXPORT SiteInstanceImpl final : public SiteInstance,
                                              public RenderProcessHostObserver {
 public:
  class CONTENT_EXPORT Observer {
   public:
    // Called when this SiteInstance transitions to having no active frames,
    // as measured by active_frame_count().
    virtual void ActiveFrameCountIsZero(SiteInstanceImpl* site_instance) {}

    // Called when the renderer process of this SiteInstance has exited.
    virtual void RenderProcessGone(SiteInstanceImpl* site_instance,
                                   const ChildProcessTerminationInfo& info) = 0;
  };

  // Methods for creating new SiteInstances. The documentation for these methods
  // are on the SiteInstance::Create* methods with the same name.
  static scoped_refptr<SiteInstanceImpl> Create(
      BrowserContext* browser_context);
  // |url_info| contains the GURL for which we want to create a SiteInstance,
  // along with other state relevant to making process allocation decisions.
  // |is_coop_coep_cross_origin_isolated| is not exposed in content/public. It
  // sets the BrowsingInstance is_coop_coep_cross_origin_isolated_ property.
  // Once this property is set it cannot be changed and is used in process
  // allocation decisions.
  // TODO(wjmaclean): absorb |is_coop_coep_cross_origin_isolated| and related
  // parameters into UrlInfo.
  static scoped_refptr<SiteInstanceImpl> CreateForUrlInfo(
      BrowserContext* browser_context,
      const UrlInfo& url_info,
      bool is_coop_coep_cross_origin_isolated);

  static scoped_refptr<SiteInstanceImpl> CreateForGuest(
      content::BrowserContext* browser_context,
      const GURL& guest_site_url);

  // Creates a SiteInstance that will be use for a service worker.
  // |url| - The script URL for the service worker if |is_guest| is false.
  //         The <webview> guest site URL if |is_guest| is true.
  // |can_reuse_process| - Set to true if the new SiteInstance can use the
  //                       same process as the renderer for |url|.
  // |is_guest| - Set to true if the new SiteInstance is for a <webview>
  // guest.
  static scoped_refptr<SiteInstanceImpl> CreateForServiceWorker(
      BrowserContext* browser_context,
      const GURL& url,
      bool can_reuse_process = false,
      bool is_guest = false);

  // Creates a SiteInstance for |url| like CreateForURL() would except the
  // instance that is returned has its process_reuse_policy set to
  // REUSE_PENDING_OR_COMMITTED_SITE and the default SiteInstance will never
  // be returned.
  static scoped_refptr<SiteInstanceImpl> CreateReusableInstanceForTesting(
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
    DCHECK(!IsDefaultSiteInstance());
    process_reuse_policy_ = policy;
  }
  ProcessReusePolicy process_reuse_policy() const {
    return process_reuse_policy_;
  }

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

  // This function returns a SiteInfo with the appropriate site_url and
  // process_lock_url computed. This function can only be called on the UI
  // thread since it expects an effective URL.
  // Note: eventually this function will replace GetSiteForURL().
  static SiteInfo ComputeSiteInfo(
      const IsolationContext& isolation_context,
      const UrlInfo& url_info,
      bool is_coop_coep_cross_origin_isolated,
      const base::Optional<url::Origin>& cross_origin_isolated_origin);

  // Helper method for tests that don't trigger special COOP/COEP
  // functionality, or test opt-in origin isolation.
  static SiteInfo ComputeSiteInfoForTesting(
      const IsolationContext& isolation_context,
      const GURL& url);

  // Returns the site for the given UrlInfo, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the URL has no host.
  // |url| will be resolved to an effective URL (via
  // ContentBrowserClient::GetEffectiveURL()) before determining the site.
  // NOTE: This function will soon be removed, and replaced by
  // ComputeSiteInfo(). New code should use that function instead.
  static GURL GetSiteForURL(const IsolationContext& isolation_context,
                            const UrlInfo& url_info);

  // Returns the site of a given |origin|.  Unlike GetSiteForURL(), this does
  // not utilize effective URLs, isolated origins, or other special logic.  It
  // only translates an origin into a site (i.e., scheme and eTLD+1) and is
  // used internally by GetSiteForURL().  For making process model decisions,
  // GetSiteForURL() should be used instead.
  static GURL GetSiteForOrigin(const url::Origin& origin);

  // Similar to above, but also computes a full SiteInfo (including a
  // process_lock_url) and returns a ProcessLock. If called from the IO thread,
  // this will return a ProcessLock that doesn't consider effective URLs.
  static ProcessLock DetermineProcessLock(
      const IsolationContext& isolation_context,
      const UrlInfo& url_info,
      bool is_coop_coep_cross_origin_isolated,
      base::Optional<url::Origin> coop_coep_cross_origin_isolated_origin);

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

  // Returns true if pages loaded from |site_info| ought to be handled only by a
  // renderer process isolated from other sites. If --site-per-process is used,
  // this is true for all sites. In other site isolation modes, only a subset
  // of sites will require dedicated processes.
  static bool DoesSiteInfoRequireDedicatedProcess(
      const IsolationContext& isolation_context,
      const SiteInfo& site_info);

  // Returns true if a process for a |site_info| should be locked. Returning
  // true here also implies that |site_info| requires a dedicated process.
  // However, the converse does not hold: this might still
  // return false for certain special cases where an origin lock can't be
  // applied even when |site_info| requires a dedicated process (e.g., with
  // --site-per-process). Examples of those cases include <webview> guests,
  // single-process mode, or extensions where a process is currently allowed to
  // be reused for different extensions.  Most of these special cases should
  // eventually be removed, and this function should become equivalent to
  // DoesSiteInfoRequireDedicatedProcess().
  //
  // |is_guest| should be set to true if the call is being made for a <webview>
  // guest SiteInstance(i.e. SiteInstance::IsGuest() returns true).
  static bool ShouldLockProcess(const IsolationContext& isolation_context,
                                const SiteInfo& site_info,
                                const bool is_guest);

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

  // If this SiteInstance doesn't require a dedicated process, this will return
  // the BrowsingInstance's default process.
  RenderProcessHost* GetDefaultProcessIfUsable();

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

  // Returns true if the SiteInstance is part of a CoopCoepCrossOriginIsolated
  // BrowsingInstance.
  bool IsCoopCoepCrossOriginIsolated() const;

  // If IsCoopCoepCrossOriginIsolated is true, returns the origin shared across
  // all top level frames in this BrowsingInstance.
  base::Optional<url::Origin> CoopCoepCrossOriginIsolatedOrigin() const;

 private:
  friend class BrowsingInstance;
  friend class SiteInstanceTestBrowserClient;
  FRIEND_TEST_ALL_PREFIXES(SiteInstanceTest, ProcessLockDoesNotUseEffectiveURL);
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

  // Returns the URL to which a process should be locked for the given UrlInfo.
  // This is computed similarly to the site URL (see GetSiteForURL), but
  // without resolving effective URLs.
  static GURL DetermineProcessLockURL(const IsolationContext& isolation_context,
                                      const UrlInfo& url_info);

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

  // Returns the site for the given UrlInfo, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the UrlInfo has no host.
  // |should_use_effective_urls| specifies whether to resolve |url| to an
  // effective URL (via ContentBrowserClient::GetEffectiveURL()) before
  // determining the site.
  static GURL GetSiteForURLInternal(const IsolationContext& isolation_context,
                                    const UrlInfo& url,
                                    bool should_use_effective_urls);

  // True if |url| resolves to an effective URL that is different from |url|.
  // See GetEffectiveURL().  This will be true for hosted apps as well as NTP
  // URLs.
  static bool HasEffectiveURL(BrowserContext* browser_context, const GURL& url);

  // Returns true if |url| and its |site_url| can be placed inside a default
  // SiteInstance.
  //
  // Note: |url| and |site_info| must be consistent with each other. In contexts
  // where the caller only has |url| it can use
  // SiteInstanceImpl::ComputeSiteInfo() to generate |site_info|. This call is
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

  // Whether the SiteInstance was created for a <webview> guest.
  // TODO(734722): Move this into the SecurityPrincipal once it is available.
  bool is_guest_;

  // How |this| was last assigned to a renderer process.
  SiteInstanceProcessAssignment process_assignment_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
