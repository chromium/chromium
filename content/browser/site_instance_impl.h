// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/observer_list.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

namespace content {
class BrowsingInstance;
class RenderProcessHostFactory;

class CONTENT_EXPORT SiteInstanceImpl final : public SiteInstance,
                                              public RenderProcessHostObserver {
 public:
  class CONTENT_EXPORT Observer {
   public:
    // Called when this SiteInstance transitions to having no active frames,
    // as measured by active_frame_count().
    virtual void ActiveFrameCountIsZero(SiteInstanceImpl* site_instance) {}

    // Called when the renderer process of this SiteInstance has exited.
    virtual void RenderProcessGone(SiteInstanceImpl* site_instance) = 0;
  };

  static scoped_refptr<SiteInstanceImpl> Create(
      BrowserContext* browser_context);
  static scoped_refptr<SiteInstanceImpl> CreateForURL(
      BrowserContext* browser_context,
      const GURL& url);
  static bool ShouldAssignSiteForURL(const GURL& url);

  // Returns whether |lock_url| is at least at the granularity of a site (i.e.,
  // a scheme plus eTLD+1, like https://google.com).  Also returns true if the
  // lock is to a more specific origin (e.g., https://accounts.google.com), but
  // not if the lock is empty or applies to an entire scheme (e.g., file://).
  static bool IsOriginLockASite(const GURL& lock_url);

  // See SiteInstance::IsSameWebSite.
  // This version allows comparing URLs without converting them to effective
  // URLs first, which is useful for avoiding OOPIFs when otherwise same-site
  // URLs may look cross-site via their effective URLs.
  static bool IsSameWebSite(content::BrowserContext* browser_context,
                            const GURL& src_url,
                            const GURL& dest_url,
                            bool should_compare_effective_urls);

  // SiteInstance interface overrides.
  int32_t GetId() override;
  bool HasProcess() const override;
  RenderProcessHost* GetProcess() override;
  BrowserContext* GetBrowserContext() const override;
  const GURL& GetSiteURL() const override;
  scoped_refptr<SiteInstance> GetRelatedSiteInstance(const GURL& url) override;
  bool IsRelatedSiteInstance(const SiteInstance* instance) override;
  size_t GetRelatedActiveContentsCount() override;
  bool RequiresDedicatedProcess() override;

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
    process_reuse_policy_ = policy;
  }
  ProcessReusePolicy process_reuse_policy() const {
    return process_reuse_policy_;
  }

  // Whether the SiteInstance is created for a service worker. If this flag
  // is true, when a new process is created for this SiteInstance or a randomly
  // chosen existing process is reused because of the process limit, the process
  // will be tracked as having an unmatched service worker until reused by
  // another SiteInstance from the same site.
  void set_is_for_service_worker() { is_for_service_worker_ = true; }
  bool is_for_service_worker() const { return is_for_service_worker_; }

  // Returns the URL which was used to set the |site_| for this SiteInstance.
  // May be empty if this SiteInstance does not have a |site_|.
  const GURL& original_url() { return original_url_; }

  // Returns the URL which should be used in a LockToOrigin call for this
  // SiteInstance's process.
  const GURL& lock_url() { return lock_url_; }

  // True if |url| resolves to an effective URL that is different from |url|.
  // See GetEffectiveURL().  This will be true for hosted apps as well as NTP
  // URLs.
  static bool HasEffectiveURL(BrowserContext* browser_context, const GURL& url);

  // Returns the site for the given URL, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the URL has no host.
  // |should_use_effective_urls| specifies whether to resolve |url| to an
  // effective URL (via ContentBrowserClient::GetEffectiveURL()) before
  // determining the site.
  static GURL GetSiteForURL(BrowserContext* context,
                            const GURL& url,
                            bool should_use_effective_urls);

  // Returns the site of a given |origin|.  Unlike GetSiteForURL(), this does
  // not utilize effective URLs, isolated origins, or other special logic.  It
  // only translates an origin into a site (i.e., scheme and eTLD+1) and is
  // used internally by GetSiteForURL().  For making process model decisions,
  // GetSiteForURL() should be used instead.
  static GURL GetSiteForOrigin(const url::Origin& origin);

  // Returns the URL to which a process should be locked for the given URL.
  // This is computed similarly to the site URL (see GetSiteForURL), but
  // without resolving effective URLs.
  static GURL DetermineProcessLockURL(BrowserContext* context, const GURL& url);

