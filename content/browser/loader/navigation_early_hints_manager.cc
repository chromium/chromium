// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_early_hints_manager.h"

#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "net/base/load_flags.h"
#include "net/base/schemeful_site.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_source_list.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
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
        "enabled by default yet. TODO(crbug.com/40496584): Update this "
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

network::mojom::CSPDirectiveName LinkAsAttributeToCSPDirective(
    network::mojom::LinkAsAttribute attr) {
  // https://w3c.github.io/webappsec-csp/#csp-directives
  switch (attr) {
    case network::mojom::LinkAsAttribute::kUnspecified:
      return network::mojom::CSPDirectiveName::Unknown;
    case network::mojom::LinkAsAttribute::kImage:
      return network::mojom::CSPDirectiveName::ImgSrc;
    case network::mojom::LinkAsAttribute::kFont:
      return network::mojom::CSPDirectiveName::FontSrc;
    case network::mojom::LinkAsAttribute::kScript:
      return network::mojom::CSPDirectiveName::ScriptSrcElem;
    case network::mojom::LinkAsAttribute::kStyleSheet:
      return network::mojom::CSPDirectiveName::StyleSrcElem;
    case network::mojom::LinkAsAttribute::kFetch:
      return network::mojom::CSPDirectiveName::ConnectSrc;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::CSPDirectiveName::Unknown;
}

bool CheckContentSecurityPolicyForPreload(
    const network::mojom::LinkHeaderPtr& link,
    const std::vector<network::mojom::ContentSecurityPolicyPtr>&
        content_security_policies) {
  DCHECK(link->rel == network::mojom::LinkRelAttribute::kPreload ||
         link->rel == network::mojom::LinkRelAttribute::kModulePreload);

  network::mojom::CSPDirectiveName directive =
      LinkAsAttributeToCSPDirective(link->as);

  for (network::mojom::CSPDirectiveName effective_directive = directive;
       effective_directive != network::mojom::CSPDirectiveName::Unknown;
       effective_directive =
           network::CSPFallbackDirective(effective_directive, directive)) {
    for (auto& policy : content_security_policies) {
      const auto& it = policy->directives.find(effective_directive);
      if (it == policy->directives.end()) {
        continue;
      }

      if (!network::CheckCSPSourceList(directive, *it->second, link->href,
                                       *(policy->self_origin),
                                       /*has_followed_redirect=*/false,
                                       /*is_opaque_fenced_frame=*/false)) {
        // TODO(crbug.com/40218207): Report CSP violation once the final
        // response is received.
        return false;
      }
    }
  }

  return true;
}

std::optional<network::mojom::RequestDestination>
LinkAsAttributeToRequestDestination(const network::mojom::LinkHeaderPtr& link) {
  // https://fetch.spec.whatwg.org/#concept-potential-destination-translate
  switch (link->as) {
    case network::mojom::LinkAsAttribute::kUnspecified:
      // For modulepreload, the request destination should be "script" when `as`
      // is not specified.
      // https://html.spec.whatwg.org/multipage/links.html#link-type-modulepreload
      if (link->rel == network::mojom::LinkRelAttribute::kModulePreload) {
        return network::mojom::RequestDestination::kScript;
      }
      return std::nullopt;
    case network::mojom::LinkAsAttribute::kImage:
      return network::mojom::RequestDestination::kImage;
    case network::mojom::LinkAsAttribute::kFont:
      return network::mojom::RequestDestination::kFont;
    case network::mojom::LinkAsAttribute::kScript:
      return network::mojom::RequestDestination::kScript;
    case network::mojom::LinkAsAttribute::kStyleSheet:
      return network::mojom::RequestDestination::kStyle;
    case network::mojom::LinkAsAttribute::kFetch:
      return network::mojom::RequestDestination::kEmpty;
  }
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
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
  return network::mojom::CredentialsMode::kOmit;
}

}  // namespace

NavigationEarlyHintsManagerParams::NavigationEarlyHintsManagerParams(
    const url::Origin& origin,
    net::IsolationInfo isolation_info,
    mojo::Remote<network::mojom::URLLoaderFactory> loader_factory)
    : origin(origin),
      isolation_info(std::move(isolation_info)),
      loader_factory(std::move(loader_factory)) {}

