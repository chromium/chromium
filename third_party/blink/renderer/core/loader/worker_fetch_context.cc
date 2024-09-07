// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/worker_global_scope_performance.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_factory.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/virtual_time_controller.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

WorkerFetchContext::~WorkerFetchContext() = default;

WorkerFetchContext::WorkerFetchContext(
    const DetachableResourceFetcherProperties& properties,
    WorkerOrWorkletGlobalScope& global_scope,
    scoped_refptr<WebWorkerFetchContext> web_context,
    SubresourceFilter* subresource_filter,
    ContentSecurityPolicy& content_security_policy,
    WorkerResourceTimingNotifier& resource_timing_notifier)
    : BaseFetchContext(
          properties,
          MakeGarbageCollected<DetachableConsoleLogger>(&global_scope)),
      global_scope_(global_scope),
      web_context_(std::move(web_context)),
      subresource_filter_(subresource_filter),
      content_security_policy_(&content_security_policy),
      content_security_notifier_(&global_scope),
      resource_timing_notifier_(&resource_timing_notifier),
      save_data_enabled_(GetNetworkStateNotifier().SaveDataEnabled()) {
  DCHECK(global_scope.IsContextThread());
  DCHECK(web_context_);
}

net::SiteForCookies WorkerFetchContext::GetSiteForCookies() const {
  return web_context_->SiteForCookies();
}

scoped_refptr<const SecurityOrigin> WorkerFetchContext::GetTopFrameOrigin()
    const {
  std::optional<WebSecurityOrigin> top_frame_origin =
      web_context_->TopFrameOrigin();

  // The top frame origin of shared and service workers is null.
  if (!top_frame_origin) {
    DCHECK(global_scope_->IsSharedWorkerGlobalScope() ||
           global_scope_->IsServiceWorkerGlobalScope());
    return scoped_refptr<const SecurityOrigin>();
  }

  return *top_frame_origin;
}

SubresourceFilter* WorkerFetchContext::GetSubresourceFilter() const {
  return subresource_filter_.Get();
}

bool WorkerFetchContext::AllowScript() const {
  // Script is always allowed in worker fetch contexts, since the fact that
  // they're running is already evidence that script is allowed.
  return true;
}

bool WorkerFetchContext::ShouldBlockRequestByInspector(const KURL& url) const {
  bool should_block_request = false;
  probe::ShouldBlockRequest(Probe(), url, &should_block_request);
  return should_block_request;
}

void WorkerFetchContext::DispatchDidBlockRequest(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    ResourceRequestBlockedReason blocked_reason,
    ResourceType resource_type) const {
  probe::DidBlockRequest(Probe(), resource_request, nullptr, Url(), options,
                         blocked_reason, resource_type);
}

ContentSecurityPolicy* WorkerFetchContext::GetContentSecurityPolicyForWorld(
    const DOMWrapperWorld* world) const {
  // Worker threads don't support per-world CSP. Hence just return the default
  // CSP.
  return GetContentSecurityPolicy();
}

bool WorkerFetchContext::IsIsolatedSVGChromeClient() const {
  return false;
}

void WorkerFetchContext::CountUsage(WebFeature feature) const {
  UseCounter::Count(global_scope_, feature);
}

void WorkerFetchContext::CountDeprecation(WebFeature feature) const {
  Deprecation::CountDeprecation(global_scope_, feature);
}

CoreProbeSink* WorkerFetchContext::Probe() const {
  return probe::ToCoreProbeSink(static_cast<ExecutionContext*>(global_scope_));
}

bool WorkerFetchContext::ShouldBlockWebSocketByMixedContentCheck(
    const KURL& url) const {
  // Worklets don't support WebSocket.
  DCHECK(global_scope_->IsWorkerGlobalScope());
  return !MixedContentChecker::IsWebSocketAllowed(
      *const_cast<WorkerFetchContext*>(this), url);
}

std::unique_ptr<WebSocketHandshakeThrottle>
WorkerFetchContext::CreateWebSocketHandshakeThrottle() {
  return web_context_->CreateWebSocketHandshakeThrottle(
      global_scope_->GetTaskRunner(blink::TaskType::kNetworking));
}