  // Set the web site that this SiteInstance is rendering pages for.
  // This includes the scheme and registered domain, but not the port.  If the
  // URL does not have a valid registered domain, then the full hostname is
  // stored.
  void SetSite(const GURL& url);
  bool HasSite() const;

  // Returns whether there is currently a related SiteInstance (registered with
  // BrowsingInstance) for the site of the given url.  If so, we should try to
  // avoid dedicating an unused SiteInstance to it (e.g., in a new tab).
  bool HasRelatedSiteInstance(const GURL& url);

  // Returns whether this SiteInstance has a process that is the wrong type for
  // the given URL.  If so, the browser should force a process swap when
  // navigating to the URL.
  bool HasWrongProcessForURL(const GURL& url);

  // Increase the number of active frames in this SiteInstance. This is
  // increased when a frame is created, or a currently swapped out frame
  // is swapped in.
  void IncrementActiveFrameCount();

  // Decrease the number of active frames in this SiteInstance. This is
  // decreased when a frame is destroyed, or a currently active frame is
  // swapped out. Decrementing this to zero will notify observers, and may
  // trigger deletion of proxies.
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

  // Get the effective URL for the given actual URL.  This allows the
  // ContentBrowserClient to override the SiteInstance's site for certain URLs.
  // For example, Chrome uses this to replace hosted app URLs with extension
  // hosts.
  // Only public so that we can make a consistent process swap decision in
  // RenderFrameHostManager.
  static GURL GetEffectiveURL(BrowserContext* browser_context,
                              const GURL& url);

  // Returns true if pages loaded from |url| ought to be handled only by a
  // renderer process isolated from other sites. If --site-per-process is on the
  // command line, this is true for all sites. In other site isolation modes,
  // only a subset of sites will require dedicated processes.
  static bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                              const GURL& url);

  // Returns true if a process can be locked to a site |site_url|. Returning
  // true here also implies that |site_url| requires a dedicated process.
  // However, the converse does not hold: this might still return false for
  // certain special cases where an origin lock can't be applied even when
  // |site_url| requires a dedicated process (e.g., with --site-per-process).
  // Examples of those cases include <webview> guests, single-process mode, or
  // extensions where a process is currently allowed to be reused for different
  // extensions.  Most of these special cases should eventually be removed, and
  // this function should become equivalent to
  // DoesSiteRequireDedicatedProcess().
  static bool ShouldLockToOrigin(BrowserContext* browser_context,
                                 GURL site_url);

 private:
  friend class BrowsingInstance;
  friend class SiteInstanceTestBrowserClient;

  // Create a new SiteInstance.  Only BrowsingInstance should call this
  // directly; clients should use Create() or GetRelatedSiteInstance() instead.
  explicit SiteInstanceImpl(BrowsingInstance* browsing_instance);

  ~SiteInstanceImpl() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // Used to restrict a process' origin access rights.
  void LockToOriginIfNeeded();

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

  // Current RenderProcessHost that is rendering pages for this SiteInstance.
  // This pointer will only change once the RenderProcessHost is destructed.  It
  // will still remain the same even if the process crashes, since in that
  // scenario the RenderProcessHost remains the same.
  RenderProcessHost* process_;

  // Describes the desired behavior when GetProcess() method needs to find a new
  // process to associate with the current SiteInstanceImpl.  If |false|, then
  // prevents the spare RenderProcessHost from being taken and stored in
  // |process_|.
  bool can_associate_with_spare_process_;

  // The web site that this SiteInstance is rendering pages for.
  GURL site_;

  // Whether SetSite has been called.
  bool has_site_;

  // The URL which was used to set the |site_| for this SiteInstance.
  GURL original_url_;

  // The URL to use when locking a process to this SiteInstance's site via
  // LockToOrigin().  This is the same as |site_| except for cases involving
  // effective URLs, such as hosted apps.  In those cases, this URL is a site
  // URL that is computed without the use of effective URLs.
  GURL lock_url_;

  // The ProcessReusePolicy to use when creating a RenderProcessHost for this
  // SiteInstance.
  ProcessReusePolicy process_reuse_policy_;

  // Whether the SiteInstance was created for a service worker.
  bool is_for_service_worker_;

  base::ObserverList<Observer, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
