// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_early_hints_manager.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/load_flags.h"
#include "net/base/schemeful_site.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

namespace content {

namespace {

const net::NetworkTrafficAnnotationTag kEarlyHintsPreloadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("early_hints_preload",
                                        R"(
    semantics {
      sender: "Early Hints"
      description:
        "This request is issued during a main frame navigation to "
        "speculatively fetch resources that will likely be used in the frame."
      trigger:
        "A 103 Early Hints HTTP informational response is received during "
        "navigation."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies and site-specific credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This feature cannot be disabled by Settings. This feature is not "
        "enabled by default yet. TODO(crbug.com/671310): Update this "
        "description once the feature is ready."
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "Chrome uses this type of request during navigation and it cannot be "
      "disabled. Using either URLBlocklist or URLAllowlist (or a combination "
      "of both) limits the scope of these requests."
)");

constexpr char kEarlyHintsPreloadForNavigationOriginTrialName[] =
    "EarlyHintsPreloadForNavigation";

bool IsDisabledEarlyHintsPreloadForcibly() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kEarlyHintsPreloadForNavigation, "force_disable", false);
}

network::mojom::RequestDestination LinkAsAttributeToRequestDestination(
    const network::mojom::LinkHeaderPtr& link) {
  switch (link->as) {
    case network::mojom::LinkAsAttribute::kUnspecified:
      // For modulepreload destination should be "script" when `as` is not
      // specified.
      if (link->rel == network::mojom::LinkRelAttribute::kModulePreload) {
        return network::mojom::RequestDestination::kScript;
      } else {
        return network::mojom::RequestDestination::kEmpty;
      }
    case network::mojom::LinkAsAttribute::kImage:
      return network::mojom::RequestDestination::kImage;
    case network::mojom::LinkAsAttribute::kFont:
      return network::mojom::RequestDestination::kFont;
    case network::mojom::LinkAsAttribute::kScript:
      return network::mojom::RequestDestination::kScript;
    case network::mojom::LinkAsAttribute::kStyleSheet:
      return network::mojom::RequestDestination::kStyle;
  }
  NOTREACHED();
  return network::mojom::RequestDestination::kEmpty;
}

// Used to determine a priority for a speculative subresource request.
// TODO(crbug.com/671310): This is almost the same as GetRequestPriority() in
// loading_predictor_tab_helper.cc and the purpose is the same. Consider merging
// them if the logic starts to be more mature.
net::RequestPriority CalculateRequestPriority(
    const network::mojom::LinkHeaderPtr& link) {
  switch (link->as) {
    case network::mojom::LinkAsAttribute::kFont:
    case network::mojom::LinkAsAttribute::kStyleSheet:
      return net::HIGHEST;
    case network::mojom::LinkAsAttribute::kScript:
      return net::MEDIUM;
    case network::mojom::LinkAsAttribute::kImage:
      return net::LOWEST;
    case network::mojom::LinkAsAttribute::kUnspecified:
      return net::IDLE;
  }
  NOTREACHED();
  return net::IDLE;
}

network::mojom::RequestMode CalculateRequestMode(
    const network::mojom::LinkHeaderPtr& link) {
  if (link->rel == network::mojom::LinkRelAttribute::kModulePreload) {
    // When fetching a module script, mode is always "cors".
    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-single-module-script
    return network::mojom::RequestMode::kCors;
  }

  switch (link->cross_origin) {
    case network::mojom::CrossOriginAttribute::kUnspecified:
      return network::mojom::RequestMode::kNoCors;
    case network::mojom::CrossOriginAttribute::kAnonymous:
    case network::mojom::CrossOriginAttribute::kUseCredentials:
      return network::mojom::RequestMode::kCors;
  }
  NOTREACHED();
  return network::mojom::RequestMode::kSameOrigin;
}

network::mojom::CredentialsMode CalculateCredentialsMode(
    const network::mojom::LinkHeaderPtr& link) {
  switch (link->cross_origin) {
    case network::mojom::CrossOriginAttribute::kUnspecified:
      // For modulepreload credentials mode should be "same-origin" when
      // `cross-origin` is not specified.
      if (link->rel == network::mojom::LinkRelAttribute::kModulePreload) {
        return network::mojom::CredentialsMode::kSameOrigin;
      } else {
        return network::mojom::CredentialsMode::kInclude;
      }
    case network::mojom::CrossOriginAttribute::kUseCredentials:
      return network::mojom::CredentialsMode::kInclude;
    case network::mojom::CrossOriginAttribute::kAnonymous:
      return network::mojom::CredentialsMode::kSameOrigin;
  }
  NOTREACHED();
  return network::mojom::CredentialsMode::kOmit;
}

}  // namespace

NavigationEarlyHintsManager::PreloadedResource::PreloadedResource() = default;

NavigationEarlyHintsManager::PreloadedResource::~PreloadedResource() = default;