NavigationEarlyHintsManagerParams::~NavigationEarlyHintsManagerParams() =
    default;

NavigationEarlyHintsManagerParams::NavigationEarlyHintsManagerParams(
    NavigationEarlyHintsManagerParams&&) = default;

NavigationEarlyHintsManagerParams& NavigationEarlyHintsManagerParams::operator=(
    NavigationEarlyHintsManagerParams&&) = default;

// Represents a preconnect.
struct NavigationEarlyHintsManager::PreconnectEntry {
  PreconnectEntry(const url::Origin& origin,
                  network::mojom::CrossOriginAttribute cross_origin);
  ~PreconnectEntry();
  PreconnectEntry(const PreconnectEntry&);
  PreconnectEntry& operator=(const PreconnectEntry&);

  bool operator==(const PreconnectEntry&);
  bool operator<(const PreconnectEntry&) const;

  url::Origin origin;
  network::mojom::CrossOriginAttribute cross_origin;
};

NavigationEarlyHintsManager::PreconnectEntry::PreconnectEntry(
    const url::Origin& origin,
    network::mojom::CrossOriginAttribute cross_origin)
    : origin(origin), cross_origin(cross_origin) {}

NavigationEarlyHintsManager::PreconnectEntry::~PreconnectEntry() = default;

NavigationEarlyHintsManager::PreconnectEntry::PreconnectEntry(
    const PreconnectEntry&) = default;

NavigationEarlyHintsManager::PreconnectEntry&
NavigationEarlyHintsManager::PreconnectEntry::operator=(
    const PreconnectEntry&) = default;

bool NavigationEarlyHintsManager::PreconnectEntry::operator==(
    const PreconnectEntry& other) {
  return origin == other.origin && cross_origin == other.cross_origin;
}

bool NavigationEarlyHintsManager::PreconnectEntry::operator<(
    const PreconnectEntry& other) const {
  if (origin == other.origin) {
    return cross_origin < other.cross_origin;
  }
  return origin < other.origin;
}

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
    : client(std::move(client)), loader(std::move(loader)) {}

NavigationEarlyHintsManager::InflightPreload::~InflightPreload() = default;

// A URLLoaderClient which drains the content of a request to put a
// response into the disk cache. If the response was already in the cache,
// this tries to cancel reading body to avoid further disk access.
class NavigationEarlyHintsManager::PreloadURLLoaderClient
    : public network::mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:
  PreloadURLLoaderClient(NavigationEarlyHintsManager& owner,
                         const network::ResourceRequest& request)
      : owner_(owner), url_(request.url) {}

  ~PreloadURLLoaderClient() override = default;

  PreloadURLLoaderClient(const PreloadURLLoaderClient&) = delete;
  PreloadURLLoaderClient& operator=(const PreloadURLLoaderClient&) = delete;
  PreloadURLLoaderClient(PreloadURLLoaderClient&&) = delete;
  PreloadURLLoaderClient& operator=(PreloadURLLoaderClient&&) = delete;

 private:
  // mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    if (!head->network_accessed && head->was_fetched_via_cache) {
      // Cancel the client since the response is already stored in the cache.
      result_.was_canceled = true;
      MaybeCompletePreload();
      return;
    }

    if (!body) {
      return;
    }

    if (response_body_drainer_) {
      mojo::ReportBadMessage("NEHM_BAD_RESPONSE_BODY");
      return;
    }
    response_body_drainer_ =
        std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
  }
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {}
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kPreloadURLLoaderClient);
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
  void OnDataAvailable(base::span<const uint8_t> data) override {}
  void OnDataComplete() override {
    DCHECK(response_body_drainer_);
    response_body_drainer_.reset();
    MaybeCompletePreload();
  }

  bool CanCompletePreload() {
    if (result_.was_canceled) {
      return true;
    }
    if (result_.error_code.has_value() && !response_body_drainer_) {
      return true;
    }
    return false;
  }

  void MaybeCompletePreload() {
    if (CanCompletePreload()) {
      // Delete `this`.
      owner_->OnPreloadComplete(url_, result_);
    }
  }

  const raw_ref<NavigationEarlyHintsManager> owner_;
  const GURL url_;

  PreloadedResource result_;
  std::unique_ptr<mojo::DataPipeDrainer> response_body_drainer_;
};

