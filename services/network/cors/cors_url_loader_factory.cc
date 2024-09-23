// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include <optional>
#include <utility>

#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/optional_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/network_service.h"
#include "services/network/prefetch_matching_url_loader_factory.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/url_loader.h"
#include "services/network/url_loader_factory.h"
#include "services/network/web_bundle/web_bundle_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network::cors {

namespace {

// Verifies state that should hold for Trust Tokens parameters provided by a
// functioning renderer:
// - Trust Tokens should be enabled
// - the request should come from a trustworthy context
// - It should be from a context where the underlying operations are permitted
// (as specified by URLLoaderFactoryParams::trust_token_redemption_policy and
// URLLoaderFactoryParams::trust_token_issuance_policy).
bool VerifyTrustTokenParamsIntegrityIfPresent(
    const ResourceRequest& resource_request,
    const NetworkContext* context,
    mojom::TrustTokenOperationPolicyVerdict trust_token_issuance_policy,
    mojom::TrustTokenOperationPolicyVerdict trust_token_redemption_policy) {
  if (!resource_request.trust_token_params) {
    return true;
  }

  if (!context->trust_token_store()) {
    // Got a request with Trust Tokens parameters with Trust tokens
    // disabled.
    //
    // Here and below, we use concise error messages to make them easier to
    // search search.
    mojo::ReportBadMessage(
        "TrustTokenParamsIntegrity: TrustTokensRequestWithTrustTokensDisabled");
    return false;
  }

  // TODO(crbug.com/40729410): There's no current way to get a trusted
  // browser-side view of whether a request "came from" a secure context, so we
  // don't implement a check for the second criterion in the function comment.

  switch (resource_request.trust_token_params->operation) {
    case network::mojom::TrustTokenOperationType::kRedemption:
    case network::mojom::TrustTokenOperationType::kSigning:
      if (trust_token_redemption_policy ==
          mojom::TrustTokenOperationPolicyVerdict::kForbid) {
        // Got a request configured for Trust Tokens redemption or signing from
        // a context in which this operation is prohibited.
        mojo::ReportBadMessage(
            "TrustTokenParamsIntegrity: RequestFromContextLackingPermission");
        return false;
      }
      break;
    case network::mojom::TrustTokenOperationType::kIssuance:
      if (trust_token_issuance_policy ==
          mojom::TrustTokenOperationPolicyVerdict::kForbid) {
        // Got a request configured for Trust Tokens issuance from
        // a context in which this operation is prohibited.
        mojo::ReportBadMessage(
            "TrustTokenParamsIntegrity: RequestFromContextLackingPermission");
        return false;
      }
      break;
  }

  return true;
}

base::debug::CrashKeyString* GetRequestInitiatorOriginLockCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "request_initiator_origin_lock", base::debug::CrashKeySize::Size256);
  return crash_key;
}

bool IsTrustedNavigationRequestFromSecureContext(
    const ResourceRequest& request) {
  if (!request.trusted_params) {
    return false;
  }
  if (request.mode != mojom::RequestMode::kNavigate) {
    return false;
  }

  // `client_security_state` is not set for top-level navigation requests.
  // TODO(crbug.com/40149351): Remove this when we set it for top-level
  // navigation requests.
  if (!request.trusted_params->client_security_state) {
    return IsUrlPotentiallyTrustworthy(request.url);
  }
  return request.trusted_params->client_security_state->is_web_secure_context;
}

}  // namespace

class CorsURLLoaderFactory::FactoryOverride final {
 public:
  class ExposedNetworkLoaderFactory final : public mojom::URLLoaderFactory {
   public:
    ExposedNetworkLoaderFactory(
        std::unique_ptr<network::URLLoaderFactory> network_loader_factory,
        mojo::PendingReceiver<mojom::URLLoaderFactory> receiver)
        : network_loader_factory_(std::move(network_loader_factory)) {
      if (receiver) {
        receivers_.Add(this, std::move(receiver));
      }
    }
    ~ExposedNetworkLoaderFactory() override = default;

