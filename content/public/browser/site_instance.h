// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "base/types/id_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/browsing_instance_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/site_instance_process_assignment.h"
#include "content/public/browser/storage_partition_config.h"
#include "url/gurl.h"

namespace perfetto::protos::pbzero {
class SiteInstance;
}  // namespace perfetto::protos::pbzero

namespace content {
class BrowserContext;
class RenderProcessHost;
class StoragePartitionConfig;

using SiteInstanceId = base::IdType32<class SiteInstanceIdTag>;
using SiteInstanceGroupId = base::IdType32<class SiteInstanceGroupIdTag>;

///////////////////////////////////////////////////////////////////////////////
// SiteInstance interface.
//
// In an ideal sense, a SiteInstance represents a group of documents and workers
// that can share memory with each other, and thus must live in the same
// renderer process. In the spec, this roughly corresponds to an agent cluster.
// Documents that are able to synchronously script each other will always be
// placed in the same SiteInstance.
//
// A document's SiteInstance is determined by a combination of where the
// document comes from (i.e., a principal based on its "site") and which frames
// have references to it (i.e., the browsing context group, or "instance"). The
// site part groups together documents that can script each other, while the
// instance part allows independent copies of such documents to safely live in
// different processes.
//
// The principal is usually based on the site of the document's URL: the scheme
// and "registrable domain" (i.e., eTLD+1), not the full origin. For example,
// https://dev.chromium.org would have a site of https://chromium.org. This
// preserves compatibility with document.domain modifications, which allow
// similar origin pages to script each other. (Note that there are many
// exceptions, and the policy for determining site URLs is complex.) Meanwhile,
// an "instance" is represented by the BrowsingInstance class, which includes
// all frames that can find each other based on how they were created (e.g.,
// window.open or targeted links).
//
// In practice, a SiteInstance may contain documents from more than a single
// site, usually for compatibility or performance reasons. For example, on
// platforms that do not support out-of-process iframes, cross-site iframes must
// necessarily be loaded in the same process as their parent document. Chrome's
// process model uses SiteInstance as the basic primitive for assigning
// documents to processes, and the process model's behavior is tuned primarily
// by changing how SiteInstance principals (e.g., site URLs) are defined.
//
// Various process models are currently supported:
//
// FULL SITE ISOLATION (the current default for desktop platforms): Every
// document from the web uses a SiteInstance whose process is strictly locked to
// a single site (scheme + eTLD+1), such that the renderer process can be
// prevented from loading documents outside that site. Cross-site navigations
// always change SiteInstances (usually within the same BrowsingInstance,
// although sometimes the BrowsingInstance changes as well). Subframes from
// other sites will use different SiteInstances (always within the same
// BrowsingInstance), and thus out-of-process iframes.
//
// PARTIAL SITE ISOLATION (the current Google Chrome default on most Android
// devices): Documents from sites that are most likely to involve login use
// SiteInstances that are strictly locked to such sites, while one shared
// SiteInstance within each BrowsingInstance is used for the remaining documents
// from other less sensitive sites. This avoids out-of-process iframes in the
// common case for performance reasons, while protecting the sites that are most
// likely to be targeted in attacks.
//
// NO SITE ISOLATION (the current Google Chrome default on low-end Android
// devices and Android WebView): No documents from the web use locked processes,
// and no out-of-process iframes are created. The shared SiteInstance for each
// BrowsingInstance is always used for documents from the web.
//
// In each model, there are many exceptions, such as always requiring locked
// processes for chrome:// URLs, or allowing some special cases to share
// processes with each other (e.g., file:// URLs).
//
// In terms of lifetime, each RenderFrameHost tracks the SiteInstance it is
// associated with, to identify its principal and determine its process. Each
// FrameNavigationEntry also tracks the SiteInstance that rendered it, to
// prevent loading attacker-controlled data into the wrong process on a session
// history navigation. A SiteInstance is jointly owned by these references and
// is only alive as long as it is accessible, either from current documents or
// from session history.
//
///////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT SiteInstance : public base::RefCounted<SiteInstance> {
 public:
  // Returns a unique ID for this SiteInstance.
  virtual SiteInstanceId GetId() = 0;

  // Returns a unique ID for the BrowsingInstance (i.e., group of related
  // browsing contexts) to which this SiteInstance belongs. This allows callers
  // to identify which SiteInstances can asynchronously script each other.
  virtual BrowsingInstanceId GetBrowsingInstanceId() = 0;

