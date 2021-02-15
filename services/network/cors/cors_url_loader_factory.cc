// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/types/strong_alias.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/http/http_util.h"
#include "services/network/cors/cors_url_loader.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/crash_keys.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cross_origin_read_blocking.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/initiator_lock_compatibility.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trust_token_operation_authorization.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler_client.h"
#include "services/network/url_loader.h"
#include "services/network/url_loader_factory.h"
#include "services/network/web_bundle_url_loader_factory.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

using IsConsistent = ::base::StrongAlias<class IsConsistentTag, bool>;

// Record, for requests with associated Trust Tokens operations of operation
// types requiring initiators to have the Trust Tokens Feature Policy feature
// enabled, whether the browser process thinks it's possible for the initiating
// frame to have the feature enabled. (If the answer is "no," it indicates
// a misbehaving renderer in theory but, unfortunately, is more likely a false
// positive due to inconsistent state; see crbug.com/1117458.)
void HistogramWhetherTrustTokenFeaturePolicyConsistentWithBrowserOpinion(
    IsConsistent is_consistent) {
  UMA_HISTOGRAM_BOOLEAN(
      "Net.TrustTokens.SubresourceOperationRequiringFeaturePolicy."
      "PolicyIsConsistentWithBrowserOpinion",
      is_consistent.value());
}

// Verifies state that should hold for Trust Tokens parameters provided by a
// functioning renderer:
// - Trust Tokens should be enabled
// - the request should come from a trustworthy context
// - if the request is for redemption or signing, it should be from a context
// where these operations are permitted (as specified by
// URLLoaderFactoryParams::trust_token_redemption_policy).
bool VerifyTrustTokenParamsIntegrityIfPresent(
    const ResourceRequest& url_request,
    const NetworkContext* context,
    mojom::TrustTokenRedemptionPolicy trust_token_redemption_policy) {
  if (!url_request.trust_token_params)
    return true;

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

  if (trust_token_redemption_policy ==
          mojom::TrustTokenRedemptionPolicy::kForbid &&
      DoesTrustTokenOperationRequireFeaturePolicy(
          url_request.trust_token_params->type)) {
    // Got a request configured for Trust Tokens redemption or signing from
    // a context in which this operation is prohibited.
    //
    // TODO(crbug.com/1118183): Re-add a ReportBadMessage (and false return)
    // once the false positives in crbug.com/1117458 have been resolved.
    base::debug::DumpWithoutCrashing();
    HistogramWhetherTrustTokenFeaturePolicyConsistentWithBrowserOpinion(
        IsConsistent(false));
  } else if (DoesTrustTokenOperationRequireFeaturePolicy(
                 url_request.trust_token_params->type)) {
    HistogramWhetherTrustTokenFeaturePolicyConsistentWithBrowserOpinion(
        IsConsistent(true));
  }

  return true;
}

}  // namespace

class CorsURLLoaderFactory::FactoryOverride final {
 public:
  class ExposedNetworkLoaderFactory final : public mojom::URLLoaderFactory {
   public:
    ExposedNetworkLoaderFactory(
        std::unique_ptr<URLLoaderFactory> network_loader_factory,
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
        int32_t routing_id,
        int32_t request_id,
        uint32_t options,
        const ResourceRequest& request,
        mojo::PendingRemote<mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      return network_loader_factory_->CreateLoaderAndStart(
          std::move(receiver), routing_id, request_id, options, request,
          std::move(client), traffic_annotation);
    }
    void Clone(
        mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override {
      receivers_.Add(this, std::move(receiver));
    }

   private:
    std::unique_ptr<URLLoaderFactory> network_loader_factory_;
    mojo::ReceiverSet<mojom::URLLoaderFactory> receivers_;
  };

  FactoryOverride(mojom::URLLoaderFactoryOverridePtr params,
                  std::unique_ptr<URLLoaderFactory> network_loader_factory)
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
    const OriginAccessList* origin_access_list)
    : context_(context),
      is_trusted_(params->is_trusted),
      disable_web_security_(params->disable_web_security),
      process_id_(params->process_id),
      request_initiator_origin_lock_(params->request_initiator_origin_lock),
      ignore_isolated_world_origin_(params->ignore_isolated_world_origin),
      trust_token_redemption_policy_(params->trust_token_redemption_policy),
      isolation_info_(params->isolation_info),
      debug_tag_(params->debug_tag),
      origin_access_list_(origin_access_list) {
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

  receivers_.Add(this, std::move(receiver));
  receivers_.set_disconnect_handler(base::BindRepeating(
      &CorsURLLoaderFactory::DeleteIfNeeded, base::Unretained(this)));
}

CorsURLLoaderFactory::~CorsURLLoaderFactory() = default;

void CorsURLLoaderFactory::OnLoaderCreated(
    std::unique_ptr<mojom::URLLoader> loader) {
  if (context_)
    context_->LoaderCreated(process_id_);
  loaders_.insert(std::move(loader));
}

void CorsURLLoaderFactory::DestroyURLLoader(mojom::URLLoader* loader) {
  if (context_)
    context_->LoaderDestroyed(process_id_);
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);

