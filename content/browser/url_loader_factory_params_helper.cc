// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_params_helper.h"

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/runtime_feature_state/runtime_feature_state_document_data.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_message.h"
#include "net/base/isolation_info.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/shared_dictionary_access_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

namespace {

// Whether loading state updates to the
// network::mojom::URLLoaderNetworkServiceObserver are inhibited for URLLoaders
// created via URLLoaderFactoryParamsHelper.
//
// network::mojom::URLLoaderNetworkServiceObserver::OnLoadingStateUpdate is
// among the most frequent Mojo messages in traces from the field
// (go/mojos-in-field-traces-2022). Inhibiting the messages has been tested all
// the way to stable with no ill effect and performance gains.
//
// Remove when evaluation of combined performance gains is complete
// crbug.com/1487544.
BASE_FEATURE(kInhibitLoadingStateUpdate,
             "InhibitLoadingStateUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Helper used by the public URLLoaderFactoryParamsHelper::Create... methods.
//
// |origin| is the origin that will use the URLLoaderFactory.
// |origin| is typically the same as the origin in
// network::ResourceRequest::request_initiator, except when
// |is_for_isolated_world|.  See also the doc comment for
// extensions::URLLoaderFactoryManager::CreateFactory.
network::mojom::URLLoaderFactoryParamsPtr CreateParams(
    RenderProcessHost* process,
    const url::Origin& origin,
    const url::Origin& request_initiator_origin_lock,
    bool is_trusted,
    const std::optional<blink::LocalFrameToken>& top_frame_token,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    bool allow_universal_access_from_file_urls,
    bool is_for_isolated_world,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
        trust_token_observer,
    mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
        shared_dictionary_observer,
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
    network::mojom::TrustTokenOperationPolicyVerdict
        trust_token_issuance_policy,
    network::mojom::TrustTokenOperationPolicyVerdict
        trust_token_redemption_policy,
    net::CookieSettingOverrides cookie_setting_overrides,
    std::string_view debug_tag,
    bool require_cross_site_request_for_cookies) {
  DCHECK(process);

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();

  params->process_id = process->GetID();
  params->request_initiator_origin_lock = request_initiator_origin_lock;

  params->is_trusted = is_trusted;
  if (top_frame_token)
    params->top_frame_id = top_frame_token.value().value();
  params->isolation_info = isolation_info;

  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  params->client_security_state = std::move(client_security_state);
  params->coep_reporter = std::move(coep_reporter);

  if (params->disable_web_security) {
    // --disable-web-security also disables Opaque Response Blocking (ORB).
    params->is_orb_enabled = false;
  } else if (allow_universal_access_from_file_urls &&
             origin.scheme() == url::kFileScheme) {
    // allow_universal_access_from_file_urls disables ORB (via
    // `is_orb_enabled`) and CORS (via `disable_web_security`) for requests
    // made from a file: |origin|.
    params->is_orb_enabled = false;
    params->disable_web_security = true;
  } else {
    params->is_orb_enabled = true;
  }

  params->trust_token_issuance_policy = trust_token_issuance_policy;
  params->trust_token_redemption_policy = trust_token_redemption_policy;

  // If we have a URLLoaderNetworkObserver, request loading state updates.
  if (url_loader_network_observer &&
      !base::FeatureList::IsEnabled(kInhibitLoadingStateUpdate)) {
    params->provide_loading_state_updates = true;
  }

  GetContentClient()->browser()->OverrideURLLoaderFactoryParams(
      process->GetBrowserContext(), origin, is_for_isolated_world,
      params.get());

  params->cookie_observer = std::move(cookie_observer);
  params->trust_token_observer = std::move(trust_token_observer);
  params->shared_dictionary_observer = std::move(shared_dictionary_observer);
  params->url_loader_network_observer = std::move(url_loader_network_observer);
  params->devtools_observer = std::move(devtools_observer);

  params->cookie_setting_overrides = cookie_setting_overrides;

  params->debug_tag = std::string(debug_tag);

  params->require_cross_site_request_for_cookies =
      require_cross_site_request_for_cookies;

  return params;
}

}  // namespace

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForFrame(
    RenderFrameHostImpl* frame,
    const url::Origin& frame_origin,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    RenderProcessHost* process,
    network::mojom::TrustTokenOperationPolicyVerdict
        trust_token_issuance_policy,
    network::mojom::TrustTokenOperationPolicyVerdict
        trust_token_redemption_policy,
    net::CookieSettingOverrides cookie_setting_overrides,
    std::string_view debug_tag) {
  return CreateParams(
      process,
      frame_origin,  // origin
      frame_origin,  // request_initiator_origin_lock
      false,         // is_trusted
      frame->GetTopFrameToken(), isolation_info,
      std::move(client_security_state), std::move(coep_reporter),
      frame->GetOrCreateWebPreferences().allow_universal_access_from_file_urls,
      false,  // is_for_isolated_world
      frame->CreateCookieAccessObserver(),
      frame->CreateTrustTokenAccessObserver(),
      frame->CreateSharedDictionaryAccessObserver(),
      frame->CreateURLLoaderNetworkObserver(),
      NetworkServiceDevToolsObserver::MakeSelfOwned(frame->frame_tree_node()),
      trust_token_issuance_policy, trust_token_redemption_policy,
      cookie_setting_overrides, debug_tag,
      /*require_cross_site_request_for_cookies=*/false);
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
    RenderFrameHostImpl* frame,
    const url::Origin& isolated_world_origin,
    const url::Origin& main_world_origin,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    network::mojom::TrustTokenOperationPolicyVerdict
        trust_token_issuance_policy,
    network::mojom::TrustTokenOperationPolicyVerdict
        trust_token_redemption_policy,
    net::CookieSettingOverrides cookie_setting_overrides) {
  return CreateParams(
      frame->GetProcess(),
      isolated_world_origin,  // origin
      main_world_origin,      // request_initiator_origin_lock
      false,                  // is_trusted
      frame->GetTopFrameToken(), isolation_info,
      std::move(client_security_state),
      mojo::NullRemote(),  // coep_reporter
      frame->GetOrCreateWebPreferences().allow_universal_access_from_file_urls,
      true,  // is_for_isolated_world
      frame->CreateCookieAccessObserver(),
      frame->CreateTrustTokenAccessObserver(),
      frame->CreateSharedDictionaryAccessObserver(),
      frame->CreateURLLoaderNetworkObserver(),
      NetworkServiceDevToolsObserver::MakeSelfOwned(frame->frame_tree_node()),
      trust_token_issuance_policy, trust_token_redemption_policy,
      cookie_setting_overrides, "ParamHelper::CreateForIsolatedWorld",
      /*require_cross_site_request_for_cookies=*/false);
}

network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForPrefetch(
    RenderFrameHostImpl* frame,
    network::mojom::ClientSecurityStatePtr client_security_state,
    net::CookieSettingOverrides cookie_setting_overrides) {
  // The factory client |is_trusted| to control the |network_isolation_key| in
  // each separate request (rather than forcing the client to use the key
  // specified in URLLoaderFactoryParams).
  const url::Origin& frame_origin = frame->GetLastCommittedOrigin();
  return CreateParams(
      frame->GetProcess(),
      frame_origin,  // origin
      frame_origin,  // request_initiator_origin_lock
      true,          // is_trusted
      frame->GetTopFrameToken(),
      net::IsolationInfo(),  // isolation_info
      std::move(client_security_state),
      mojo::NullRemote(),  // coep_reporter
      frame->GetOrCreateWebPreferences().allow_universal_access_from_file_urls,
      false,  // is_for_isolated_world
      frame->CreateCookieAccessObserver(),
      frame->CreateTrustTokenAccessObserver(),
      frame->CreateSharedDictionaryAccessObserver(),
      frame->CreateURLLoaderNetworkObserver(),
      NetworkServiceDevToolsObserver::MakeSelfOwned(frame->frame_tree_node()),
      network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
      network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
      cookie_setting_overrides, "ParamHelper::CreateForPrefetch",
      /*require_cross_site_request_for_cookies=*/false);
}

// static
// TODO(crbug.com/40190528): make sure client_security_state is no longer
// nullptr anywhere.
// TODO(crbug.com/40247160): Investigate whether to support cookie setting
// overrides (hardcoded empty set used for now).
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForWorker(
    RenderProcessHost* process,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<network::mojom::DevToolsObserver> devtools_observer,
    network::mojom::ClientSecurityStatePtr client_security_state,
    std::string_view debug_tag,
    bool require_cross_site_request_for_cookies) {
  return CreateParams(
      process,
      request_initiator,  // origin
      request_initiator,  // request_initiator_origin_lock
      false,              // is_trusted
      std::nullopt,       // top_frame_token
      isolation_info, std::move(client_security_state),
      std::move(coep_reporter),
      false,  // allow_universal_access_from_file_urls
      false,  // is_for_isolated_world
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition())
          ->CreateCookieAccessObserverForServiceWorker(),
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition())
          ->CreateTrustTokenAccessObserverForServiceWorker(),
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition())
          ->CreateSharedDictionaryAccessObserverForServiceWorker(),
      std::move(url_loader_network_observer), std::move(devtools_observer),
      // Trust Token redemption and signing operations require the Permissions
      // Policy. It seems Permissions Policy in worker contexts
      // is currently an open issue (as of 06/21/2022):
      // https://github.com/w3c/webappsec-permissions-policy/issues/207.
      network::mojom::TrustTokenOperationPolicyVerdict::kPotentiallyPermit,
      network::mojom::TrustTokenOperationPolicyVerdict::kPotentiallyPermit,
      net::CookieSettingOverrides(), debug_tag,
      require_cross_site_request_for_cookies);
}

