// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_GLOBAL_SCOPE_CREATION_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_GLOBAL_SCOPE_CREATION_PARAMS_H_

#include <memory>
#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink-forward.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink-forward.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/platform/graphics/begin_frame_provider.h"
#include "third_party/blink/renderer/platform/loader/fetch/https_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class WorkerClients;

// GlobalScopeCreationParams contains parameters for initializing
// WorkerGlobalScope or WorkletGlobalScope.
struct CORE_EXPORT GlobalScopeCreationParams final {
  USING_FAST_MALLOC(GlobalScopeCreationParams);

 public:
  GlobalScopeCreationParams(
      const KURL& script_url,
      mojom::ScriptType script_type,
      const String& global_scope_name,
      const String& user_agent,
      const base::Optional<UserAgentMetadata>& ua_metadata,
      scoped_refptr<WebWorkerFetchContext>,
      const Vector<CSPHeaderAndType>& outside_content_security_policy_headers,
      network::mojom::ReferrerPolicy referrer_policy,
      const SecurityOrigin*,
      bool starter_secure_context,
      HttpsState starter_https_state,
      WorkerClients*,
      std::unique_ptr<WebContentSettingsClient>,
      base::Optional<network::mojom::IPAddressSpace>,
      const Vector<String>* origin_trial_tokens,
      const base::UnguessableToken& parent_devtools_token,
      std::unique_ptr<WorkerSettings>,
      V8CacheOptions,
      WorkletModuleResponsesMap*,
      mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
          browser_interface_broker = mojo::NullRemote(),
      BeginFrameProviderParams begin_frame_provider_params = {},
      const FeaturePolicy* parent_feature_policy = nullptr,
      base::UnguessableToken agent_cluster_id = {},
      const base::Optional<ExecutionContextToken>& parent_context_token =
          base::nullopt);

  ~GlobalScopeCreationParams() = default;

  // The URL to be used as the worker global scope's URL.
  // According to the spec, this should be response URL of the top-level
  // worker script after the top-level worker script is loaded.
  // https://html.spec.whatwg.org/C/#run-a-worker
  //
  // However, this can't be set to response URL in case of module workers or
  // off-the-main-thread fetch, because at the time of GlobalScopeCreationParams
  // creation the response of worker script is not yet received. Therefore,
  // the worker global scope's URL should be set to the response URL outside
  // GlobalScopeCreationParams, but this mechanism is not yet implemented.
  // TODO(crbug/861564): implement this and set the response URL to module
  // workers.
  KURL script_url;

  mojom::ScriptType script_type;

  String global_scope_name;
  String user_agent;
  UserAgentMetadata ua_metadata;

  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context;

  // TODO(bashi): This contains "inside" CSP headers for on-the-main-thread
  // service/shared worker script fetch. Add a separate parameter for "inside"
  // CSP headers.
  Vector<CSPHeaderAndType> outside_content_security_policy_headers;

  network::mojom::ReferrerPolicy referrer_policy;
  std::unique_ptr<Vector<String>> origin_trial_tokens;

  // The SecurityOrigin of the Document creating a Worker/Worklet.
  //
  // For Workers, the origin may have been configured with extra policy
  // privileges when it was created (e.g., enforce path-based file:// origins.)
  // To ensure that these are transferred to the origin of a new worker global
  // scope, supply the Document's SecurityOrigin as the 'starter origin'. See
  // SecurityOrigin::TransferPrivilegesFrom() for details on what privileges are
  // transferred.
  //
  // For Worklets, the origin is used for fetching module scripts. Worklet
  // scripts need to be fetched as sub-resources of the Document, and a module
  // script loader uses Document's SecurityOrigin for security checks.
  scoped_refptr<const SecurityOrigin> starter_origin;

  // Indicates if the Document creating a Worker/Worklet is a secure context.
  //
  // Worklets are defined to have a unique, opaque origin, so are not secure:
  // https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
  // Origin trials are only enabled in secure contexts, and the trial tokens are
  // inherited from the document, so also consider the context of the document.
  // The value should be supplied as the result of Document.IsSecureContext().
  bool starter_secure_context;

  HttpsState starter_https_state;

  // This object is created and initialized on the thread creating
  // a new worker context, but ownership of it and this
  // GlobalScopeCreationParams structure is passed along to the new worker
  // thread, where it is finalized.
  //
  // Hence, CrossThreadPersistent<> is required to allow finalization
  // to happen on a thread different than the thread creating the
  // persistent reference. If the worker thread creation context
  // supplies no extra 'clients', m_workerClients can be left as empty/null.
  CrossThreadPersistent<WorkerClients> worker_clients;

  std::unique_ptr<WebContentSettingsClient> content_settings_client;

  // Worker script response's address space. This is valid only when the worker
  // script is fetched on the main thread (i.e., when
  // |off_main_thread_fetch_option| is kDisabled).
  base::Optional<network::mojom::IPAddressSpace> response_address_space;

  base::UnguessableToken parent_devtools_token;

  std::unique_ptr<WorkerSettings> worker_settings;

  V8CacheOptions v8_cache_options;

  CrossThreadPersistent<WorkletModuleResponsesMap> module_responses_map;

  mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
      browser_interface_broker;

  BeginFrameProviderParams begin_frame_provider_params;

  std::unique_ptr<FeaturePolicy> worker_feature_policy;

  // Set when the worker/worklet has the same AgentClusterID as the execution
  // context that created it (e.g. for a dedicated worker).
  // See https://tc39.github.io/ecma262/#sec-agent-clusters
  base::UnguessableToken agent_cluster_id;

  // The identity of the parent ExecutionContext that is the sole owner of this
  // worker or worklet, which caused it to be created, and to whose lifetime
  // this worker/worklet is bound. This is used for resource usage attribution.
  base::Optional<ExecutionContextToken> parent_context_token;

  DISALLOW_COPY_AND_ASSIGN(GlobalScopeCreationParams);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_GLOBAL_SCOPE_CREATION_PARAMS_H_
