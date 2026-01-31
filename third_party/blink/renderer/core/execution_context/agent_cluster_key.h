// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_CLUSTER_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_CLUSTER_KEY_H_

#include <optional>
#include <variant>

#include "base/dcheck_is_on.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/frame/agent_cluster_key.mojom-blink.h"
#include "third_party/blink/public/web/web_agent_cluster_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

// AgentClusterKey represents the implementation in the renderer process of the
// AgentClusterKey concept of the HTML spec:
// https://html.spec.whatwg.org/multipage/webappapis.html#agent-cluster-key
//
// The AgentClusterKey is first computed in the browser process (see
// content/browser/agent_cluster_key.h). It is passed down to the renderer
// process at commit time and used to select an Agent for the document. Each
// Agent has an associated AgentClusterKey.
class CORE_EXPORT AgentClusterKey {
 public:
  AgentClusterKey(const AgentClusterKey& other);
  ~AgentClusterKey();

  static AgentClusterKey CreateSiteKeyed(const KURL& site_url);
  static AgentClusterKey CreateOriginKeyed(
      scoped_refptr<const SecurityOrigin> origin);

  static AgentClusterKey CreateUniversalFileAgent();

  // Cross-origin isolated agent clusters have an additional isolation key that
  // tracks the origin that enabled cross-origin isolation.
  // https://wicg.github.io/document-isolation-policy/#coi-agent-cluster-key
  struct CrossOriginIsolationKey {
    CrossOriginIsolationKey(scoped_refptr<const SecurityOrigin> common_origin,
                            mojom::blink::CrossOriginIsolationMode mode);
    CrossOriginIsolationKey(const CrossOriginIsolationKey& other);
    virtual ~CrossOriginIsolationKey();
    bool operator==(const CrossOriginIsolationKey& b) const;

    // The origin of the document which triggered cross-origin isolation. This
    // might be different from the origin returned by AgentClusterKey::GetOrigin
    // when cross-origin isolation was enabled by COOP + COEP. It should always
    // match when cross-origin isolation was enabled by
    // Document-Isolation-Policy.
    scoped_refptr<const SecurityOrigin> common_origin;

    // Whether cross-origin isolation is effective or logical. Effective
    // cross-origin isolation grants access to extra web APIs. Some platforms
    // might not have the process model needed to support cross-origin
    // isolation. In this case, the web-visible isolation restrictions apply,
    // but do not lead to access to extra APIs. This is logical cross-origin
    // isolation.
    mojom::blink::CrossOriginIsolationMode mode;
  };
  static AgentClusterKey CreateWithCrossOriginIsolationKey(
      scoped_refptr<const SecurityOrigin> origin,
      const CrossOriginIsolationKey& isolation_key);

  // Used to create empty and deleted values for a HeapHashMap. This should only
  // ever be used by HashTraits.
  using PassKey = base::PassKey<HashTraits<AgentClusterKey>>;
  static AgentClusterKey CreateEmpty(PassKey);
  static AgentClusterKey CreateDeleted(PassKey);
  bool IsEmpty(PassKey) const { return std::holds_alternative<Empty>(key_); }

  bool IsDeleted(PassKey) const {
    return std::holds_alternative<Deleted>(key_);
  }

  // Whether the Agent Cluster is keyed using Site URL or Origin.
  bool IsSiteKeyed() const;
  bool IsOriginKeyed() const;

  // Whether the Agent Cluster is the universal file AgentCluster, shared
  // between all file URLs.
  bool IsUniversalFileAgent() const;

  // Returns the origin or URL that keys the AgentClusterKey. Note that the
  // the functions should only be used if the AgentClusterKey is origin-keyed or
  // site-keyed respectively.
  const SecurityOrigin* GetOrigin() const {
    return std::get<OriginKey>(key_).origin.get();
  }
  const KURL& GetURL() const { return std::get<KURL>(key_); }

  const CrossOriginIsolationKey* GetCrossOriginIsolationKey() const;