NavigationEarlyHintsManager::PreloadedResource::PreloadedResource(
    const PreloadedResource&) = default;

NavigationEarlyHintsManager::PreloadedResource&
NavigationEarlyHintsManager::PreloadedResource::operator=(
    const PreloadedResource&) = default;

NavigationEarlyHintsManager::InflightPreload::InflightPreload(
    std::unique_ptr<blink::ThrottlingURLLoader> loader,
    std::unique_ptr<PreloadURLLoaderClient> client)
    : loader(std::move(loader)), client(std::move(client)) {}

NavigationEarlyHintsManager::InflightPreload::~InflightPreload() = default;

// A URLLoaderClient which drains the content of a request to put a
// response into the disk cache. If the response was already in the cache,
// this tries to cancel reading body to avoid further disk access.
class NavigationEarlyHintsManager::PreloadURLLoaderClient
    : public network::mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:
  PreloadURLLoaderClient(NavigationEarlyHintsManager& owner, const GURL& url)
      : owner_(owner), url_(url) {}

  ~PreloadURLLoaderClient() override = default;

  PreloadURLLoaderClient(const PreloadURLLoaderClient&) = delete;
  PreloadURLLoaderClient& operator=(const PreloadURLLoaderClient&) = delete;
  PreloadURLLoaderClient(PreloadURLLoaderClient&&) = delete;
  PreloadURLLoaderClient& operator=(PreloadURLLoaderClient&&) = delete;

 private:
  // mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override {
    if (head->network_accessed || !head->was_fetched_via_cache)
      return;
    result_.was_canceled = true;
    MaybeCompletePreload();
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    NOTREACHED();
  }
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {}
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {}
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    if (response_body_drainer_) {
      mojo::ReportBadMessage("NEHM_BAD_RESPONSE_BODY");
      return;
    }
    response_body_drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
  }
  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (result_.was_canceled || result_.error_code.has_value()) {
      mojo::ReportBadMessage("NEHM_BAD_COMPLETE");
      return;
    }
    result_.error_code = status.error_code;
    result_.cors_error_status = status.cors_error_status;
    MaybeCompletePreload();
  }

  // mojo::DataPipeDrainer::Client overrides:
  void OnDataAvailable(const void* data, size_t num_bytes) override {}
  void OnDataComplete() override {
    DCHECK(response_body_drainer_);
    response_body_drainer_.reset();
    MaybeCompletePreload();
  }

  bool CanCompletePreload() {
    if (result_.was_canceled)
      return true;
    if (result_.error_code.has_value() && !response_body_drainer_)
      return true;
    return false;
  }

  void MaybeCompletePreload() {
    if (CanCompletePreload()) {
      // Delete `this`.
      owner_.OnPreloadComplete(url_, result_);
    }
  }

  NavigationEarlyHintsManager& owner_;
  const GURL url_;

  PreloadedResource result_;
  std::unique_ptr<mojo::DataPipeDrainer> response_body_drainer_;
};

NavigationEarlyHintsManager::NavigationEarlyHintsManager(
    BrowserContext& browser_context,
    mojo::Remote<network::mojom::URLLoaderFactory> loader_factory,
    url::Origin origin,
    int frame_tree_node_id)
    : browser_context_(browser_context),
      loader_factory_(std::move(loader_factory)),
      origin_(origin),
      frame_tree_node_id_(frame_tree_node_id) {
  shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          loader_factory_.get());
}

NavigationEarlyHintsManager::~NavigationEarlyHintsManager() = default;

void NavigationEarlyHintsManager::HandleEarlyHints(
    network::mojom::EarlyHintsPtr early_hints,
    const network::ResourceRequest& navigation_request) {
  bool enabled_by_origin_trial = IsPreloadForNavigationEnabledByOriginTrial(
      early_hints->origin_trial_tokens);

  for (const auto& link : early_hints->headers->link_headers) {
    // TODO(crbug.com/671310): Support other `rel` attributes.
    if (link->rel == network::mojom::LinkRelAttribute::kPreload ||
        link->rel == network::mojom::LinkRelAttribute::kModulePreload) {
      MaybePreloadHintedResource(link, navigation_request,
                                 enabled_by_origin_trial);
    }
  }
}

bool NavigationEarlyHintsManager::WasPreloadLinkHeaderReceived() const {
  // The field trial for Early Hints preload uses this method to determine
  // whether custom page metrics for the trial should be recorded. Returns false
  // when Early Hints preloads are triggered by the origin trial but the field
  // trial is disabled so that we can avoid skewing the custom page metrics for
  // the field trial.
  // TODO(crbug.com/1197989): Consider renaming this method.
  if (base::FeatureList::IsEnabled(features::kEarlyHintsPreloadForNavigation))
    return was_preload_link_header_received_;
  return was_preload_link_header_received_ &&
         !was_preload_triggered_by_origin_trial_;
}