// static
// TODO(crbug.com/40247160): Investigate whether to support cookie setting
// overrides (hardcoded empty set used for now).
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForEarlyHintsPreload(
    RenderProcessHost* process,
    const url::Origin& tentative_origin,
    NavigationRequest& navigation_request,
    const network::mojom::EarlyHints& early_hints,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    mojo::PendingRemote<network::mojom::TrustTokenAccessObserver>
        trust_token_observer,
    mojo::PendingRemote<network::mojom::SharedDictionaryAccessObserver>
        shared_dictionary_observer) {
  // TODO(crbug.com/40188470): Consider not using the speculative
  // RenderFrameHostImpl to create URLLoaderNetworkServiceObserver.
  // In general we should avoid using speculative RenderFrameHostImpl
  // to fill URLLoaderFactoryParams because some parameters can be calculated
  // only after the RenderFrameHostImpl is committed.
  // See also the design doc linked from the bug entry. It describes options
  // to create the observer without RenderFrameHostImpl.
  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer = navigation_request.frame_tree_node()
                                        ->current_frame_host()
                                        ->CreateURLLoaderNetworkObserver();

  auto isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther,
      /*top_frame_origin=*/tentative_origin, /*frame_origin=*/tentative_origin,
      net::SiteForCookies::FromOrigin(tentative_origin));

  // TODO(https://issues.chromium.org/issues/336754077):
  // Support Document-Isolation-Policy in early hints headers instead of passing
  // a default DocumentIsolationPolicy.
  network::mojom::ClientSecurityStatePtr client_security_state =
      network::mojom::ClientSecurityState::New(
          early_hints.headers->cross_origin_embedder_policy,
          network::IsOriginPotentiallyTrustworthy(tentative_origin),
          early_hints.ip_address_space,
          network::mojom::PrivateNetworkRequestPolicy::kBlock,
          network::DocumentIsolationPolicy());

  return CreateParams(
      process, /*origin=*/tentative_origin,
      /*request_initiator_origin_lock=*/tentative_origin,
      /*is_trusted=*/false, /*top_frame_token=*/std::nullopt, isolation_info,
      std::move(client_security_state),
      /*coep_reporter=*/mojo::NullRemote(),
      /*allow_universal_access_from_file_urls=*/false,
      /*is_for_isolated_world=*/false, std::move(cookie_observer),
      std::move(trust_token_observer), std::move(shared_dictionary_observer),
      std::move(url_loader_network_observer),
      /*devtools_observer=*/mojo::NullRemote(),
      network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
      network::mojom::TrustTokenOperationPolicyVerdict::kForbid,
      net::CookieSettingOverrides(), "ParamHelper::CreateForEarlyHintsPreload",
      /*require_cross_site_request_for_cookies=*/false);
}

}  // namespace content