NavigationEarlyHintsManager::NavigationEarlyHintsManager(
    BrowserContext& browser_context,
    StoragePartition& storage_partition,
    FrameTreeNodeId frame_tree_node_id,
    NavigationEarlyHintsManagerParams params)
    : browser_context_(browser_context),
      storage_partition_(storage_partition),
      frame_tree_node_id_(frame_tree_node_id),
      loader_factory_(std::move(params.loader_factory)),
      origin_(params.origin),
      isolation_info_(std::move(params.isolation_info)) {
  shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          loader_factory_.get());
}

NavigationEarlyHintsManager::~NavigationEarlyHintsManager() = default;

void NavigationEarlyHintsManager::HandleEarlyHints(
    network::mojom::EarlyHintsPtr early_hints,
    const network::ResourceRequest& request_for_navigation) {
  // Ignore the second and subsequent responses to avoid situations where
  // policies such as CSP are inconsistent among the first and following
  // responses. This behavior is specified by the step 19.5 of
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#create-navigation-params-by-fetching
  if (first_early_hints_receive_time_) {
    return;
  }

  first_early_hints_receive_time_ = base::TimeTicks::Now();

  net::ReferrerPolicy referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(early_hints->referrer_policy);

  for (const auto& link : early_hints->headers->link_headers) {
    // TODO(crbug.com/40496584): Support other `rel` attributes.
    if (link->rel == network::mojom::LinkRelAttribute::kPreconnect) {
      MaybePreconnect(link);
    } else if (link->rel == network::mojom::LinkRelAttribute::kPreload ||
               link->rel == network::mojom::LinkRelAttribute::kModulePreload) {
      MaybePreloadHintedResource(link, request_for_navigation,
                                 early_hints->headers->content_security_policy,
                                 referrer_policy);
    }
  }
}

bool NavigationEarlyHintsManager::WasResourceHintsReceived() const {
  return was_resource_hints_received_;
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
  if (inflight_preloads_.empty()) {
    std::move(callback).Run(preloaded_resources_);
  } else {
    preloads_completion_callback_for_testing_ = std::move(callback);
  }
}

void NavigationEarlyHintsManager::SetNetworkContextForTesting(
    network::mojom::NetworkContext* network_context) {
  DCHECK(!network_context_for_testing_);
  DCHECK(network_context);
  network_context_for_testing_ = network_context;
}

network::mojom::NetworkContext*
NavigationEarlyHintsManager::GetNetworkContext() {
  if (network_context_for_testing_) {
    return network_context_for_testing_;
  }

  return storage_partition_->GetNetworkContext();
}

void NavigationEarlyHintsManager::MaybePreconnect(
    const network::mojom::LinkHeaderPtr& link) {
  was_resource_hints_received_ = true;

  if (!ShouldHandleResourceHints(link)) {
    return;
  }

  PreconnectEntry entry(url::Origin::Create(link->href), link->cross_origin);
  if (preconnect_entries_.contains(entry)) {
    return;
  }

  network::mojom::NetworkContext* network_context = GetNetworkContext();
  if (!network_context) {
    return;
  }

  bool allow_credentials =
      link->cross_origin != network::mojom::CrossOriginAttribute::kAnonymous;
  network_context->PreconnectSockets(
      /*num_streams=*/1, link->href,
      allow_credentials ? network::mojom::CredentialsMode::kInclude
                        : network::mojom::CredentialsMode::kOmit,
      isolation_info_.network_anonymization_key());
  preconnect_entries_.insert(std::move(entry));
}

