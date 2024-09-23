// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGENT_CLUSTER_KEY_H_
#define CONTENT_BROWSER_AGENT_CLUSTER_KEY_H_

#include <optional>

#include "content/browser/security/coop/cross_origin_isolation_mode.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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
    bool operator!=(const CrossOriginIsolationKey& b) const;
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

  // Note: CreateSiteKeyed and CreateOriginKeyed are currently only used in
  // tests. Eventually, we will refactor the Origin-Agent-Cluster code so that
  // all navigations receive an AgentClusterKey. See crbug.com/342365078.
  // Following the deprecation of document.domain by default (a.k.a.
  // Origin-Agent-Cluster by default), AgentClusterKeys should be origin keyed
  // unless the document sends a "Origin-Agent-Cluster: ?0" header. However,
  // without SiteInstanceGroup, this would lead to extra process creation. So
  // when computing AgentClusterKeys for all navigations, we might need to make
  // them site-keyed by default until SiteInstanceGroup ships.
  // See crbug.com/40176090.
  static AgentClusterKey CreateSiteKeyed(const GURL& site_url);
  static AgentClusterKey CreateOriginKeyed(const url::Origin& origin);

  static AgentClusterKey CreateWithCrossOriginIsolationKey(
      const url::Origin& origin,
      const AgentClusterKey::CrossOriginIsolationKey& isolation_key);

  AgentClusterKey(const AgentClusterKey& other);
  ~AgentClusterKey();

  // Whether the Agent Cluster is keyed using Site URL or Origin.
  bool IsSiteKeyed() const;
  bool IsOriginKeyed() const;

  // The site URL or the origin of the AgentClusterKey. Each function should
  // only be called when the Agent Cluster is site-keyed or origin-keyed
  // respectively. The functions will CHECK fail if called in the wrong cases.
  const GURL& GetSite() const;
  const url::Origin& GetOrigin() const;

  // Returns nullopt if the AgentClusterKey is not cross-origin isolated.
  // Otherwise, returns the CrossOriginIsolationKey associated to the
  // AgentClusterKey.
  const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
  GetCrossOriginIsolationKey() const;

  bool operator==(const AgentClusterKey& b) const;
  bool operator!=(const AgentClusterKey& b) const;

  // Needed for tie comparisons in SiteInfo.
  bool operator<(const AgentClusterKey& b) const;

 private:
  AgentClusterKey(const absl::variant<GURL, url::Origin>& key,
                  const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
                      isolation_key);

  // The key used for the agent cluster. By default, this is a site URL.
  absl::variant<GURL, url::Origin> key_;

  // This is used by DocumentIsolationPolicy to isolate the document in an agent
  // cluster with the appropriate cross-origin isolation status. Setting this to
  // nullopt means that the AgentClusterKey is not cross-origin isolated.
  // TODO(crbug.com/342365083): Currently the CrossOriginIsolationKey is only
  // set based on DocumentIsolationPolicy. It should also be set for documents
  // in a page with COOP and COEP.
  std::optional<AgentClusterKey::CrossOriginIsolationKey> isolation_key_;
};

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const AgentClusterKey& agent_cluster_key);

}  // namespace content

#endif  // CONTENT_BROWSER_AGENT_CLUSTER_KEY_H_