bool WorkerFetchContext::ShouldBlockFetchByMixedContentCheck(
    mojom::blink::RequestContextType request_context,
    network::mojom::blink::IPAddressSpace target_address_space,
    base::optional_ref<const ResourceRequest::RedirectInfo> redirect_info,
    const KURL& url,
    ReportingDisposition reporting_disposition,
    const String& devtools_id) const {
  RedirectStatus redirect_status = redirect_info.has_value()
                                       ? RedirectStatus::kFollowedRedirect
                                       : RedirectStatus::kNoRedirect;
  const KURL& url_before_redirects =
      redirect_info.has_value() ? redirect_info->original_url : url;
  return MixedContentChecker::ShouldBlockFetchOnWorker(
      *const_cast<WorkerFetchContext*>(this), request_context,
      url_before_redirects, redirect_status, url, reporting_disposition,
      global_scope_->IsWorkletGlobalScope());
}

bool WorkerFetchContext::ShouldBlockFetchAsCredentialedSubresource(
    const ResourceRequest& resource_request,
    const KURL& url) const {
  if ((!url.User().empty() || !url.Pass().empty()) &&
      resource_request.GetRequestContext() !=
          mojom::blink::RequestContextType::XML_HTTP_REQUEST) {
    if (Url().User() != url.User() || Url().Pass() != url.Pass()) {
      CountDeprecation(
          WebFeature::kRequestedSubresourceWithEmbeddedCredentials);

      return true;
    }
  }
  return false;
}

const KURL& WorkerFetchContext::Url() const {
  return GetResourceFetcherProperties()
      .GetFetchClientSettingsObject()
      .GlobalObjectUrl();
}

ContentSecurityPolicy* WorkerFetchContext::GetContentSecurityPolicy() const {
  return content_security_policy_.Get();
}