  // Whether this SiteInstance has a running process associated with it.
  // This may return true before the first call to GetProcess(), in cases where
  // we use process-per-site and there is an existing process available.
  virtual bool HasProcess() = 0;

  // Returns the current RenderProcessHost being used to render pages for this
  // SiteInstance.  If there is no RenderProcessHost (because either none has
  // yet been created or there was one but it was cleanly destroyed (e.g. when
  // it is not actively being used), then this method will create a new
  // RenderProcessHost (and a new ID).  Note that renderer process crashes leave
  // the current RenderProcessHost (and ID) in place.
  //
  // For sites that require process-per-site mode (e.g., NTP), this will
  // ensure only one RenderProcessHost for the site exists within the
  // BrowserContext.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the ID of the SiteInstanceGroup this SiteInstance belongs to. If
  // the SiteInstance has no group, return 0, which is an invalid
  // SiteInstanceGroup ID.
  virtual SiteInstanceGroupId GetSiteInstanceGroupId() = 0;

  // Browser context to which this SiteInstance (and all related
  // SiteInstances) belongs.
  virtual BrowserContext* GetBrowserContext() = 0;

  // Get the web site that this SiteInstance is rendering pages for. This
  // includes the scheme and registered domain, but not the port.
  //
  // NOTE: In most cases, code should be performing checks against the origin
  // returned by |RenderFrameHost::GetLastCommittedOrigin()|. In contrast, the
  // GURL returned by |GetSiteURL()| should not be considered authoritative
  // because:
  // - a SiteInstance can host pages from multiple sites if "site per process"
  //   is not enabled and the SiteInstance isn't hosting pages that require
  //   process isolation (e.g. WebUI or extensions)
  // - even with site per process, the site URL is not an origin: while often
  //   derived from the origin, it only contains the scheme and the eTLD + 1,
  //   i.e. an origin with the host "deeply.nested.subdomain.example.com"
  //   corresponds to a site URL with the host "example.com".
  virtual const GURL& GetSiteURL() = 0;

  // Get the StoragePartitionConfig used by this SiteInstance.
  virtual const StoragePartitionConfig& GetStoragePartitionConfig() = 0;

  // Gets a SiteInstance for the given URL that shares the current
  // BrowsingInstance, creating a new SiteInstance if necessary.  This ensures
  // that a BrowsingInstance only has one SiteInstance per site, so that pages
  // in a BrowsingInstance have the ability to script each other.
  virtual scoped_refptr<SiteInstance> GetRelatedSiteInstance(
      const GURL& url) = 0;

  // Returns whether the given SiteInstance is in the same BrowsingInstance as
  // this one.  If so, JavaScript interactions that are permitted across
  // origins (e.g., postMessage) should be supported.
  virtual bool IsRelatedSiteInstance(const SiteInstance* instance) = 0;

  // Returns the total active WebContents count for this SiteInstance and all
  // related SiteInstances that have a form of communication with each other.
  // This include all the WebContents for documents in the same BrowsingInstance
  // as well as all the BrowsingInstances in the same CoopRelatedGroup. The
  // latter is useful to include because some interactions (e.g., messaging) are
  // allowed across such BrowsingInstances.
  virtual size_t GetRelatedActiveContentsCount() = 0;

  // Returns true if this SiteInstance is for a site that requires a dedicated
  // process. This only returns true under the "site per process" process model.
  virtual bool RequiresDedicatedProcess() = 0;

  // Returns true if this SiteInstance is for a process-isolated origin with its
  // own OriginAgentCluster.
  virtual bool RequiresOriginKeyedProcess() = 0;

  // Returns true if the SiteInstance is for a process-isolated sandboxed
  // documents only.
  virtual bool IsSandboxed() = 0;

  // Return whether this SiteInstance and the provided |url| are part of the
  // same web site, for the purpose of assigning them to processes accordingly.
  // The decision is currently based on the registered domain of the URLs
  // (google.com, bbc.co.uk), as well as the scheme (https, http). This ensures
  // that two pages will be in the same process if they can communicate with
  // other via JavaScript. (e.g., docs.google.com and mail.google.com have DOM
  // access to each other if they both set their document.domain properties to
  // google.com.) Note that if the destination is a blank page, we consider
  // that to be part of the same web site for the purposes for process
  // assignment.
  virtual bool IsSameSiteWithURL(const GURL& url) = 0;

  // Returns true if this object is used for a <webview> guest.
  virtual bool IsGuest() = 0;