  DeleteIfNeeded();
}

void CorsURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<mojom::URLLoader> receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const ResourceRequest& resource_request,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  debug::ScopedRequestCrashKeys request_crash_keys(resource_request);

  if (!IsValidRequest(resource_request, options)) {
    mojo::Remote<mojom::URLLoaderClient>(std::move(client))
        ->OnComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  if (resource_request.destination ==
      network::mojom::RequestDestination::kWebBundle) {
    DCHECK(resource_request.web_bundle_token_params.has_value());
    base::WeakPtr<WebBundleURLLoaderFactory> web_bundle_url_loader_factory =
        context_->GetWebBundleManager().CreateWebBundleURLLoaderFactory(
            resource_request.url, *resource_request.web_bundle_token_params,
            process_id_, request_initiator_origin_lock_);
    client =
        web_bundle_url_loader_factory->WrapURLLoaderClient(std::move(client));
  }

  mojom::URLLoaderFactory* const inner_url_loader_factory =
      factory_override_ ? factory_override_->get()
                        : network_loader_factory_.get();
  DCHECK(inner_url_loader_factory);
  if (!disable_web_security_) {
    auto loader = std::make_unique<CorsURLLoader>(
        std::move(receiver), process_id_, routing_id, request_id, options,
        base::BindOnce(&CorsURLLoaderFactory::DestroyURLLoader,
                       base::Unretained(this)),
        resource_request, ignore_isolated_world_origin_,
        factory_override_ &&
            factory_override_->ShouldSkipCorsEnabledSchemeCheck(),
        std::move(client), traffic_annotation, inner_url_loader_factory,
        origin_access_list_, context_->cors_preflight_controller(),
        context_->cors_exempt_header_list(),
        GetAllowAnyCorsExemptHeaderForBrowser(), isolation_info_);
    auto* raw_loader = loader.get();
    OnLoaderCreated(std::move(loader));
    raw_loader->Start();
  } else {
    inner_url_loader_factory->CreateLoaderAndStart(
        std::move(receiver), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation);
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
  if (!context_)
    return;
  if (receivers_.empty() && loaders_.empty())
    context_->DestroyURLLoaderFactory(this);
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
    LOG(WARNING) << "|cors_exempt_headers| contains unexpected key: "
                 << header.value;
    return false;
  }
  return true;
}

