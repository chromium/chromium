// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"

namespace blink {

GlobalScopeCreationParams::GlobalScopeCreationParams(
    const KURL& script_url,
    mojom::blink::ScriptType script_type,
    const String& global_scope_name,
    const String& user_agent,
    const absl::optional<UserAgentMetadata>& ua_metadata,
    scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr>
        outside_content_security_policies,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr>
        response_content_security_policies,
    network::mojom::ReferrerPolicy referrer_policy,
    const SecurityOrigin* starter_origin,
    bool starter_secure_context,
    HttpsState starter_https_state,
    WorkerClients* worker_clients,
    std::unique_ptr<WebContentSettingsClient> content_settings_client,
    const Vector<OriginTrialFeature>* inherited_trial_features,
    const base::UnguessableToken& parent_devtools_token,
    std::unique_ptr<WorkerSettings> worker_settings,
    mojom::blink::V8CacheOptions v8_cache_options,
    WorkletModuleResponsesMap* module_responses_map,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
        browser_interface_broker,
    mojo::PendingRemote<mojom::blink::CodeCacheHost> code_cache_host_interface,
    mojo::PendingRemote<mojom::blink::BlobURLStore> blob_url_store,
    BeginFrameProviderParams begin_frame_provider_params,
    const PermissionsPolicy* parent_permissions_policy,
    base::UnguessableToken agent_cluster_id,
    ukm::SourceId ukm_source_id,
    const absl::optional<ExecutionContextToken>& parent_context_token,
    bool parent_cross_origin_isolated_capability,
    bool parent_is_isolated_context,
    InterfaceRegistry* interface_registry,
    scoped_refptr<base::SingleThreadTaskRunner>
        agent_group_scheduler_compositor_task_runner,
    const SecurityOrigin* top_level_frame_security_origin)
    : script_url(script_url),
      script_type(script_type),
      global_scope_name(global_scope_name),
      user_agent(user_agent),
      ua_metadata(ua_metadata.value_or(blink::UserAgentMetadata())),
      web_worker_fetch_context(std::move(web_worker_fetch_context)),
      outside_content_security_policies(
          std::move(outside_content_security_policies)),
      response_content_security_policies(
          std::move(response_content_security_policies)),
      referrer_policy(referrer_policy),
      starter_origin(starter_origin ? starter_origin->IsolatedCopy() : nullptr),
      starter_secure_context(starter_secure_context),
      starter_https_state(starter_https_state),
      worker_clients(worker_clients),
      content_settings_client(std::move(content_settings_client)),
      parent_devtools_token(parent_devtools_token),
      worker_settings(std::move(worker_settings)),
      v8_cache_options(v8_cache_options),
      module_responses_map(module_responses_map),
      browser_interface_broker(std::move(browser_interface_broker)),
      code_cache_host_interface(std::move(code_cache_host_interface)),
      blob_url_store(std::move(blob_url_store)),
      begin_frame_provider_params(std::move(begin_frame_provider_params)),
      // At the moment, workers do not support their container policy being set,
      // so it will just be an empty ParsedPermissionsPolicy for now.
      // Shared storage worklets have a null `parent_permissions_policy` and
      // `starter_origin`.
      // TODO(crbug.com/1419253): Pass non-null `parent_permissions_policy` and
      // `starter_origin`. Also, we could ensure `starter_origin` is never null
      // after that.
      worker_permissions_policy(PermissionsPolicy::CreateFromParentPolicy(
          parent_permissions_policy,
          ParsedPermissionsPolicy() /* container_policy */,
          starter_origin ? starter_origin->ToUrlOrigin() : url::Origin())),
      agent_cluster_id(agent_cluster_id),
      ukm_source_id(ukm_source_id),
      parent_context_token(parent_context_token),
      parent_cross_origin_isolated_capability(
          parent_cross_origin_isolated_capability),
      parent_is_isolated_context(parent_is_isolated_context),
      interface_registry(interface_registry),
      agent_group_scheduler_compositor_task_runner(
          std::move(agent_group_scheduler_compositor_task_runner)),
      top_level_frame_security_origin(
          top_level_frame_security_origin
              ? top_level_frame_security_origin->IsolatedCopy()
              : nullptr) {
  this->inherited_trial_features =
      std::make_unique<Vector<OriginTrialFeature>>();
  if (inherited_trial_features) {
    for (OriginTrialFeature feature : *inherited_trial_features)
      this->inherited_trial_features->push_back(feature);
  }
}

}  // namespace blink
