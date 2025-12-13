// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGENT_CLUSTER_KEY_H_
#define CONTENT_BROWSER_AGENT_CLUSTER_KEY_H_

#include <optional>
#include <variant>

#include "content/browser/security/coop/cross_origin_isolation_mode.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// AgentClusterKey represents the implementation in the browser process of the
// AgentClusterKey concept of the HTML spec:
// https://html.spec.whatwg.org/multipage/webappapis.html#agent-cluster-key
//
// SiteInstances have an AgentClusterKey in their SiteInfo, which represents the
// AgentClusterKey of the execution contexts hosted by the SiteInstance. In a
// BrowsingInstance, all regular web execution contexts needing the same
// AgentClusterKey are hosted in the same SiteInstance. There can be exceptions
// for non-regular web contexts, such as Guest Views, as they may require to be
// hosted in a separate SiteInstance for security reasons.
//
// The AgentClusterKey is computed upon navigation, or when launching a worker.
// It is then passed to RenderFrameHostManager to determine which SiteInstance
// is appropriate to host the execution context.
// TODO(crbug.com/342365078): Currently, AgentClusterKey is only computed when a
// document has a Document-Isolation-Policy. Compute it on all navigations. Once
// this is properly done, use the AgentClusterKey to replace the site URL in
// SiteInfo, as it will only duplicate the information in AgentClusterKey.
class CONTENT_EXPORT AgentClusterKey {
 public:
  // Cross-origin isolated agent clusters have an additional isolation key.
  struct CONTENT_EXPORT CrossOriginIsolationKey {
    CrossOriginIsolationKey(
        const url::Origin& common_coi_origin,
        CrossOriginIsolationMode cross_origin_isolation_mode);
    CrossOriginIsolationKey(const CrossOriginIsolationKey& other);
    ~CrossOriginIsolationKey();
    bool operator==(const CrossOriginIsolationKey& b) const;
    // The origin of the document which triggered cross-origin isolation. This
    // might be different from the origin returned by AgentClusterKey::GetOrigin
    // when cross-origin isolation was enabled by COOP + COEP. It should always
    // match when cross-origin isolation was enabled by
    // Document-Isolation-Policy.
    url::Origin common_coi_origin;

    // Whether cross-origin isolation is effective or logical. Effective
    // cross-origin isolation grants access to extra web APIs. Some platforms
    // might not have the process model needed to support cross-origin
    // isolation. In this case, the web-visible isolation restrictions apply,
    // but do not lead to access to extra APIs. This is logical cross-origin
    // isolation.
    CrossOriginIsolationMode cross_origin_isolation_mode;
  };

  // Tracks the state of an Origin-Agent-Cluster request for a document.
  // The Origin-Agent-Cluster header can be used to request either an
  // origin-keyed agent cluster (1?) or a site-keyed one (0?). In the absence of
  // an OAC header, agent clusters will be either site-keyed or origin-keyed by
  // default, depending on whether features::kOriginKeyedProcessesByDefault is
  // enabled.
  enum class OACStatus {
    kOriginKeyedByHeader,
    kSiteKeyedByHeader,
    kOriginKeyedByDefault,
    kSiteKeyedByDefault
  };

  // The following functions are used to create appropriate AgentClusterKeys for
  // a navigation. The |oac_status| parameter passed to the AgentClusterKey is
  // the OAC status computed for the navigation. Note that it corresponds to OAC
  // backed by actual process isolation, and not logical OAC (ie OAC enabled
  // only in the renderer process).
  //
  // Following the deprecation of document.domain by default (a.k.a.
  // Origin-Agent-Cluster by default), AgentClusterKeys should be origin keyed
  // unless the document sends a "Origin-Agent-Cluster: ?0" header. However,
  // without SiteInstanceGroup, this would lead to extra process creation. So
  // when computing AgentClusterKeys for all navigations, we make
  // them site-keyed by default. Eventually, we should make them origin-keyed,
  // but have SiteInfos with a same-site AgentClusterKey share a
  // SiteInstanceGroup when kOriginKeyedProcessesByDefault is not enabled.
  // See crbug.com/40176090.
  //
  // When using CreateSiteKeyed, the |oac_status| must be kSiteKeyed*.
  //
  // When using CreateOriginKeyed, the |oac_status| does not have to be
  // kOriginKeyed*. The browser might want to
  // assign origin-keyed agent clusters in some cases, even when the document
  // did not request OAC and kOriginKeyedProcessesByDefault is not enabled. This
  // does not happen in practice currently, but should happen when we convert
  // the following cases to always create origin-keyed AgentClusterKeys:
  //   - origin-isolated sandboxed data iframes
  //   - legacy kStrictOriginIsolation mode.
  //
  // When using CreateWithCrossOriginIsolationKey, the |oac_status| does not
  // have to be kOriginKeyed*. Cross-origin isolated contexts are always
  // origin-keyed per spec, regardless of the OAC header.
  static AgentClusterKey CreateSiteKeyed(const GURL& site_url,
                                         const OACStatus& oac_status);
  static AgentClusterKey CreateOriginKeyed(const url::Origin& origin,
                                           const OACStatus& oac_status);

  static AgentClusterKey CreateWithCrossOriginIsolationKey(
      const url::Origin& origin,
      const AgentClusterKey::CrossOriginIsolationKey& isolation_key,
      const OACStatus& oac_status);