  bool operator==(const AgentClusterKey& b) const;

 private:
  friend class WebAgentClusterKey;

  // Marker for the universal file agent.
  struct File {
    bool operator==(const File& b) const;
  };

  // Tombstone markers for `blink::HashTraits`.
  struct Empty {
    bool operator==(const Empty& b) const;
  };
  struct Deleted {
    bool operator==(const Deleted& b) const;
  };

  // See comments below.
  struct OriginKey {
    scoped_refptr<const SecurityOrigin> origin;
    std::optional<CrossOriginIsolationKey> isolation_key;
    bool operator==(const OriginKey& b) const;
  };

  explicit AgentClusterKey(const KURL& site_url);
  explicit AgentClusterKey(const OriginKey& origin_key);
  explicit AgentClusterKey(Empty);
  explicit AgentClusterKey(Deleted);
  explicit AgentClusterKey(File);

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
  // Origin-keyed AgentClusterKey may also have an optional
  // CrossOriginIsolationKey (see OriginKey strict above). It is used by
  // DocumentIsolationPolicy and COOP and COEP to isolate execution contexts
  // with extra-capabilities in an agent cluster with the appropriate
  // cross-origin isolation status. Setting the cross-origin isolation key to
  // nullopt means that the AgentClusterKey is not cross-origin isolated.
  std::variant<KURL, OriginKey, Empty, Deleted, File> key_;
};

template <>
struct HashTraits<AgentClusterKey> : GenericHashTraits<AgentClusterKey> {
  using PassKey = base::PassKey<HashTraits<AgentClusterKey>>;
  static unsigned GetHash(const AgentClusterKey& agent_cluster_key) {
    unsigned cross_origin_isolation_mode = 0;
    if (agent_cluster_key.GetCrossOriginIsolationKey()) {
      switch (agent_cluster_key.GetCrossOriginIsolationKey()->mode) {
        case mojom::blink::CrossOriginIsolationMode::kLogical:
          cross_origin_isolation_mode = 1;
          break;
        case mojom::blink::CrossOriginIsolationMode::kConcrete:
          cross_origin_isolation_mode = 2;
          break;
          NOTREACHED();
      }
    }
    unsigned key_status = 0;
    if (agent_cluster_key.IsOriginKeyed()) {
      key_status = 1;
    }
    if (agent_cluster_key.IsEmpty(PassKey())) {
      key_status |= (1 << 1);
    }
    if (agent_cluster_key.IsDeleted(PassKey())) {
      key_status |= (1 << 2);
    }
    if (agent_cluster_key.IsUniversalFileAgent()) {
      key_status |= (1 << 3);
    }
    unsigned hash_codes[] = {
        key_status,
        agent_cluster_key.IsOriginKeyed()
            ? HashTraits<scoped_refptr<const SecurityOrigin>>::GetHash(
                  agent_cluster_key.GetOrigin())
            : 0,
        agent_cluster_key.IsSiteKeyed() && !agent_cluster_key.GetURL().IsNull()
            ? HashTraits<KURL>::GetHash(agent_cluster_key.GetURL())
            : 0,
        agent_cluster_key.GetCrossOriginIsolationKey()
            ? HashTraits<scoped_refptr<const SecurityOrigin>>::GetHash(
                  agent_cluster_key.GetCrossOriginIsolationKey()->common_origin)
            : 0,
        cross_origin_isolation_mode,
    };
    return StringHasher::HashMemory(base::as_byte_span(hash_codes));
  }

  static AgentClusterKey& EmptyValue() {
    DEFINE_STATIC_LOCAL(AgentClusterKey, kEmpty,
                        (AgentClusterKey::CreateEmpty(PassKey())));
    return kEmpty;
  }
  static AgentClusterKey& DeletedValue() {
    DEFINE_STATIC_LOCAL(AgentClusterKey, kDeleted,
                        (AgentClusterKey::CreateDeleted(PassKey())));
    return kDeleted;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_AGENT_CLUSTER_KEY_H_
