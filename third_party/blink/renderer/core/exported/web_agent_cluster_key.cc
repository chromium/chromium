// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_agent_cluster_key.h"

#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/renderer/core/execution_context/agent_cluster_key.h"

namespace blink {

WebAgentClusterKey::WebAgentClusterKey() : WebAgentClusterKey(WebURL()) {}

WebAgentClusterKey::WebAgentClusterKey(const WebURL& url) : key_(url) {}

WebAgentClusterKey::WebAgentClusterKey(const WebOriginKeyedAgentClusterKey& key)
    : key_(key) {}

WebAgentClusterKey::WebAgentClusterKey(const AgentClusterKey& key)
    : key_(std::visit(
          absl::Overload{
              [](const KURL& site_url) -> decltype(key_) {
                return WebURL(site_url);
              },
              [](const AgentClusterKey::OriginKey& origin_key)
                  -> decltype(key_) {
                if (origin_key.isolation_key.has_value()) {
                  return WebOriginKeyedAgentClusterKey{
                      .origin = origin_key.origin,
                      .isolation_key = WebCrossOriginIsolationKey{
                          .common_origin =
                              origin_key.isolation_key->common_origin,
                          .mode = origin_key.isolation_key->mode}};
                }
                return WebOriginKeyedAgentClusterKey{
                    .origin = origin_key.origin,
                };
              },
              [](const AgentClusterKey::File& file) -> decltype(key_) {
                // This is the universal file AgentClusterKey, which is a blink
                // internal. Return a site-keyed key with a default file URL.
                return WebURL(KURL("file:///"));
              },
              [](const auto&) -> decltype(key_) { NOTREACHED(); },
          },
          key.key_)) {}

WebAgentClusterKey::operator AgentClusterKey() const {
  return std::visit(
      absl::Overload{
          [](const WebURL& site_url) {
            return AgentClusterKey::CreateSiteKeyed(site_url);
          },
          [](const WebOriginKeyedAgentClusterKey& cluster_key) {
            if (cluster_key.isolation_key.has_value()) {
              const AgentClusterKey::CrossOriginIsolationKey coi_key =
                  AgentClusterKey::CrossOriginIsolationKey(
                      cluster_key.isolation_key->common_origin,
                      cluster_key.isolation_key->mode);
              return AgentClusterKey::CreateWithCrossOriginIsolationKey(
                  cluster_key.origin, coi_key);
            }
            return AgentClusterKey::CreateOriginKeyed(cluster_key.origin);
          }},
      key_);
}

}  // namespace blink