  // The default constructor will create an AgentClusterKey site-keyed to the
  // empty URL.
  // TODO(crbug.com/342366372): Once SiteInstanceGroup has launched for all
  // SiteInstances, the default constructor should return an origin-keyed
  // AgentClusterKey with an empty origin.
  AgentClusterKey();
  AgentClusterKey(const AgentClusterKey& other);
  ~AgentClusterKey();

  // Whether the Agent Cluster is keyed using Site URL or Origin.
  bool IsSiteKeyed() const;
  bool IsOriginKeyed() const;

  // The status of the Origin-Agent-Cluster header request for the navigation
  // that created the AgentClusterKey.
  // This is mainly used in SiteInfo to distinguish between SiteInfos that
  // received process isolation for their origin due to an explicit OAC opt-in
  // via header (kOriginKeyedByHeader) from the SiteInfos that received process
  // isolation due to features::kOriginKeyedProcessesByDefault
  // (kOriginKeyedByDefault). The former must be tracked per BrowsingInstance to
  // maintain a consistent OAC state, while the latter do not need to do so.
  // Note that this only applies to OAC that is backed by process isolation. OAC
  // can also be logical, in which case it will only apply in the renderer
  // process and is not tracked in the AgentClusterKey (and by extension
  // SiteInfo). Also note that having an |oac_status_| of kOriginKeyedByHeader
  // or kOriginKeyedByDefault will make the AgentClusterKey origin-keyed, the
  // reverse is not true. It is possible for the AgentClusterKey to be
  // origin-keyed and |oac_status_| to be kSiteKeyedByDefault, for example in
  // the case of a cross-origin isolated document with DocumentIsolationPolicy.
  const OACStatus& oac_status() const { return oac_status_; }

  // The site URL or the origin of the AgentClusterKey. Each function should
  // only be called when the Agent Cluster is site-keyed or origin-keyed
  // respectively. The functions will CHECK fail if called in the wrong cases.
  const GURL& GetSite() const;
  const url::Origin& GetOrigin() const;

  // This will return a URL based on the site URL or the origin of the
  // AgentClusterKey depending on whether the AgentClusterKey is site-keyed or
  // origin-keyed. Prefer comparing the Site URL or the Origin directly when
  // possible.
  GURL GetURL() const;

  // Returns nullopt if the AgentClusterKey is not cross-origin isolated.
  // Otherwise, returns the CrossOriginIsolationKey associated to the
  // AgentClusterKey.
  const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
  GetCrossOriginIsolationKey() const;

  // Returns true if the AgentClusterKey is cross-origin isolated.
  bool IsCrossOriginIsolated() const;

  bool operator==(const AgentClusterKey& b) const;

  // Needed for tie comparisons in SiteInfo.
  bool operator<(const AgentClusterKey& b) const;

 private:
  AgentClusterKey(const std::variant<GURL, url::Origin>& key,
                  const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
                      isolation_key,
                  const OACStatus& oac_status);

  // The origin or site URL that all execution contexts in the agent cluster
  // must share. By default, this is a site URL and the agent cluster is
  // site-keyed. The agent cluster can also be origin-keyed, in which case
  // execution contexts in the agent cluster must share the same origin, as
  // opposed to the site URL.
  //
  // For example, execution contexts with origin "https://example.com" and
  // "https://subdomain.example.com" can be placed in the same site-keyed agent
  // cluster with site URL key "https://example.com". But an execution context
  // with origin "https://subdomain.example.com" cannot be placed in
  // origin-keyed agent cluster with origin key "https://example.com" (because
  // it is not same-origin with the origin key of the agent cluster).
  //
  // When used in ProcessLocks, in the case of an unlocked AllowAnySite process,
  // the key_ will be an empty GURL in non-cross-origin isolated cases. For
  // cross-origin isolated cases, it will be an empty origin (along with the
  // appropriate cross-origin isolation key).
  std::variant<GURL, url::Origin> key_;

  // This is used by DocumentIsolationPolicy to isolate the document in an agent
  // cluster with the appropriate cross-origin isolation status. Setting this to
  // nullopt means that the AgentClusterKey is not cross-origin isolated.
  // TODO(crbug.com/342365083): Currently the CrossOriginIsolationKey is only
  // set based on DocumentIsolationPolicy. It should also be set for documents
  // in a page with COOP and COEP.
  std::optional<AgentClusterKey::CrossOriginIsolationKey> isolation_key_;

  // Tracks the status of the OAC header opt-in request for the navigation that
  // created this AgentClusterKey.
  // Note: this is not taken into account in
  // AgentClusterKey::operator== because we want a document
  // with OAC: 1? to have an AgentClusterKey equal to the AgentClusterKey of a
  // document that got origin isolation through other means
  // (features::kOriginKeyedProcessesByDefault,
  // cross-origin isolation provided the cross-origin isolation status
  // match...). Origin isolation is taken into account because origin-keyed and
  // site-keyed AgentClusterKeys are never equivalent. The precise manner in
  // which it was achieved is not.
  AgentClusterKey::OACStatus oac_status_ =
      AgentClusterKey::OACStatus::kSiteKeyedByDefault;
};

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const AgentClusterKey& agent_cluster_key);

}  // namespace content

#endif  // CONTENT_BROWSER_AGENT_CLUSTER_KEY_H_
