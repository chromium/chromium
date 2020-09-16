// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_params_helper.h"

#include "base/command_line.h"
#include "base/optional.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_message.h"
#include "net/base/isolation_info.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

namespace {

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
    const base::Optional<base::UnguessableToken>& top_frame_token,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    bool allow_universal_access_from_file_urls,
    bool is_for_isolated_world,
    mojo::PendingRemote<network::mojom::CookieAccessObserver> cookie_observer,
    network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy,
    base::StringPiece debug_tag) {
  DCHECK(process);

  // "chrome-guest://..." is never used as a main or isolated world origin.
  DCHECK_NE(kGuestScheme, origin.scheme());
  DCHECK_NE(kGuestScheme, request_initiator_origin_lock.scheme());

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();

  params->process_id = process->GetID();
  params->request_initiator_origin_lock = request_initiator_origin_lock;

  params->is_trusted = is_trusted;
  params->top_frame_id = top_frame_token;
  params->isolation_info = isolation_info;

  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  params->client_security_state = std::move(client_security_state);
  params->coep_reporter = std::move(coep_reporter);

  if (params->disable_web_security) {
    // --disable-web-security also disables Cross-Origin Read Blocking (CORB).
    params->is_corb_enabled = false;
  } else if (allow_universal_access_from_file_urls &&
             origin.scheme() == url::kFileScheme) {
    // allow_universal_access_from_file_urls disables CORB (via
    // |is_corb_enabled|) and CORS (via |disable_web_security|) for requests
    // made from a file: |origin|.
    params->is_corb_enabled = false;
    params->disable_web_security = true;
  } else {
    params->is_corb_enabled = true;
  }

  params->trust_token_redemption_policy = trust_token_redemption_policy;

  GetContentClient()->browser()->OverrideURLLoaderFactoryParams(
      process->GetBrowserContext(), origin, is_for_isolated_world,
      params.get());

  params->cookie_observer = std::move(cookie_observer);

  if (params->debug_tag.empty())
    params->debug_tag = std::string(debug_tag);
  else
    params->debug_tag = std::string(debug_tag) + ": " + params->debug_tag;

  return params;
}

}  // namespace

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForFrame(
    RenderFrameHostImpl* frame,
    const url::Origin& frame_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    RenderProcessHost* process,
    network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy,
    base::StringPiece debug_tag) {
  return CreateParams(
      process,
      frame_origin,  // origin
      frame_origin,  // request_initiator_origin_lock
      false,         // is_trusted
      frame->GetTopFrameToken(), frame->GetIsolationInfoForSubresources(),
      std::move(client_security_state), std::move(coep_reporter),
      WebContents::FromRenderFrameHost(frame)
          ->GetOrCreateWebPreferences()
          .allow_universal_access_from_file_urls,
      false,  // is_for_isolated_world
      frame->CreateCookieAccessObserver(), trust_token_redemption_policy,
      debug_tag);
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForIsolatedWorld(
    RenderFrameHostImpl* frame,
    const url::Origin& isolated_world_origin,
    const url::Origin& main_world_origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    network::mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy) {
  return CreateParams(frame->GetProcess(),
                      isolated_world_origin,  // origin
                      main_world_origin,      // request_initiator_origin_lock
                      false,                  // is_trusted
                      frame->GetTopFrameToken(),
                      frame->GetIsolationInfoForSubresources(),
                      std::move(client_security_state),
                      mojo::NullRemote(),  // coep_reporter
                      WebContents::FromRenderFrameHost(frame)
                          ->GetOrCreateWebPreferences()
                          .allow_universal_access_from_file_urls,
                      true,  // is_for_isolated_world
                      frame->CreateCookieAccessObserver(),
                      trust_token_redemption_policy, "CreateForIsolatedWorld");
}

network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForPrefetch(
    RenderFrameHostImpl* frame,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  // The factory client |is_trusted| to control the |network_isolation_key| in
  // each separate request (rather than forcing the client to use the key
  // specified in URLLoaderFactoryParams).
  const url::Origin& frame_origin = frame->GetLastCommittedOrigin();
  return CreateParams(frame->GetProcess(),
                      frame_origin,  // origin
                      frame_origin,  // request_initiator_origin_lock
                      true,          // is_trusted
                      frame->GetTopFrameToken(),
                      net::IsolationInfo(),  // isolation_info
                      std::move(client_security_state),
                      mojo::NullRemote(),  // coep_reporter
                      WebContents::FromRenderFrameHost(frame)
                          ->GetOrCreateWebPreferences()
                          .allow_universal_access_from_file_urls,
                      false,  // is_for_isolated_world
                      frame->CreateCookieAccessObserver(),
                      network::mojom::TrustTokenRedemptionPolicy::kForbid,
                      "CreateForPrefetch");
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForWorker(
    RenderProcessHost* process,
    const url::Origin& request_initiator,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter,
    base::StringPiece debug_tag) {
  return CreateParams(
      process,
      request_initiator,  // origin
      request_initiator,  // request_initiator_origin_lock
      false,              // is_trusted
      base::nullopt,      // top_frame_token
      isolation_info,
      nullptr,  // client_security_state
      std::move(coep_reporter),
      false,  // allow_universal_access_from_file_urls
      false,  // is_for_isolated_world
      static_cast<StoragePartitionImpl*>(process->GetStoragePartition())
          ->CreateCookieAccessObserverForServiceWorker(),
      // Since ExecutionContext::IsFeatureEnabled returns
      // false in non-Document contexts, no worker should ever
      // execute a trust token redemption or signing operation,
      // as these operations require the Feature Policy feature.
      network::mojom::TrustTokenRedemptionPolicy::kForbid, debug_tag);
}

// static
network::mojom::URLLoaderFactoryParamsPtr
URLLoaderFactoryParamsHelper::CreateForRendererProcess(
    RenderProcessHost* process) {
  // Lock the |request_initiator| to an opaque origin - before something commits
  // in a frame, requests initiated by such frame should use an opaque
  // |request_initiator|.  See also https://crbug.com/1105794 and
  // https://crbug.com/1098938.
  url::Origin request_initiator_origin_lock = url::Origin();

  // Since this function is about to get deprecated (crbug.com/1114822), it
  // should be fine to not add support for isolation info thus using an empty
  // NetworkIsolationKey.
  //
  // We may not be able to allow powerful APIs such as memory measurement APIs
  // (see https://crbug.com/887967) without removing this call.
  net::IsolationInfo isolation_info = net::IsolationInfo::CreateTransient();
  base::Optional<base::UnguessableToken> top_frame_token = base::nullopt;

  return CreateParams(
      process,
      url::Origin(),                  // origin
      request_initiator_origin_lock,  // request_initiator_origin_lock
      false,                          // is_trusted
      top_frame_token, isolation_info,
      nullptr,             // client_security_state
      mojo::NullRemote(),  // coep_reporter
      false,               // allow_universal_access_from_file_urls
      false,               // is_for_isolated_world
      mojo::NullRemote(), network::mojom::TrustTokenRedemptionPolicy::kForbid,
      "CreateForRendererProcess");
}

}  // namespace content