  // Returns how this SiteInstance was assigned to a renderer process the most
  // recent time that such an assignment was done. This allows the content
  // embedder to collect metrics on how renderer process starting or reuse
  // affects performance.
  virtual SiteInstanceProcessAssignment GetLastProcessAssignmentOutcome() = 0;

  using TraceProto = perfetto::protos::pbzero::SiteInstance;
  // Write a representation of this object into a trace.
  virtual void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) = 0;

  // Estimates the overhead in terms of process count due to OriginAgentCluster
  // (OAC) SiteInstances in the BrowsingInstance related to this SiteInstance.
  // The estimate is based on counting SiteInstances where OAC is on, and
  // subtracting from it the count of SiteInstances that would exist without
  // OAC. If we assume that we don't coalesce SiteInstances from different
  // BrowsingInstances into a single RenderProcess, this roughly corresponds to
  // the number of renderer processes engendered by OAC.
  virtual int EstimateOriginAgentClusterOverheadForMetrics() = 0;

  // Factory method to create a new SiteInstance.  This will create a new
  // BrowsingInstance, so it should only be used when creating a new tab from
  // scratch (or similar circumstances).
  //
  // The render process host factory may be nullptr.  See SiteInstance
  // constructor.
  static scoped_refptr<SiteInstance> Create(BrowserContext* browser_context);

  // Factory method to get the appropriate SiteInstance for the given URL, in
  // a new BrowsingInstance.  Use this instead of Create when you know the URL,
  // since it allows special site grouping rules to be applied (for example, to
  // obey process-per-site for sites that require it, such as NTP, or to use a
  // default SiteInstance for sites that don't require a dedicated process on
  // Android).
  static scoped_refptr<SiteInstance> CreateForURL(
      BrowserContext* browser_context,
      const GURL& url);

  // Factory method to create a SiteInstance for a <webview> guest in a new
  // BrowsingInstance. A guest requires a non-default StoragePartitionConfig
  // which should be passed in via `partition_config`.
  static scoped_refptr<SiteInstance> CreateForGuest(
      BrowserContext* browser_context,
      const StoragePartitionConfig& partition_config);

  // Factory method to create a SiteInstance in a new BrowsingInstance with a
  // custom StoragePartition that is preserved across navigations.
  // `partition_config` needs to be for a non-default StoragePartition.
  static scoped_refptr<SiteInstance> CreateForFixedStoragePartition(
      BrowserContext* browser_context,
      const GURL& url,
      const StoragePartitionConfig& partition_config);

  // Determine if a URL should "use up" a site.  URLs such as about:blank or
  // chrome-native:// leave the site unassigned.
  //
  // Note that this API shouldn't be used for cases where about:blank has an
  // inherited origin, because that origin may influence the outcome of this
  // call.  See the content-internal ShouldAssignSiteForUrlInfo() for more
  // information.
  static bool ShouldAssignSiteForURL(const GURL& url);

  // Starts requiring a dedicated process for |url|'s site.  On platforms where
  // strict site isolation is disabled, this may be used as a runtime signal
  // that a certain site should become process-isolated, because its security
  // is important to the user (e.g., if the user has typed a password or logged
  // in via OAuth on that site).  The site will be determined from |url|'s
  // scheme and eTLD+1. If |context| is non-null, the site will be isolated
  // only within that BrowserContext; if |context| is null, the site will be
  // isolated globally for all BrowserContexts. |source| specifies why the new
  // site is being isolated.
  //
  // Note that this has no effect if site isolation is turned off, such as via
  // the kDisableSiteIsolation cmdline flag or enterprise policy -- see also
  // SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled().
  //
  // The |should_persist| parameter controls whether the site is added
  // *persistently*.  When true (this is the default), this function will ask
  // the embedder to save the site as part of profile data for |context|, so
  // that it survives restarts. The site will be cleared from profile data if
  // the user clears browsing data.  When false, the isolation will last only
  // until the end of the current browsing session.  This is appropriate if the
  // site's persistence is not desired or is managed separately (e.g., sites
  // isolated due to OAuth logins are saved and in another component).
  static void StartIsolatingSite(
      BrowserContext* context,
      const GURL& url,
      ChildProcessSecurityPolicy::IsolatedOriginSource source,
      bool should_persist = true);

 protected:
  friend class base::RefCounted<SiteInstance>;

  SiteInstance() {}
  virtual ~SiteInstance() {}
};

}  // namespace content.

#endif  // CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_H_