    ExposedNetworkLoaderFactory(const ExposedNetworkLoaderFactory&) = delete;
    ExposedNetworkLoaderFactory& operator=(const ExposedNetworkLoaderFactory&) =
        delete;

    // mojom::URLLoaderFactory implementation
    void CreateLoaderAndStart(
        mojo::PendingReceiver<mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const ResourceRequest& request,
        mojo::PendingRemote<mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      return network_loader_factory_->CreateLoaderAndStart(
          std::move(receiver), request_id, options, request, std::move(client),
          traffic_annotation);
    }
    void Clone(
        mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
      receivers_.Add(this, std::move(receiver));
    }
    mojom::DevToolsObserver* GetDevToolsObserver() {
      return network_loader_factory_->GetDevToolsObserver();
    }

   private:
    std::unique_ptr<network::URLLoaderFactory> network_loader_factory_;
    mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
  };

  FactoryOverride(
      mojom::URLLoaderFactoryOverridePtr params,
      std::unique_ptr<network::URLLoaderFactory> network_loader_factory)
      : network_loader_factory_(std::move(network_loader_factory),
                                std::move(params->overridden_factory_receiver)),
        overriding_factory_(std::move(params->overriding_factory)),
        skip_cors_enabled_scheme_check_(
            params->skip_cors_enabled_scheme_check) {}

  FactoryOverride(const FactoryOverride&) = delete;
  FactoryOverride& operator=(const FactoryOverride&) = delete;

  mojom::URLLoaderFactory* get() { return overriding_factory_.get(); }

  bool ShouldSkipCorsEnabledSchemeCheck() {
    return skip_cors_enabled_scheme_check_;
  }

  mojom::DevToolsObserver* GetDevToolsObserver() {
    return network_loader_factory_.GetDevToolsObserver();
  }

 private:
  ExposedNetworkLoaderFactory network_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> overriding_factory_;
  bool skip_cors_enabled_scheme_check_;
};

bool CorsURLLoaderFactory::allow_external_preflights_for_testing_ = false;

CorsURLLoaderFactory::CorsURLLoaderFactory(
    NetworkContext* context,
    mojom::URLLoaderFactoryParamsPtr params,
    scoped_refptr<ResourceSchedulerClient> resource_scheduler_client,
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver,
    const OriginAccessList* origin_access_list,
    PrefetchMatchingURLLoaderFactory* owner)
    : context_(context),
      is_trusted_(params->is_trusted),
      disable_web_security_(params->disable_web_security),
      process_id_(params->process_id),
      request_initiator_origin_lock_(params->request_initiator_origin_lock),
      ignore_isolated_world_origin_(params->ignore_isolated_world_origin),
      trust_token_issuance_policy_(params->trust_token_issuance_policy),
      trust_token_redemption_policy_(params->trust_token_redemption_policy),
      isolation_info_(params->isolation_info),
      automatically_assign_isolation_info_(
          params->automatically_assign_isolation_info),
      debug_tag_(params->debug_tag),
      cross_origin_embedder_policy_(
          params->client_security_state
              ? params->client_security_state->cross_origin_embedder_policy
              : CrossOriginEmbedderPolicy()),
      coep_reporter_(std::move(params->coep_reporter)),
      client_security_state_(params->client_security_state.Clone()),
      url_loader_network_service_observer_(
          std::move(params->url_loader_network_observer)),
      shared_dictionary_observer_(
          std::move(params->shared_dictionary_observer)),
      require_cross_site_request_for_cookies_(
          params->require_cross_site_request_for_cookies),
      factory_cookie_setting_overrides_(params->cookie_setting_overrides),
      origin_access_list_(origin_access_list),
      owner_(owner) {
  TRACE_EVENT("loading", "CorsURLLoaderFactory::CorsURLLoaderFactory",
              perfetto::Flow::FromPointer(this));
  DCHECK(context_);
  DCHECK(origin_access_list_);
  DCHECK_NE(mojom::kInvalidProcessId, process_id_);
  DCHECK_EQ(net::IsolationInfo::RequestType::kOther,
            params->isolation_info.request_type());
  if (params->automatically_assign_isolation_info) {
    DCHECK(params->isolation_info.IsEmpty());
    // Only the browser process is currently permitted to use automatically
    // assigned IsolationInfo, to prevent cross-site information leaks.
    DCHECK_EQ(mojom::kBrowserProcessId, process_id_);
  }

  if (context_->GetSharedDictionaryManager() && client_security_state_ &&
      client_security_state_->is_web_secure_context) {
    std::optional<net::SharedDictionaryIsolationKey> isolation_key =
        net::SharedDictionaryIsolationKey::MaybeCreate(params->isolation_info);
    if (isolation_key) {
      shared_dictionary_storage_ =
          context_->GetSharedDictionaryManager()->GetStorage(*isolation_key);
    }
  }

  auto factory_override = std::move(params->factory_override);
  auto network_loader_factory = std::make_unique<network::URLLoaderFactory>(
      context, std::move(params), std::move(resource_scheduler_client), this);

  if (factory_override) {
    DCHECK(factory_override->overriding_factory);
    factory_override_ = std::make_unique<FactoryOverride>(
        std::move(factory_override), std::move(network_loader_factory));
  } else {
    network_loader_factory_ = std::move(network_loader_factory);
  }

  if (receiver.is_valid()) {
    receivers_.Add(this, std::move(receiver));
  }
  receivers_.set_disconnect_handler(base::BindRepeating(
      &CorsURLLoaderFactory::DeleteIfNeeded, base::Unretained(this)));
}

