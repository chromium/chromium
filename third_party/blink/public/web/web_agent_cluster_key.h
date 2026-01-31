// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AGENT_CLUSTER_KEY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AGENT_CLUSTER_KEY_H_

#include <optional>
#include <variant>

#include "third_party/blink/public/mojom/frame/agent_cluster_key.mojom-shared.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {
class AgentClusterKey;

// These structs are used to pass the AgentClusterKey received in
// CommitNavigationParams to blink so that it can be used to select an agent
// cluster for the navigation.
struct WebCrossOriginIsolationKey {
  WebSecurityOrigin common_origin;
  mojom::CrossOriginIsolationMode mode;
};

// This is the origin-keyed version of a WebAgentClusterKey. Note that it can
// contain an optional WebCrossOriginIsolationKey when the context is
// cross-origin isolated.
struct WebOriginKeyedAgentClusterKey {
  WebSecurityOrigin origin;
  std::optional<WebCrossOriginIsolationKey> isolation_key;
};

class BLINK_EXPORT WebAgentClusterKey {
 public:
  WebAgentClusterKey();
  explicit WebAgentClusterKey(const WebURL& url);
  explicit WebAgentClusterKey(const WebOriginKeyedAgentClusterKey& key);

#if INSIDE_BLINK
  WebAgentClusterKey(const AgentClusterKey& key);
  operator AgentClusterKey() const;
#endif

 private:
  std::variant<WebOriginKeyedAgentClusterKey, WebURL> key_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AGENT_CLUSTER_KEY_H_
