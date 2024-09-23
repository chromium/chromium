// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_test_util.h"

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/scheme_host_port_matcher_rule.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/is_browser_initiated.h"
#include "services/network/network_service.h"
#include "services/network/prefetch_matching_url_loader_factory.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/mock_devtools_observer.h"
#include "services/network/test/test_url_loader_client.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {

// TEST URL LOADER FACTORY
// =======================

TestURLLoaderFactory::TestURLLoaderFactory() = default;

TestURLLoaderFactory::~TestURLLoaderFactory() = default;

base::WeakPtr<TestURLLoaderFactory> TestURLLoaderFactory::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void TestURLLoaderFactory::NotifyClientOnReceiveEarlyHints(
    const std::vector<std::pair<std::string, std::string>>& headers) {
  DCHECK(client_remote_);
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  for (const auto& header : headers)
    response_headers->SetHeader(header.first, header.second);
  auto hints = mojom::EarlyHints::New(
      PopulateParsedHeaders(response_headers.get(), GetRequestedURL()),
      mojom::ReferrerPolicy::kDefault, mojom::IPAddressSpace::kPublic);
  client_remote_->OnReceiveEarlyHints(std::move(hints));
}

void TestURLLoaderFactory::NotifyClientOnReceiveResponse(
    int status_code,
    const std::vector<std::pair<std::string, std::string>>& extra_headers,
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(client_remote_);
  auto response = mojom::URLResponseHead::New();
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      base::StringPrintf("HTTP/1.1 %d OK\n"
                         "Content-Type: image/png\n",
                         status_code));
  for (const auto& header : extra_headers)
    response->headers->SetHeader(header.first, header.second);

  client_remote_->OnReceiveResponse(std::move(response), std::move(body),
                                    std::nullopt);
}

void TestURLLoaderFactory::NotifyClientOnComplete(int error_code) {
  DCHECK(client_remote_);
  client_remote_->OnComplete(URLLoaderCompletionStatus(error_code));
}

void TestURLLoaderFactory::NotifyClientOnComplete(
    const CorsErrorStatus& status) {
  DCHECK(client_remote_);
  client_remote_->OnComplete(URLLoaderCompletionStatus(status));
}

void TestURLLoaderFactory::NotifyClientOnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const std::vector<std::pair<std::string, std::string>>& extra_headers) {
  auto response = mojom::URLResponseHead::New();
  response->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      base::StringPrintf("HTTP/1.1 %d\n", redirect_info.status_code));
  for (const auto& header : extra_headers)
    response->headers->SetHeader(header.first, header.second);

  client_remote_->OnReceiveRedirect(redirect_info, std::move(response));
}

void TestURLLoaderFactory::ResetClientRemote() {
  client_remote_.reset();
}

void TestURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  ++num_created_loaders_;
  DCHECK(client);
  request_ = resource_request;
  client_remote_.reset();
  client_remote_.Bind(std::move(client));

  if (on_create_loader_and_start_)
    on_create_loader_and_start_.Run();
}

void TestURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  NOTREACHED_IN_MIGRATION();
}

// RESET FACTORY PARAMS
// ====================

CorsURLLoaderTestBase::ResetFactoryParams::ResetFactoryParams() {
  mojom::URLLoaderFactoryParams params;
  is_trusted = params.is_trusted;
  ignore_isolated_world_origin = params.ignore_isolated_world_origin;
  client_security_state = std::move(params.client_security_state);

  mojom::URLLoaderFactoryOverride factory_override;
  skip_cors_enabled_scheme_check =
      factory_override.skip_cors_enabled_scheme_check;

  url_loader_network_observer = std::move(
      const_cast<mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>&>(
          params.url_loader_network_observer));
}

CorsURLLoaderTestBase::ResetFactoryParams::~ResetFactoryParams() = default;

// CORS URL LOADER TEST BASE
// =========================

