// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"

#include <memory>

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"

namespace blink {

GlobalScopeCreationParams::GlobalScopeCreationParams(
    const KURL& script_url,
    mojom::blink::ScriptType script_type,
    const String& global_scope_name,
    const String& user_agent,
    const base::Optional<UserAgentMetadata>& ua_metadata,
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context,
    const Vector<CSPHeaderAndType>& outside_content_security_policy_headers,
    network::mojom::ReferrerPolicy referrer_policy,
    const SecurityOrigin* starter_origin,
    bool starter_secure_context,
    HttpsState starter_https_state,
    WorkerClients* worker_clients,
    std::unique_ptr<WebContentSettingsClient> content_settings_client,
    base::Optional<network::mojom::IPAddressSpace> response_address_space,
    const Vector<String>* origin_trial_tokens,
    const base::UnguessableToken& parent_devtools_token,
    std::unique_ptr<WorkerSettings> worker_settings,
    mojom::blink::V8CacheOptions v8_cache_options,
    WorkletModuleResponsesMap* module_responses_map,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
        browser_interface_broker,
    BeginFrameProviderParams begin_frame_provider_params,
    const FeaturePolicy* parent_feature_policy,
    base::UnguessableToken agent_cluster_id,
    const base::Optional<ExecutionContextToken>& parent_context_token,
    bool parent_cross_origin_isolated_capability)
    : script_url(script_url.Copy()),
      script_type(script_type),
      global_scope_name(global_scope_name.IsolatedCopy()),
      user_agent(user_agent.IsolatedCopy()),
      ua_metadata(ua_metadata.value_or(blink::UserAgentMetadata())),
      web_worker_fetch_context(std::move(web_worker_fetch_context)),
      referrer_policy(referrer_policy),
      starter_origin(starter_origin ? starter_origin->IsolatedCopy() : nullptr),
      starter_secure_context(starter_secure_context),
      starter_https_state(starter_https_state),
      worker_clients(worker_clients),
      content_settings_client(std::move(content_settings_client)),
      response_address_space(response_address_space),
      parent_devtools_token(parent_devtools_token),
      worker_settings(std::move(worker_settings)),
      v8_cache_options(v8_cache_options),
      module_responses_map(module_responses_map),
      browser_interface_broker(std::move(browser_interface_broker)),
      begin_frame_provider_params(std::move(begin_frame_provider_params)),
      // At the moment, workers do not support their container policy being set,
      // so it will just be an empty ParsedFeaturePolicy for now.
      worker_feature_policy(FeaturePolicy::CreateFromParentPolicy(
          parent_feature_policy,
          ParsedFeaturePolicy() /* container_policy */,
          starter_origin->ToUrlOrigin())),
      agent_cluster_id(agent_cluster_id),
      parent_context_token(parent_context_token),
      parent_cross_origin_isolated_capability(
          parent_cross_origin_isolated_capability) {
  this->outside_content_security_policy_headers.ReserveInitialCapacity(
      outside_content_security_policy_headers.size());
  for (const auto& header : outside_content_security_policy_headers) {
    this->outside_content_security_policy_headers.emplace_back(
        header.first.IsolatedCopy(), header.second);
  }

  this->origin_trial_tokens = std::make_unique<Vector<String>>();
  if (origin_trial_tokens) {
    for (const String& token : *origin_trial_tokens)
      this->origin_trial_tokens->push_back(token.IsolatedCopy());
  }
}

}  // namespace blink