CorsURLLoaderFactory::~CorsURLLoaderFactory() {
  // Delete loaders one at a time, since deleting one loader can cause another
  // loader waiting on it to fail synchronously, which could result in the other
  // loader calling DestroyURLLoader().
  while (!url_loaders_.empty()) {
    // No need to call context_->LoaderDestroyed(), since this method is only
    // called from the NetworkContext's destructor, or when there are no
    // remaining URLLoaders.
    url_loaders_.erase(url_loaders_.begin());
  }

  // Same as above for CorsURLLoaders.
  while (!cors_url_loaders_.empty()) {
    cors_url_loaders_.erase(cors_url_loaders_.begin());
  }
}

// This function is only used as an export for URLLoaderNetworkServiceObserver
// gained from URLLoaderFactoryParams, which might be invalid in a few cases.
// Please call URLLoaderFactory::GetURLLoaderNetworkServiceObserver() instead.
mojom::URLLoaderNetworkServiceObserver*
CorsURLLoaderFactory::url_loader_network_service_observer() const {
  if (url_loader_network_service_observer_) {
    return url_loader_network_service_observer_.get();
  }
  return nullptr;
}

void CorsURLLoaderFactory::OnURLLoaderCreated(
    std::unique_ptr<URLLoader> loader) {
  OnLoaderCreated(std::move(loader), url_loaders_);
}

void CorsURLLoaderFactory::OnCorsURLLoaderCreated(
    std::unique_ptr<CorsURLLoader> loader) {
  OnLoaderCreated(std::move(loader), cors_url_loaders_);
}

void CorsURLLoaderFactory::DestroyURLLoader(URLLoader* loader) {
  DestroyLoader(loader, url_loaders_);
}

void CorsURLLoaderFactory::DestroyCorsURLLoader(CorsURLLoader* loader) {
  DestroyLoader(loader, cors_url_loaders_);
}

void CorsURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  NOTREACHED() << "Non-const ref version of this method should be used as a "
                  "performance optimization.";
}

void CorsURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    ResourceRequest& resource_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  TRACE_EVENT("loading", "CorsURLLoaderFactory::CreateLoaderAndStart",
              perfetto::Flow::FromPointer(this));

  std::optional<base::ElapsedTimer> timer;
  std::optional<base::ElapsedThreadTimer> thread_timer;

  if (metrics_subsampler_.ShouldSample(0.001)) {
    timer.emplace();
    if (base::ThreadTicks::IsSupported()) {
      thread_timer.emplace();
    }
  }

  debug::ScopedResourceRequestCrashKeys request_crash_keys(resource_request);
  SCOPED_CRASH_KEY_NUMBER("net", "traffic_annotation_hash",
                          traffic_annotation.unique_id_hash_code);
  SCOPED_CRASH_KEY_STRING64("network", "factory_debug_tag", debug_tag_);

  if (!IsValidRequest(resource_request, options)) {
    mojo::Remote<mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (resource_request.destination ==
      network::mojom::RequestDestination::kWebBundle) {
    DCHECK(resource_request.web_bundle_token_params.has_value());

    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer;
    if (resource_request.devtools_request_id.has_value()) {
      devtools_observer = GetDevToolsObserver(resource_request);
    }

    base::WeakPtr<WebBundleURLLoaderFactory> web_bundle_url_loader_factory =
        context_->GetWebBundleManager().CreateWebBundleURLLoaderFactory(
            resource_request.url, *resource_request.web_bundle_token_params,
            process_id_, std::move(devtools_observer),
            resource_request.devtools_request_id, cross_origin_embedder_policy_,
            coep_reporter());
    client = web_bundle_url_loader_factory->MaybeWrapURLLoaderClient(
        std::move(client));
    if (!client) {
      return;
    }
  }

  mojom::URLLoaderFactory* const inner_url_loader_factory =
      factory_override_ ? factory_override_->get()
                        : network_loader_factory_.get();
  DCHECK(inner_url_loader_factory);

  const net::IsolationInfo* isolation_info_ptr = &isolation_info_;
  auto isolation_info = URLLoader::GetIsolationInfo(
      isolation_info_, automatically_assign_isolation_info_, resource_request);
  if (isolation_info.has_value()) {
    isolation_info_ptr = &isolation_info.value();
  }

  // Check if the initiator's network access has been revoked.
  // This check is only relevant if there is a partition nonce in the
  // isolation info. (All requests originating from a fenced frame have a
  // nonce specified.)
  if (isolation_info.has_value() && isolation_info->nonce().has_value() &&
      !context_->IsNetworkForNonceAndUrlAllowed(*isolation_info->nonce(),
                                                resource_request.url)) {
    mojo::Remote<mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(
            URLLoaderCompletionStatus(net::ERR_NETWORK_ACCESS_REVOKED));
    return;
  }

  if (!disable_web_security_) {
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer;
    const bool always_clone = !base::FeatureList::IsEnabled(
        network::features::kCloneDevToolsConnectionOnlyIfRequested);
    if (always_clone || resource_request.devtools_request_id.has_value()) {
      devtools_observer = GetDevToolsObserver(resource_request);
    }

    scoped_refptr<SharedDictionaryStorage> shared_dictionary_storage =
        shared_dictionary_storage_;
    if (context_->GetSharedDictionaryManager() &&
        IsTrustedNavigationRequestFromSecureContext(resource_request)) {
      // For trusted navigation requests, we need to get a storage using
      // `isolation_info_ptr`.
      std::optional<net::SharedDictionaryIsolationKey> isolation_key =
          net::SharedDictionaryIsolationKey::MaybeCreate(*isolation_info_ptr);
      if (isolation_key) {
        shared_dictionary_storage =
            context_->GetSharedDictionaryManager()->GetStorage(*isolation_key);
      }
    }

    std::unique_ptr<CorsURLLoader> loader;
    if (base::FeatureList::IsEnabled(
            network::features::kAvoidResourceRequestCopies)) {
      loader = std::make_unique<CorsURLLoader>(
          std::move(receiver), process_id_, request_id, options,
          base::BindOnce(&CorsURLLoaderFactory::DestroyCorsURLLoader,
                         base::Unretained(this)),
          std::move(resource_request), ignore_isolated_world_origin_,
          factory_override_ &&
              factory_override_->ShouldSkipCorsEnabledSchemeCheck(),
          std::move(client), traffic_annotation, inner_url_loader_factory,
          factory_override_ ? nullptr : network_loader_factory_.get(),
          origin_access_list_, GetAllowAnyCorsExemptHeaderForBrowser(),
          HasFactoryOverride(!!factory_override_), *isolation_info_ptr,
          std::move(devtools_observer), client_security_state_.get(),
          &url_loader_network_service_observer_, cross_origin_embedder_policy_,
          shared_dictionary_storage,
          shared_dictionary_observer_ ? shared_dictionary_observer_.get()
                                      : nullptr,
          context_, factory_cookie_setting_overrides_);
    } else {
      loader = std::make_unique<CorsURLLoader>(
          std::move(receiver), process_id_, request_id, options,
          base::BindOnce(&CorsURLLoaderFactory::DestroyCorsURLLoader,
                         base::Unretained(this)),
          resource_request, ignore_isolated_world_origin_,
          factory_override_ &&
              factory_override_->ShouldSkipCorsEnabledSchemeCheck(),
          std::move(client), traffic_annotation, inner_url_loader_factory,
          factory_override_ ? nullptr : network_loader_factory_.get(),
          origin_access_list_, GetAllowAnyCorsExemptHeaderForBrowser(),
          HasFactoryOverride(!!factory_override_), *isolation_info_ptr,
          std::move(devtools_observer), client_security_state_.get(),
          &url_loader_network_service_observer_, cross_origin_embedder_policy_,
          shared_dictionary_storage,
          shared_dictionary_observer_ ? shared_dictionary_observer_.get()
                                      : nullptr,
          context_, factory_cookie_setting_overrides_);
    }
    auto* raw_loader = loader.get();
    OnCorsURLLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    inner_url_loader_factory->CreateLoaderAndStart(
        std::move(receiver), request_id, options, resource_request,
        std::move(client), traffic_annotation);
  }

  if (timer.has_value()) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "NetworkService.CorsURLLoaderFactory.CreateLoaderAndStart2.Duration",
        timer->Elapsed(), base::Microseconds(1), base::Milliseconds(16), 100);
  }
  if (thread_timer.has_value()) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "NetworkService.CorsURLLoaderFactory.CreateLoaderAndStart2."
        "ThreadDuration",
        thread_timer->Elapsed(), base::Microseconds(1), base::Milliseconds(16),
        100);
  }
}

void CorsURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) {
  // The cloned factories stop working when this factory is destructed.
  receivers_.Add(this, std::move(receiver));
}

void CorsURLLoaderFactory::ClearBindings() {
  receivers_.Clear();
  DeleteIfNeeded();
}

void CorsURLLoaderFactory::DeleteIfNeeded() {
  if (receivers_.empty() && url_loaders_.empty() && cors_url_loaders_.empty() &&
      !owner_->HasAdditionalReferences()) {
    owner_->DestroyURLLoaderFactory(this);
  }
}

// static
bool CorsURLLoaderFactory::IsValidCorsExemptHeaders(
    const base::flat_set<std::string>& allowed_exempt_headers,
    const net::HttpRequestHeaders& headers) {
  for (const auto& header : headers.GetHeaderVector()) {
    if (allowed_exempt_headers.find(header.key) !=
        allowed_exempt_headers.end()) {
      continue;
    }
    LOG(WARNING) << "`cors_exempt_headers` contains unexpected key: "
                 << header.value;
    return false;
  }
  return true;
}

bool CorsURLLoaderFactory::IsValidRequest(const ResourceRequest& request,
                                          uint32_t options) {
  if (request.url.SchemeIs(url::kDataScheme)) {
    LOG(WARNING) << "CorsURLLoaderFactory doesn't support `data` scheme.";
    mojo::ReportBadMessage("CorsURLLoaderFactory: data: URL is not supported.");
    return false;
  }

  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS cannot work.
  if (!request.request_initiator &&
      request.mode != network::mojom::RequestMode::kNavigate &&
      request.mode != mojom::RequestMode::kNoCors) {
    LOG(WARNING) << "`mode` is " << request.mode
                 << ", but `request_initiator` is not set.";
    mojo::ReportBadMessage("CorsURLLoaderFactory: cors without initiator");
    return false;
  }

  // Reject request with trusted params if factory is not for a trusted
  // consumer.
  if (request.trusted_params && !is_trusted_) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: Untrusted caller making trusted request");
    return false;
  }

  // Reject request if the restricted prefetch load flag is set but the
  // request's IsolationInfo is not present. This is because the restricted
  // prefetch flag is only used when the browser sets the request's
  // IsolationInfo to correctly cache-partition the resource.
  bool request_network_isolation_info_present =
      request.trusted_params &&
      !request.trusted_params->isolation_info.IsEmpty();
  if (request.load_flags & net::LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME &&
      !request_network_isolation_info_present) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: Request with "
        "LOAD_RESTRICTED_PREFETCH_FOR_MAIN_FRAME flag is "
        "not trusted");
    return false;
  }

  // The `force_main_frame_for_same_site_cookies` should only be set when a
  // service worker passes through a navigation request.  In this case the
  // mode must be `kNavigate` and the destination must be empty.
  if (request.original_destination == mojom::RequestDestination::kDocument &&
      (request.mode != mojom::RequestMode::kNavigate ||
       request.destination != mojom::RequestDestination::kEmpty)) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: original_destination is unexpectedly set to "
        "kDocument");
    return false;
  }

  // Validate that a navigation redirect chain is not sent for a non-navigation
  // request.
  if (!request.navigation_redirect_chain.empty() &&
      request.mode != mojom::RequestMode::kNavigate) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: navigation redirect chain set for a "
        "non-navigation");
    return false;
  }

  // Validate that `require_cross_site_request_for_cookies_` is respected by an
  // empty SiteForCookies (indicating a cross-site request being made).
  if (require_cross_site_request_for_cookies_ &&
      !request.site_for_cookies.IsNull()) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: all requests in this context must be "
        "cross-site");
    return false;
  }

  // By default we compare the `request_initiator` to the lock below.  This is
  // overridden for renderer navigations, however.
  std::optional<url::Origin> origin_to_validate = request.request_initiator;

  // Ensure that renderer requests are covered either by CORS or ORB.
  if (process_id_ != mojom::kBrowserProcessId) {
    switch (request.mode) {
      case mojom::RequestMode::kNavigate:
        // A navigation request from a renderer can legally occur when a service
        // worker passes it through from its `FetchEvent.request` to `fetch()`.
        // In this case it is making a navigation request on behalf of the
        // original initiator.  Since that initiator may be cross-origin, its
        // possible the request's initiator will not match our lock.
        //
        // To make this operation safe we instead compare the request URL origin
        // against the initiator lock.  We can do this since service workers
        // should only ever handle same-origin navigations.
        //
        // With this approach its possible the initiator could be spoofed by the
        // renderer.  However, since we have validated the request URL they can
        // only every lie to the origin that they have already compromised.  It
        // does not allow an attacker to target other arbitrary origins.
        origin_to_validate = url::Origin::Create(request.url);

        // We further validate the navigation request by ensuring it has the
        // correct redirect mode.  This avoids an attacker attempting to
        // craft a navigation that is then automatically followed to a separate
        // target origin.  With manual mode the redirect will instead be
        // processed as an opaque redirect response that is passed back to the
        // renderer and navigation code.  The redirected requested must be
        // sent anew and go through this validation again.
        if (request.redirect_mode != mojom::RedirectMode::kManual) {
          mojo::ReportBadMessage(
              "CorsURLLoaderFactory: navigate from non-browser-process with "
              "redirect_mode set to 'follow'");
          return false;
        }

        // Validate that a navigation redirect chain is always provided for a
        // navigation request.
        if (request.navigation_redirect_chain.empty()) {
          mojo::ReportBadMessage(
              "CorsURLLoaderFactory: navigate from non-browser-process without "
              "a redirect chain provided");
          return false;
        }

        break;

      case mojom::RequestMode::kSameOrigin:
      case mojom::RequestMode::kCors:
      case mojom::RequestMode::kCorsWithForcedPreflight:
        // SOP enforced by CORS.
        break;

      case mojom::RequestMode::kNoCors:
        // SOP enforced by ORB.
        break;
    }
  }

  // Depending on the type of request, compare either `request_initiator` or
  // `request.url` to `request_initiator_origin_lock_`.
  InitiatorLockCompatibility initiator_lock_compatibility;
  if (process_id_ == mojom::kBrowserProcessId) {
    initiator_lock_compatibility = InitiatorLockCompatibility::kBrowserProcess;
  } else {
    initiator_lock_compatibility = VerifyRequestInitiatorLock(
        request_initiator_origin_lock_, origin_to_validate);
  }
  switch (initiator_lock_compatibility) {
    case InitiatorLockCompatibility::kCompatibleLock:
    case InitiatorLockCompatibility::kBrowserProcess:
      break;

    case InitiatorLockCompatibility::kNoLock:
      // `request_initiator_origin_lock` should always be set in a
      // URLLoaderFactory vended to a renderer process.  See also
      // https://crbug.com/1114906.
      NOTREACHED_IN_MIGRATION();
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator lock in a renderer request");
      return false;

    case InitiatorLockCompatibility::kNoInitiator:
      // Requests from the renderer need to always specify an initiator.
      NOTREACHED_IN_MIGRATION();
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator in a renderer request");
      return false;

    case InitiatorLockCompatibility::kIncorrectLock:
      // Requests from the renderer need to always specify a correct initiator.
      url::debug::ScopedOriginCrashKey initiator_origin_lock_crash_key(
          GetRequestInitiatorOriginLockCrashKey(),
          base::OptionalToPtr(request_initiator_origin_lock_));
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: lock VS initiator mismatch");
      return false;
  }

  if (!GetAllowAnyCorsExemptHeaderForBrowser() &&
      !IsValidCorsExemptHeaders(*context_->cors_exempt_header_list(),
                                request.cors_exempt_headers)) {
    return false;
  }

  if (!AreRequestHeadersSafe(request.headers) ||
      !AreRequestHeadersSafe(request.cors_exempt_headers)) {
    return false;
  }

  // Specifying CredentialsMode::kSameOrigin without an initiator origin doesn't
  // make sense.
  if (request.credentials_mode == mojom::CredentialsMode::kSameOrigin &&
      !request.request_initiator) {
    LOG(WARNING) << "same-origin credentials mode without initiator";
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: same-origin credentials mode without initiator");
    return false;
  }

  // We only support `kInclude` credentials mode with navigations. See also:
  // a note at https://fetch.spec.whatwg.org/#concept-request-credentials-mode.
  if (request.credentials_mode != mojom::CredentialsMode::kInclude &&
      request.mode == network::mojom::RequestMode::kNavigate) {
    LOG(WARNING) << "unsupported credentials mode on a navigation request";
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: unsupported credentials mode on navigation");
    return false;
  }

  if (!allow_external_preflights_for_testing_) {
    // kURLLoadOptionAsCorsPreflight should be set only by the network service.
    // Otherwise the network service will be confused.
    if (options & mojom::kURLLoadOptionAsCorsPreflight) {
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: kURLLoadOptionAsCorsPreflight is set");
      return false;
    }
  }

  if (!VerifyTrustTokenParamsIntegrityIfPresent(
          request, context_, trust_token_issuance_policy_,
          trust_token_redemption_policy_)) {
    // VerifyTrustTokenParamsIntegrityIfPresent will report an appropriate bad
    // message.
    return false;
  }

  if (!net::HttpUtil::IsToken(request.method)) {
    // Callers are expected to ensure that `method` follows RFC 7230.
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: invalid characters in method");
    return false;
  }

  // Don't allow forbidden methods for any requests except RequestMode::kNoCors.
  // Don't allow CONNECT method for any request.
  if ((request.mode != mojom::RequestMode::kNoCors &&
       cors::IsForbiddenMethod(request.method)) ||
      (request.mode == mojom::RequestMode::kNoCors &&
       base::EqualsCaseInsensitiveASCII(
           request.method, net::HttpRequestHeaders::kConnectMethod))) {
    mojo::ReportBadMessage("CorsURLLoaderFactory: Forbidden method");
    return false;
  }

  // Check only when `disable_web_security_` is false because
  // PreflightControllerTest use CorsURLLoaderFactory with this flag true
  // instead of network::URLLoaderFactory.
  // TODO(crbug.com/40203308): consider if we can remove this exemption.
  if (!disable_web_security_) {
    // `net_log_create_info` field is expected to be used within network
    // service.
    if (request.net_log_create_info && !is_trusted_) {
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: net_log_create_info field is not expected.");
      return false;
    }

    // `net_log_reference_info` field is expected to be used within network
    // service.
    if (request.net_log_reference_info) {
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: net_log_reference_info field is not "
          "expected.");
      return false;
    }

    if (client_security_state_ &&
        PrivateNetworkAccessChecker::NeedPermission(
            request.url, client_security_state_->is_web_secure_context,
            request.required_ip_address_space)) {
      if (request.required_ip_address_space == mojom::IPAddressSpace::kPublic) {
        mojo::ReportBadMessage(
            "CorsURLLoaderFactory: required_ip_address_space "
            "is set to public.");
        return false;
      }
    } else if (request.target_ip_address_space !=
               mojom::IPAddressSpace::kUnknown) {
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: target_ip_address_space is "
          "set.");
      return false;
    }
  }

  // TODO(yhirano): If the request mode is "no-cors", the redirect mode should
  // be "follow".
  return true;
}