CorsURLLoaderTestBase::CorsURLLoaderTestBase(bool shared_dictionary_enabled)
    : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
  net::URLRequestContextBuilder context_builder;
  context_builder.set_proxy_resolution_service(
      net::ConfiguredProxyResolutionService::CreateDirect());
  url_request_context_ = context_builder.Build();

  network_service_ = NetworkService::CreateForTesting();

  auto context_params = mojom::NetworkContextParams::New();

  // Use a dummy CertVerifier that always passes cert verification, since
  // these unittests don't need to test CertVerifier behavior.
  context_params->cert_verifier_params =
      FakeTestCertVerifierParamsFactory::GetCertVerifierParams();

  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  context_params->initial_proxy_config =
      net::ProxyConfigWithAnnotation::CreateDirect();

  context_params->cors_exempt_header_list.push_back(kTestCorsExemptHeader);

  context_params->shared_dictionary_enabled = shared_dictionary_enabled;

  network_context_ = std::make_unique<NetworkContext>(
      network_service_.get(),
      network_context_remote_.BindNewPipeAndPassReceiver(),
      std::move(context_params));

  const url::Origin default_initiator_origin =
      url::Origin::Create(GURL("https://example.com"));
  ResetFactory(default_initiator_origin, kRendererProcessId);
}

CorsURLLoaderTestBase::~CorsURLLoaderTestBase() = default;

void CorsURLLoaderTestBase::CreateLoaderAndStart(
    const GURL& origin,
    const GURL& url,
    mojom::RequestMode mode,
    mojom::RedirectMode redirect_mode,
    mojom::CredentialsMode credentials_mode) {
  ResourceRequest request;
  request.mode = mode;
  request.redirect_mode = redirect_mode;
  request.credentials_mode = credentials_mode;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.url = url;
  if (request.mode == mojom::RequestMode::kNavigate)
    request.navigation_redirect_chain.push_back(url);
  request.request_initiator = url::Origin::Create(origin);
  request.devtools_request_id = "devtools";
  if (devtools_observer_for_next_request_) {
    request.trusted_params = ResourceRequest::TrustedParams();
    request.trusted_params->devtools_observer =
        devtools_observer_for_next_request_->Bind();
    devtools_observer_for_next_request_ = nullptr;
  }
  CreateLoaderAndStart(request);
}

void CorsURLLoaderTestBase::CreateLoaderAndStart(
    const ResourceRequest& request) {
  test_cors_loader_client_ = std::make_unique<TestURLLoaderClient>();
  url_loader_.reset();
  ResourceRequest request_copy(request);
  cors_url_loader_factory_->CreateLoaderAndStart(
      url_loader_.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      mojom::kURLLoadOptionNone, request_copy,
      test_cors_loader_client_->CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
}

void CorsURLLoaderTestBase::ClearHasReceivedRedirect() {
  test_cors_loader_client_->ClearHasReceivedRedirect();
}

void CorsURLLoaderTestBase::RunUntilCreateLoaderAndStartCalled() {
  DCHECK(test_url_loader_factory_);
  base::RunLoop run_loop;
  test_url_loader_factory_->SetOnCreateLoaderAndStart(run_loop.QuitClosure());
  run_loop.Run();
  test_url_loader_factory_->SetOnCreateLoaderAndStart({});
}

void CorsURLLoaderTestBase::RunUntilComplete() {
  test_cors_loader_client_->RunUntilComplete();
}

void CorsURLLoaderTestBase::RunUntilRedirectReceived() {
  test_cors_loader_client_->RunUntilRedirectReceived();
}

void CorsURLLoaderTestBase::AddAllowListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    const mojom::CorsDomainMatchMode mode) {
  origin_access_list_.AddAllowListEntryForOrigin(
      source_origin, protocol, domain, /*port=*/0, mode,
      mojom::CorsPortMatchMode::kAllowAnyPort,
      mojom::CorsOriginAccessMatchPriority::kDefaultPriority);
}

void CorsURLLoaderTestBase::AddBlockListEntryForOrigin(
    const url::Origin& source_origin,
    const std::string& protocol,
    const std::string& domain,
    const mojom::CorsDomainMatchMode mode) {
  origin_access_list_.AddBlockListEntryForOrigin(
      source_origin, protocol, domain, /*port=*/0, mode,
      mojom::CorsPortMatchMode::kAllowAnyPort,
      mojom::CorsOriginAccessMatchPriority::kHighPriority);
}