bool CorsURLLoaderFactory::IsValidRequest(const ResourceRequest& request,
                                          uint32_t options) {
  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS cannot work.
  if (!request.request_initiator &&
      request.mode != network::mojom::RequestMode::kNavigate &&
      request.mode != mojom::RequestMode::kNoCors) {
    LOG(WARNING) << "|mode| is " << request.mode
                 << ", but |request_initiator| is not set.";
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

  if (request.obey_origin_policy &&
      (!request.trusted_params ||
       request.trusted_params->isolation_info.IsEmpty())) {
    mojo::ReportBadMessage(
        "Origin policy only allowed on trusted requests with IsolationInfo");
    return false;
  }

  // Reject request if the restricted prefetch load flag is set but the
  // request's NetworkIsolationKey is not present. This is because the
  // restricted prefetch flag is only used when the browser sets the request's
  // NetworkIsolationKey to correctly cache-partition the resource.
  bool request_network_isolation_key_present =
      request.trusted_params &&
      !request.trusted_params->isolation_info.IsEmpty();
  if (request.load_flags & net::LOAD_RESTRICTED_PREFETCH &&
      !request_network_isolation_key_present) {
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: Request with LOAD_RESTRICTED_PREFETCH flag is "
        "not trusted");
    return false;
  }

  // Ensure that renderer requests are covered either by CORS or CORB.
  if (process_id_ != mojom::kBrowserProcessId) {
    switch (request.mode) {
      case mojom::RequestMode::kNavigate:
        // Only the browser process can initiate navigations.  This helps ensure
        // that a malicious/compromised renderer cannot bypass CORB by issuing
        // kNavigate, rather than kNoCors requests.  (CORB should apply only to
        // no-cors requests as tracked in https://crbug.com/953315 and as
        // captured in https://fetch.spec.whatwg.org/#main-fetch).
        mojo::ReportBadMessage(
            "CorsURLLoaderFactory: navigate from non-browser-process");
        return false;

      case mojom::RequestMode::kSameOrigin:
      case mojom::RequestMode::kCors:
      case mojom::RequestMode::kCorsWithForcedPreflight:
        // SOP enforced by CORS.
        break;

      case mojom::RequestMode::kNoCors:
        // SOP enforced by CORB.
        break;
    }
  }

  // Compare |request_initiator| and |request_initiator_origin_lock_|.
  InitiatorLockCompatibility initiator_lock_compatibility =
      VerifyRequestInitiatorLockWithPluginCheck(process_id_,
                                                request_initiator_origin_lock_,
                                                request.request_initiator);
  UMA_HISTOGRAM_ENUMERATION(
      "NetworkService.URLLoader.RequestInitiatorOriginLockCompatibility",
      initiator_lock_compatibility);
  switch (initiator_lock_compatibility) {
    case InitiatorLockCompatibility::kCompatibleLock:
    case InitiatorLockCompatibility::kBrowserProcess:
    case InitiatorLockCompatibility::kAllowedRequestInitiatorForPlugin:
      break;

    case InitiatorLockCompatibility::kNoLock:
      // |request_initiator_origin_lock| should always be set in a
      // URLLoaderFactory vended to a renderer process.  See also
      // https://crbug.com/1114906.
      NOTREACHED();
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator lock in a renderer request");
      return false;

    case InitiatorLockCompatibility::kNoInitiator:
      // Requests from the renderer need to always specify an initiator.
      NOTREACHED();
      mojo::ReportBadMessage(
          "CorsURLLoaderFactory: no initiator in a renderer request");
      return false;

    case InitiatorLockCompatibility::kIncorrectLock:
      // Requests from the renderer need to always specify a correct initiator.
      NOTREACHED() << "request_initiator_origin_lock_ = "
                   << request_initiator_origin_lock_.value_or(
                          url::Origin::Create(GURL("https://no-lock.com")))
                   << "; request.request_initiator = "
                   << request.request_initiator.value_or(url::Origin::Create(
                          GURL("https://no-initiator.com")));
      if (base::FeatureList::IsEnabled(
              features::kRequestInitiatorSiteLockEnfocement)) {
        url::debug::ScopedOriginCrashKey initiator_lock_crash_key(
            debug::GetRequestInitiatorOriginLockCrashKey(),
            base::OptionalOrNullptr(request_initiator_origin_lock_));
        base::debug::ScopedCrashKeyString debug_tag_crash_key(
            debug::GetFactoryDebugTagCrashKey(), debug_tag_);
        mojo::ReportBadMessage(
            "CorsURLLoaderFactory: lock VS initiator mismatch");
        return false;
      }
      break;
  }

  if (context_ && !GetAllowAnyCorsExemptHeaderForBrowser() &&
      !IsValidCorsExemptHeaders(*context_->cors_exempt_header_list(),
                                request.cors_exempt_headers)) {
    return false;
  }

  if (!AreRequestHeadersSafe(request.headers) ||
      !AreRequestHeadersSafe(request.cors_exempt_headers)) {
    return false;
  }

  URLLoader::LogConcerningRequestHeaders(request.headers,
                                         false /* added_during_redirect */);

  // Specifying CredentialsMode::kSameOrigin without an initiator origin doesn't
  // make sense.
  if (request.credentials_mode == mojom::CredentialsMode::kSameOrigin &&
      !request.request_initiator) {
    LOG(WARNING) << "same-origin credentials mode without initiator";
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: same-origin credentials mode without initiator");
    return false;
  }

  // We only support |kInclude| credentials mode with navigations. See also:
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
          request, context_, trust_token_redemption_policy_)) {
    // VerifyTrustTokenParamsIntegrityIfPresent will report an appropriate bad
    // message.
    return false;
  }

  if (!net::HttpUtil::IsToken(request.method)) {
    // Callers are expected to ensure that |method| follows RFC 7230.
    mojo::ReportBadMessage(
        "CorsURLLoaderFactory: invalid characters in method");
    return false;
  }

  // TODO(yhirano): If the request mode is "no-cors", the redirect mode should
  // be "follow".
  return true;
}

InitiatorLockCompatibility
CorsURLLoaderFactory::VerifyRequestInitiatorLockWithPluginCheck(
    uint32_t process_id,
    const base::Optional<url::Origin>& request_initiator_origin_lock,
    const base::Optional<url::Origin>& request_initiator) {
  if (process_id == mojom::kBrowserProcessId)
    return InitiatorLockCompatibility::kBrowserProcess;

  InitiatorLockCompatibility result = VerifyRequestInitiatorLock(
      request_initiator_origin_lock, request_initiator);

  if (result == InitiatorLockCompatibility::kIncorrectLock &&
      request_initiator.has_value() &&
      context_->network_service()->IsInitiatorAllowedForPlugin(
          process_id, request_initiator.value())) {
    result = InitiatorLockCompatibility::kAllowedRequestInitiatorForPlugin;
  }

  return result;
}

bool CorsURLLoaderFactory::GetAllowAnyCorsExemptHeaderForBrowser() const {
  return context_ && process_id_ == mojom::kBrowserProcessId &&
         context_->allow_any_cors_exempt_header_for_browser();
}

}  // namespace cors

}  // namespace network