bool CorsURLLoaderFactory::GetAllowAnyCorsExemptHeaderForBrowser() const {
  return process_id_ == mojom::kBrowserProcessId &&
         context_->allow_any_cors_exempt_header_for_browser();
}

mojo::PendingRemote<mojom::DevToolsObserver>
CorsURLLoaderFactory::GetDevToolsObserver(
    ResourceRequest& resource_request) const {
  TRACE_EVENT("loading", "CorsURLLoaderFactory::GetDevToolsObserver");
  mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer;
  if (resource_request.trusted_params &&
      resource_request.trusted_params->devtools_observer) {
    if (base::FeatureList::IsEnabled(features::kAvoidResourceRequestCopies)) {
      auto& original_observer =
          resource_request.trusted_params->devtools_observer;
      mojo::Remote<mojom::DevToolsObserver> remote(
          std::move(original_observer));
      remote->Clone(devtools_observer.InitWithNewPipeAndPassReceiver());
      original_observer = remote.Unbind();
    } else {
      ResourceRequest::TrustedParams cloned_params =
          *resource_request.trusted_params;
      devtools_observer = std::move(cloned_params.devtools_observer);
    }
  } else {
    mojom::DevToolsObserver* observer =
        factory_override_ ? factory_override_->GetDevToolsObserver()
                          : network_loader_factory_->GetDevToolsObserver();
    if (observer) {
      observer->Clone(devtools_observer.InitWithNewPipeAndPassReceiver());
    }
  }
  return devtools_observer;
}