std::vector<GURL> NavigationEarlyHintsManager::TakePreloadedResourceURLs() {
  return std::move(preloaded_urls_);
}

bool NavigationEarlyHintsManager::HasInflightPreloads() const {
  return inflight_preloads_.size() > 0;
}

void NavigationEarlyHintsManager::WaitForPreloadsFinishedForTesting(
    base::OnceCallback<void(PreloadedResources)> callback) {
  DCHECK(!preloads_completion_callback_for_testing_);
  if (inflight_preloads_.empty())
    std::move(callback).Run(preloaded_resources_);
  else
    preloads_completion_callback_for_testing_ = std::move(callback);
}

bool NavigationEarlyHintsManager::IsPreloadForNavigationEnabledByOriginTrial(
    const std::vector<std::string>& raw_tokens) {
  if (!blink::TrialTokenValidator::IsTrialPossibleOnOrigin(origin_.GetURL()))
    return false;

  auto current_time = base::Time::Now();
  for (auto& raw_token : raw_tokens) {
    blink::TrialTokenResult result =
        trial_token_validator_.ValidateToken(raw_token, origin_, current_time);
    if (result.Status() != blink::OriginTrialTokenStatus::kSuccess)
      continue;

    const blink::TrialToken* token = result.ParsedToken();
    DCHECK(token);
    DCHECK_EQ(token->IsValid(origin_, current_time),
              blink::OriginTrialTokenStatus::kSuccess);
    if (token->feature_name() != kEarlyHintsPreloadForNavigationOriginTrialName)
      continue;

    return true;
  }
  return false;
}

void NavigationEarlyHintsManager::MaybePreloadHintedResource(
    const network::mojom::LinkHeaderPtr& link,
    const network::ResourceRequest& navigation_request,
    bool enabled_by_origin_trial) {
  DCHECK(navigation_request.is_main_frame);

  was_preload_link_header_received_ = true;

  if (!ShouldPreload(link, enabled_by_origin_trial))
    return;

  DCHECK(navigation_request.url.SchemeIsHTTPOrHTTPS());
  auto preload_origin = url::Origin::Create(link->href);

  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromOrigin(origin_);
  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.priority = CalculateRequestPriority(link);
  request.destination = LinkAsAttributeToRequestDestination(link);
  request.url = link->href;
  request.site_for_cookies = site_for_cookies;
  request.request_initiator = origin_;
  request.referrer = net::URLRequestJob::ComputeReferrerForPolicy(
      navigation_request.referrer_policy, navigation_request.url, request.url);
  request.referrer_policy = navigation_request.referrer_policy;
  request.load_flags = net::LOAD_NORMAL;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  request.mode = CalculateRequestMode(link);
  request.credentials_mode = CalculateCredentialsMode(link);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      CreateContentBrowserURLLoaderThrottles(
          request, &browser_context_,
          base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                              frame_tree_node_id_),
          /*navigation_ui_data=*/nullptr, frame_tree_node_id_);

  auto loader_client =
      std::make_unique<PreloadURLLoaderClient>(*this, request.url);
  auto loader = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      shared_loader_factory_, std::move(throttles),
      content::GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, &request, loader_client.get(),
      kEarlyHintsPreloadTrafficAnnotation, base::ThreadTaskRunnerHandle::Get());

  inflight_preloads_[request.url] = std::make_unique<InflightPreload>(
      std::move(loader), std::move(loader_client));

  preloaded_urls_.push_back(request.url);

  if (enabled_by_origin_trial)
    was_preload_triggered_by_origin_trial_ = true;
}

bool NavigationEarlyHintsManager::ShouldPreload(
    const network::mojom::LinkHeaderPtr& link,
    bool enabled_by_origin_trial) {
  if (IsDisabledEarlyHintsPreloadForcibly())
    return false;

  if (!base::FeatureList::IsEnabled(
          features::kEarlyHintsPreloadForNavigation) &&
      !enabled_by_origin_trial) {
    return false;
  }

  if (!link->href.SchemeIsHTTPOrHTTPS())
    return false;

  if (inflight_preloads_.contains(link->href) ||
      preloaded_resources_.contains(link->href)) {
    return false;
  }

  return true;
}

void NavigationEarlyHintsManager::OnPreloadComplete(
    const GURL& url,
    const PreloadedResource& result) {
  DCHECK(inflight_preloads_.contains(url));
  DCHECK(!preloaded_resources_.contains(url));
  preloaded_resources_[url] = result;
  inflight_preloads_.erase(url);

  if (inflight_preloads_.empty() && preloads_completion_callback_for_testing_) {
    std::move(preloads_completion_callback_for_testing_)
        .Run(preloaded_resources_);
  }

  // TODO(crbug.com/671310): Consider to delete `this` when there is no inflight
  // preloads.
}

}  // namespace content