void NavigationEarlyHintsManager::MaybePreloadHintedResource(
    const network::mojom::LinkHeaderPtr& link,
    const network::ResourceRequest& request_for_navigation,
    const std::vector<network::mojom::ContentSecurityPolicyPtr>&
        content_security_policies,
    net::ReferrerPolicy referrer_policy) {
  DCHECK(request_for_navigation.is_outermost_main_frame);
  DCHECK(request_for_navigation.url.SchemeIsHTTPOrHTTPS());

  was_resource_hints_received_ = true;

  if (!ShouldHandleResourceHints(link)) {
    return;
  }

  // Step 2. If options's destination is not a destination, then return null.
  // https://html.spec.whatwg.org/multipage/semantics.html#create-a-link-request
  std::optional<network::mojom::RequestDestination> destination =
      LinkAsAttributeToRequestDestination(link);
  if (!destination) {
    return;
  }

  if (!CheckContentSecurityPolicyForPreload(link, content_security_policies)) {
    return;
  }

  if (inflight_preloads_.contains(link->href) ||
      preloaded_resources_.contains(link->href)) {
    return;
  }

  auto preload_origin = url::Origin::Create(link->href);

  net::SiteForCookies site_for_cookies =
      net::SiteForCookies::FromOrigin(origin_);
  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.priority = CalculateRequestPriority(link);
  request.destination = *destination;
  request.url = link->href;
  request.site_for_cookies = site_for_cookies;
  request.request_initiator = origin_;
  request.referrer = net::URLRequestJob::ComputeReferrerForPolicy(
      referrer_policy, request_for_navigation.url, request.url);
  request.referrer_policy = referrer_policy;
  request.load_flags = net::LOAD_NORMAL;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  request.mode = CalculateRequestMode(link);
  request.credentials_mode = CalculateCredentialsMode(link);

  blink::network_utils::SetAcceptHeader(request.headers, request.destination);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      CreateContentBrowserURLLoaderThrottles(
          request, &*browser_context_,
          base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                              frame_tree_node_id_),
          /*navigation_ui_data=*/nullptr, frame_tree_node_id_,
          /*navigation_id=*/std::nullopt);

  auto loader_client = std::make_unique<PreloadURLLoaderClient>(*this, request);
  auto loader = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      shared_loader_factory_, std::move(throttles),
      content::GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, &request, loader_client.get(),
      kEarlyHintsPreloadTrafficAnnotation,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  inflight_preloads_[request.url] = std::make_unique<InflightPreload>(
      std::move(loader), std::move(loader_client));

  preloaded_urls_.push_back(request.url);
}

bool NavigationEarlyHintsManager::ShouldHandleResourceHints(
    const network::mojom::LinkHeaderPtr& link) {
  if (!link->href.SchemeIsHTTPOrHTTPS()) {
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

  // TODO(crbug.com/40496584): Consider to delete `this` when there is no
  // inflight preloads.
}

// Used to determine a priority for a speculative subresource request.
// TODO(crbug.com/40496584): This is almost the same as GetRequestPriority() in
// loading_predictor_tab_helper.cc and the purpose is the same. Consider merging
// them if the logic starts to be more mature.
// platform/loader/fetch/README.md in blink contains more details on
// prioritization as well as links to all of the relevant places in the code
// where priority is determined. If the priority logic is updated here, be sure
// to update the other code as needed.
net::RequestPriority NavigationEarlyHintsManager::CalculateRequestPriority(
    const network::mojom::LinkHeaderPtr& link) {
  // When fetchPriority is explicitly specified for preload, independent of
  // most content types, the blink priority matches the fetchpriority value.
  // In net priority terms that maps to MEDIUM for "high" LOWEST for "low".
  // https://web.dev/priority-hints/#browser-priority-and-fetchpriority
  switch (link->fetch_priority) {
    case network::mojom::FetchPriorityAttribute::kHigh:
      switch (link->as) {
        case network::mojom::LinkAsAttribute::kStyleSheet:
          return net::HIGHEST;
        default:
          return net::MEDIUM;
      }
    case network::mojom::FetchPriorityAttribute::kLow:
      return net::LOWEST;
    case network::mojom::FetchPriorityAttribute::kAuto:
      switch (link->as) {
        case network::mojom::LinkAsAttribute::kStyleSheet:
          return net::HIGHEST;
        case network::mojom::LinkAsAttribute::kFont:
        case network::mojom::LinkAsAttribute::kScript:
          return net::MEDIUM;
        case network::mojom::LinkAsAttribute::kImage:
        case network::mojom::LinkAsAttribute::kFetch:
          return net::LOWEST;
        case network::mojom::LinkAsAttribute::kUnspecified:
          return net::IDLE;
      }
  }
  NOTREACHED();
}

}  // namespace content