mojom::SharedDictionaryAccessObserver*
CorsURLLoaderFactory::GetSharedDictionaryAccessObserver() const {
  return shared_dictionary_observer_ ? shared_dictionary_observer_.get()
                                     : nullptr;
}

net::handles::NetworkHandle CorsURLLoaderFactory::GetBoundNetworkForTesting()
    const {
  CHECK(!factory_override_);
  return network_loader_factory_->GetBoundNetworkForTesting();  // IN-TEST
}

void CorsURLLoaderFactory::CancelRequestsIfNonceMatchesAndUrlNotExempted(
    const base::UnguessableToken& nonce,
    const std::set<GURL>& exemptions) {
  auto iterate_over_set = [&nonce, &exemptions](auto& url_loaders) {
    // Cancelling the request may cause the URL loader to be deleted from the
    // data structure, invalidating the iterator if it is currently pointing to
    // that element. So advance to the next element first and delete the
    // previous one.
    for (auto loader_it = url_loaders.begin(); loader_it != url_loaders.end();
         /* iteration performed inside the loop */) {
      auto* loader = loader_it->get();
      ++loader_it;
      loader->CancelRequestIfNonceMatchesAndUrlNotExempted(nonce, exemptions);
    }
  };

  iterate_over_set(url_loaders_);
  iterate_over_set(cors_url_loaders_);
}

}  // namespace network::cors