void CorsURLLoaderTestBase::ResetFactory(std::optional<url::Origin> initiator,
                                         uint32_t process_id,
                                         const ResetFactoryParams& params) {
  if (process_id != mojom::kBrowserProcessId)
    DCHECK(initiator.has_value());

  test_url_loader_factory_ = std::make_unique<TestURLLoaderFactory>();
  test_url_loader_factory_receiver_ =
      std::make_unique<mojo::Receiver<mojom::URLLoaderFactory>>(
          test_url_loader_factory_.get());

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  if (initiator) {
    factory_params->request_initiator_origin_lock = *initiator;
  }
  factory_params->is_trusted = params.is_trusted;
  factory_params->process_id = process_id;
  factory_params->is_orb_enabled = (process_id != mojom::kBrowserProcessId);
  factory_params->ignore_isolated_world_origin =
      params.ignore_isolated_world_origin;
  factory_params->factory_override = mojom::URLLoaderFactoryOverride::New();
  factory_params->factory_override->overriding_factory =
      test_url_loader_factory_receiver_->BindNewPipeAndPassRemote();
  factory_params->factory_override->skip_cors_enabled_scheme_check =
      params.skip_cors_enabled_scheme_check;
  factory_params->client_security_state = params.client_security_state.Clone();
  factory_params->isolation_info = params.isolation_info;
  factory_params->url_loader_network_observer = std::move(
      const_cast<mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>&>(
          params.url_loader_network_observer));

  auto resource_scheduler_client =
      base::MakeRefCounted<ResourceSchedulerClient>(
          ResourceScheduler::ClientId::Create(),
          IsBrowserInitiated(process_id == mojom::kBrowserProcessId),
          &resource_scheduler_,
          url_request_context_->network_quality_estimator());

  // Avoid the raw_ptr<> becoming dangling.
  cors_url_loader_factory_ = nullptr;
  cors_url_loader_factory_remote_.reset();
  factory_owner_ = std::make_unique<PrefetchMatchingURLLoaderFactory>(
      network_context_.get(), std::move(factory_params),
      resource_scheduler_client,
      cors_url_loader_factory_remote_.BindNewPipeAndPassReceiver(),
      &origin_access_list_, nullptr);
  cors_url_loader_factory_ =
      factory_owner_->GetCorsURLLoaderFactoryForTesting();
}

std::vector<net::NetLogEntry> CorsURLLoaderTestBase::GetEntries() const {
  std::vector<net::NetLogEntry> entries, filtered;
  entries = net_log_observer_.GetEntries();
  for (auto& entry : entries) {
    if (entry.type == net::NetLogEventType::CORS_REQUEST ||
        entry.type == net::NetLogEventType::CHECK_CORS_PREFLIGHT_REQUIRED ||
        entry.type == net::NetLogEventType::CHECK_CORS_PREFLIGHT_CACHE ||
        entry.type == net::NetLogEventType::CORS_PREFLIGHT_RESULT ||
        entry.type == net::NetLogEventType::CORS_PREFLIGHT_CACHED_RESULT ||
        entry.type == net::NetLogEventType::CORS_PREFLIGHT_ERROR) {
      filtered.push_back(std::move(entry));
    }
  }
  return filtered;
}

// static.
std::vector<net::NetLogEventType>
CorsURLLoaderTestBase::GetTypesOfNetLogEntries(
    const std::vector<net::NetLogEntry>& entries) {
  std::vector<net::NetLogEventType> types;
  for (const auto& entry : entries) {
    types.push_back(entry.type);
  }
  return types;
}

// static.
const net::NetLogEntry* CorsURLLoaderTestBase::FindEntryByType(
    const std::vector<net::NetLogEntry>& entries,
    net::NetLogEventType type) {
  for (const auto& entry : entries) {
    if (entry.type == type) {
      return &entry;
    }
  }
  return nullptr;
}

net::RedirectInfo CorsURLLoaderTestBase::CreateRedirectInfo(
    int status_code,
    std::string_view method,
    const GURL& url,
    std::string_view referrer,
    net::ReferrerPolicy referrer_policy,
    net::SiteForCookies site_for_cookies) {
  net::RedirectInfo redirect_info;
  redirect_info.status_code = status_code;
  redirect_info.new_method = std::string{method};
  redirect_info.new_url = url;
  redirect_info.new_referrer = std::string{referrer};
  redirect_info.new_referrer_policy = referrer_policy;
  redirect_info.new_site_for_cookies = site_for_cookies;
  return redirect_info;
}

}  // namespace network::cors