void WorkerFetchContext::PrepareRequest(
    ResourceRequest& request,
    ResourceLoaderOptions& options,
    WebScopedVirtualTimePauser& virtual_time_pauser,
    ResourceType resource_type) {
  request.SetUkmSourceId(GetExecutionContext()->UkmSourceID());

  String user_agent = global_scope_->UserAgent();
  probe::ApplyUserAgentOverride(Probe(), &user_agent);
  DCHECK(!user_agent.IsNull());
  request.SetHTTPUserAgent(AtomicString(user_agent));
  request.SetSharedDictionaryWriterEnabled(
      RuntimeEnabledFeatures::CompressionDictionaryTransportEnabled(
          GetExecutionContext()));

  request.SetStorageAccessApiStatus(
      GetExecutionContext()->GetStorageAccessApiStatus());

  if (!RuntimeEnabledFeatures::
          MinimimalResourceRequestPrepBeforeCacheLookupEnabled()) {
    std::optional<WebURL> overriden_url =
        web_context_->WillSendRequest(request.Url());
    if (overriden_url.has_value()) {
      request.SetUrl(*overriden_url);
    }
  }
  WrappedResourceRequest webreq(request);
  web_context_->FinalizeRequest(webreq);
  if (auto* worker_scope = DynamicTo<WorkerGlobalScope>(*global_scope_)) {
    virtual_time_pauser =
        worker_scope->GetScheduler()
            ->GetVirtualTimeController()
            ->CreateWebScopedVirtualTimePauser(
                request.Url().GetString(),
                WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  }

  probe::PrepareRequest(Probe(), nullptr, request, options, resource_type);
}

void WorkerFetchContext::AddAdditionalRequestHeaders(ResourceRequest& request) {
  // The remaining modifications are only necessary for HTTP and HTTPS.
  if (!request.Url().IsEmpty() && !request.Url().ProtocolIsInHTTPFamily())
    return;

  // TODO(crbug.com/1315612): WARNING: This bypasses the permissions policy.
  // Unfortunately, workers lack a permissions policy and to derive proper hints
  // https://github.com/w3c/webappsec-permissions-policy/issues/207.
  // Save-Data was previously included in hints for workers, thus we cannot
  // remove it for the time being. If you're reading this, consider building
  // permissions policies for workers and/or deprecating this inclusion.
  if (save_data_enabled_)
    request.SetHttpHeaderField(http_names::kSaveData, AtomicString("on"));
}

void WorkerFetchContext::AddResourceTiming(
    mojom::blink::ResourceTimingInfoPtr info,
    const AtomicString& initiator_type) {
  resource_timing_notifier_->AddResourceTiming(std::move(info), initiator_type);
}

void WorkerFetchContext::PopulateResourceRequestBeforeCacheAccess(
    const ResourceLoaderOptions& options,
    ResourceRequest& request) {
  DCHECK(RuntimeEnabledFeatures::
             MinimimalResourceRequestPrepBeforeCacheLookupEnabled());

  MixedContentChecker::UpgradeInsecureRequest(
      request, &GetResourceFetcherProperties().GetFetchClientSettingsObject(),
      global_scope_, mojom::RequestContextFrameType::kNone,
      global_scope_->ContentSettingsClient());
}

void WorkerFetchContext::WillSendRequest(ResourceRequest& request) {
  std::optional<WebURL> overriden_url =
      web_context_->WillSendRequest(request.Url());
  if (overriden_url.has_value()) {
    request.SetUrl(*overriden_url);
  }
}

void WorkerFetchContext::UpgradeResourceRequestForLoader(
    ResourceType type,
    const std::optional<float> resource_width,
    ResourceRequest& out_request,
    const ResourceLoaderOptions& options) {
  if (!GetResourceFetcherProperties().IsDetached())
    probe::SetDevToolsIds(Probe(), out_request, options.initiator_info);
  if (!RuntimeEnabledFeatures::
          MinimimalResourceRequestPrepBeforeCacheLookupEnabled()) {
    MixedContentChecker::UpgradeInsecureRequest(
        out_request,
        &GetResourceFetcherProperties().GetFetchClientSettingsObject(),
        global_scope_, mojom::RequestContextFrameType::kNone,
        global_scope_->ContentSettingsClient());
  }
  SetFirstPartyCookie(out_request);
  if (!out_request.TopFrameOrigin())
    out_request.SetTopFrameOrigin(GetTopFrameOrigin());
}

std::unique_ptr<ResourceLoadInfoNotifierWrapper>
WorkerFetchContext::CreateResourceLoadInfoNotifierWrapper() {
  return web_context_->CreateResourceLoadInfoNotifierWrapper();
}

void WorkerFetchContext::SetFirstPartyCookie(ResourceRequest& out_request) {
  if (out_request.SiteForCookies().IsNull())
    out_request.SetSiteForCookies(GetSiteForCookies());
}

WorkerSettings* WorkerFetchContext::GetWorkerSettings() const {
  auto* scope = DynamicTo<WorkerGlobalScope>(*global_scope_);
  return scope ? scope->GetWorkerSettings() : nullptr;
}

bool WorkerFetchContext::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const KURL& url) const {
  if (!global_scope_->ContentSettingsClient())
    return enabled_per_settings;
  return global_scope_->ContentSettingsClient()->AllowRunningInsecureContent(
      enabled_per_settings, url);
}

mojom::blink::ContentSecurityNotifier&
WorkerFetchContext::GetContentSecurityNotifier() {
  if (!content_security_notifier_.is_bound()) {
    global_scope_->GetBrowserInterfaceBroker().GetInterface(
        content_security_notifier_.BindNewPipeAndPassReceiver(
            global_scope_->GetTaskRunner(TaskType::kInternalLoading)));
  }
  return *content_security_notifier_.get();
}

ExecutionContext* WorkerFetchContext::GetExecutionContext() const {
  return global_scope_.Get();
}

void WorkerFetchContext::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  visitor->Trace(subresource_filter_);
  visitor->Trace(content_security_policy_);
  visitor->Trace(content_security_notifier_);
  BaseFetchContext::Trace(visitor);
}

}  // namespace blink
