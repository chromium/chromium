// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader.h"

#include <sstream>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/features.h"
#include "content/common/fetch/fetch_request_type_converters.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

namespace content {

namespace {

const char kHistogramLoadTiming[] =
    "ServiceWorker.LoadTiming.MainFrame.MainResource";

std::string ComposeFetchEventResultString(
    ServiceWorkerFetchDispatcher::FetchEventResult result,
    const blink::mojom::FetchAPIResponse& response) {
  if (result == ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback)
    return "Fallback to network";
  std::stringstream stream;
  stream << "Got response (status_code: " << response.status_code
         << " status_text: '" << response.status_text << "')";
  return stream.str();
}

const std::string ComposeNavigationTypeString(
    const network::ResourceRequest& resource_request) {
  return (resource_request.request_initiator &&
          resource_request.request_initiator->IsSameOriginWith(
              resource_request.url))
             ? "SameOriginNavigation"
             : "CrossOriginNavigation";
}

// Check the eligibility based on the allowlist. This doesn't mean the
// experiment is actually enabled. The eligibility is checked and UMA is
// reported for the analysis purpose.
bool HasRaceNetworkRequestEligibleScript(
    scoped_refptr<ServiceWorkerVersion> version) {
  return content::service_worker_loader_helpers::
      FetchHandlerBypassedHashStrings()
          .contains(version->sha256_script_checksum());
}

bool IsEligibleForRaceNetworkRequestByOriginTrial(
    scoped_refptr<ServiceWorkerVersion> version) {
  return version->origin_trial_tokens() &&
         version->origin_trial_tokens()->contains(
             "ServiceWorkerBypassFetchHandlerWithRaceNetworkRequest");
}

bool IsEligibleForRaceNetworkRequest(
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!base::FeatureList::IsEnabled(
          features::kServiceWorkerBypassFetchHandler)) {
    return false;
  }
  if (features::kServiceWorkerBypassFetchHandlerTarget.Get() !=
      features::ServiceWorkerBypassFetchHandlerTarget::
          kAllWithRaceNetworkRequest) {
    return false;
  }

  switch (features::kServiceWorkerBypassFetchHandlerStrategy.Get()) {
    // kFeatureOptIn means that the feature relies on the manual feature
    // toggle from about://flags etc, which is triggered by developers.
    case features::ServiceWorkerBypassFetchHandlerStrategy::kFeatureOptIn:
      return true;
    // If kAllowList, the allowlist should be specified. In this case,
    // RaceNetworkRequest is allowed only when the sha256 checksum of the
    // script is in the allowlist.
    case features::ServiceWorkerBypassFetchHandlerStrategy::kAllowList:
      return HasRaceNetworkRequestEligibleScript(version);
  }
}

}  // namespace

// This class waits for completion of a stream response from the service worker.
// It calls ServiceWorkerMainResourceLoader::CommitCompleted() upon completion
// of the response.
class ServiceWorkerMainResourceLoader::StreamWaiter
    : public blink::mojom::ServiceWorkerStreamCallback {
 public:
  StreamWaiter(ServiceWorkerMainResourceLoader* owner,
               mojo::PendingReceiver<blink::mojom::ServiceWorkerStreamCallback>
                   callback_receiver)
      : owner_(owner), receiver_(this, std::move(callback_receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&StreamWaiter::OnAborted, base::Unretained(this)));
  }

  StreamWaiter(const StreamWaiter&) = delete;
  StreamWaiter& operator=(const StreamWaiter&) = delete;

  // Implements mojom::ServiceWorkerStreamCallback.
  void OnCompleted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::OK, "Stream has completed.");
  }
  void OnAborted() override {
    // Destroys |this|.
    owner_->CommitCompleted(net::ERR_ABORTED, "Stream has aborted.");
  }

 private:
  raw_ptr<ServiceWorkerMainResourceLoader> owner_;
  mojo::Receiver<blink::mojom::ServiceWorkerStreamCallback> receiver_;
};

ServiceWorkerMainResourceLoader::ServiceWorkerMainResourceLoader(
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    base::WeakPtr<ServiceWorkerContainerHost> container_host,
    int frame_tree_node_id,
    base::TimeTicks find_registration_start_time)
    : fallback_callback_(std::move(fallback_callback)),
      container_host_(std::move(container_host)),
      frame_tree_node_id_(frame_tree_node_id),
      find_registration_start_time_(std::move(find_registration_start_time)) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerMainResourceLoader::ServiceWorkerMainResourceLoader", this,
      TRACE_EVENT_FLAG_FLOW_OUT);

  scoped_refptr<ServiceWorkerVersion> active_worker =
      container_host_->controller();
  if (active_worker) {
    switch (active_worker->running_status()) {
      case blink::EmbeddedWorkerStatus::kRunning:
        initial_service_worker_status_ = InitialServiceWorkerStatus::kRunning;
        break;
      case blink::EmbeddedWorkerStatus::kStarting:
        initial_service_worker_status_ = InitialServiceWorkerStatus::kStarting;
        break;
      case blink::EmbeddedWorkerStatus::kStopping:
        initial_service_worker_status_ = InitialServiceWorkerStatus::kStopping;
        break;
      case blink::EmbeddedWorkerStatus::kStopped:
        initial_service_worker_status_ = InitialServiceWorkerStatus::kStopped;
        break;
    }
    if (active_worker->IsWarmingUp()) {
      initial_service_worker_status_ = InitialServiceWorkerStatus::kWarmingUp;
    } else if (active_worker->IsWarmedUp()) {
      initial_service_worker_status_ = InitialServiceWorkerStatus::kWarmedUp;
    }
  }

  response_head_->load_timing.request_start = base::TimeTicks::Now();
  response_head_->load_timing.request_start_time = base::Time::Now();
}

ServiceWorkerMainResourceLoader::~ServiceWorkerMainResourceLoader() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerMainResourceLoader::~ServiceWorkerMainResourceLoader", this,
      TRACE_EVENT_FLAG_FLOW_IN);
}

void ServiceWorkerMainResourceLoader::DetachedFromRequest() {
  is_detached_ = true;
  // Clear |fallback_callback_| since it's no longer safe to invoke it because
  // the bound object has been destroyed.
  fallback_callback_.Reset();
  DeleteIfNeeded();
}

base::WeakPtr<ServiceWorkerMainResourceLoader>
ServiceWorkerMainResourceLoader::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerMainResourceLoader::StartRequest(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerMainResourceLoader::StartRequest", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", resource_request.url.spec());
  DCHECK(blink::ServiceWorkerLoaderHelpers::IsMainRequestDestination(
      resource_request.destination));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  resource_request_ = resource_request;
  if (container_host_ && container_host_->fetch_request_window_id()) {
    resource_request_.fetch_window_id =
        absl::make_optional(container_host_->fetch_request_window_id());
  }

  DCHECK(!receiver_.is_bound());
  DCHECK(!url_loader_client_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerMainResourceLoader::OnConnectionClosed,
                     base::Unretained(this)));
  url_loader_client_.Bind(std::move(client));

  TransitionToStatus(Status::kStarted);
  CHECK_EQ(commit_responsibility(), FetchResponseFrom::kNoResponseYet);

  if (!container_host_) {
    // We lost |container_host_| (for the client) somehow before dispatching
    // FetchEvent.
    CommitCompleted(net::ERR_ABORTED, "No container host");
    return;
  }

  scoped_refptr<ServiceWorkerVersion> active_worker =
      container_host_->controller();
  if (!active_worker) {
    CommitCompleted(net::ERR_FAILED, "No active worker");
    return;
  }

  base::WeakPtr<ServiceWorkerContextCore> core = active_worker->context();
  if (!core) {
    CommitCompleted(net::ERR_ABORTED, "No service worker context");
    return;
  }
  scoped_refptr<ServiceWorkerContextWrapper> context = core->wrapper();
  DCHECK(context);

  RaceNetworkRequestMode race_network_request_mode =
      RaceNetworkRequestMode::kDefault;
  // Check if registered static route rules match the request.
  if (active_worker->router_evaluator()) {
    CHECK(active_worker->router_evaluator()->IsValid());
    auto sources = active_worker->router_evaluator()->Evaluate(
        resource_request_, active_worker->running_status());
    // TODO(crbug.com/1371756) In some cases the router is evaluated only in the
    // renderer side. The same mechanism is needed in the subresource loader
    // as well.
    active_worker->CountFeature(
        blink::mojom::WebFeature::kServiceWorkerStaticRouter_Evaluate);
    if (!sources.empty()) {  // matched the rule.
      // TODO(crbug.com/1371756): support other sources in the full form.
      // https://github.com/yoshisatoyanagisawa/service-worker-static-routing-api/blob/main/final-form.md
      switch (sources[0].type) {
        case blink::ServiceWorkerRouterSource::Type::kNetwork:
          // Network fallback is requested.
          // URLLoader in |fallback_callback_|, in other words |url_loader_|
          // which is referred in
          // NavigationURLLoaderImpl::FallbackToNonInterceptedRequest() is not
          // ready until ServiceWorkerMainResourceLoader::StartRequest()
          // finishes, so calling the fallback at this point doesn't correctly
          // handle the fallback process. Use PostTask to run the callback after
          // finishing StartRequset().
          //
          // If the kServiceWorkerStaticRouterStartServiceWorker feature is
          // enabled, it starts the ServiceWorker manually since we don't
          // instantiate ServiceWorkerFetchDispatcher, which involves the
          // ServiceWorker startup.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](NavigationLoaderInterceptor::FallbackCallback
                         fallback_callback,
                     scoped_refptr<ServiceWorkerVersion> active_worker) {
                    std::move(fallback_callback)
                        .Run(false /* reset_subresource_loader_params */,
                             net::LoadTimingInfo());
                    if (active_worker->running_status() !=
                            blink::EmbeddedWorkerStatus::kRunning &&
                        base::FeatureList::IsEnabled(
                            features::
                                kServiceWorkerStaticRouterStartServiceWorker)) {
                      active_worker->StartWorker(
                          ServiceWorkerMetrics::EventType::STATIC_ROUTER,
                          base::DoNothing());
                    }
                  },
                  std::move(fallback_callback_), active_worker));
          return;
        case blink::ServiceWorkerRouterSource::Type::kRace:
          race_network_request_mode = RaceNetworkRequestMode::kForced;
          break;
        case blink::ServiceWorkerRouterSource::Type::kFetchEvent:
          race_network_request_mode = RaceNetworkRequestMode::kSkipped;
          break;
        case blink::ServiceWorkerRouterSource::Type::kCache:
          cache_matcher_ = std::make_unique<ServiceWorkerCacheStorageMatcher>(
              sources[0].cache_source->cache_name,
              blink::mojom::FetchAPIRequest::From(resource_request_),
              active_worker,
              base::BindOnce(
                  &ServiceWorkerMainResourceLoader::DidDispatchFetchEvent,
                  weak_factory_.GetWeakPtr()));
          cache_matcher_->Run();
          // If the kServiceWorkerStaticRouterStartServiceWorker feature is
          // enabled, it starts the ServiceWorker manually since we don't
          // instantiate ServiceWorkerFetchDispatcher, which involves the
          // ServiceWorker startup.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(
                  [](scoped_refptr<ServiceWorkerVersion> active_worker) {
                    if (active_worker->running_status() !=
                            blink::EmbeddedWorkerStatus::kRunning &&
                        base::FeatureList::IsEnabled(
                            features::
                                kServiceWorkerStaticRouterStartServiceWorker)) {
                      active_worker->StartWorker(
                          ServiceWorkerMetrics::EventType::STATIC_ROUTER,
                          base::DoNothing());
                    }
                  },
                  active_worker));
          return;
      }
    }
  }

  // Dispatch the fetch event.
  fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
      blink::mojom::FetchAPIRequest::From(resource_request_),
      resource_request_.destination, container_host_->client_uuid(),
      active_worker,
      base::BindOnce(&ServiceWorkerMainResourceLoader::DidPrepareFetchEvent,
                     weak_factory_.GetWeakPtr(), active_worker,
                     active_worker->running_status()),
      base::BindOnce(&ServiceWorkerMainResourceLoader::DidDispatchFetchEvent,
                     weak_factory_.GetWeakPtr()),
      /*is_offline_capability_check=*/false);

  if (container_host_->IsContainerForWindowClient()) {
    MaybeDispatchPreload(race_network_request_mode, context, active_worker);
  }

  // Record worker start time here as |fetch_dispatcher_| will start a service
  // worker if there is no running service worker.
  response_head_->load_timing.service_worker_start_time =
      base::TimeTicks::Now();
  fetch_dispatcher_->Run();
}

void ServiceWorkerMainResourceLoader::MaybeDispatchPreload(
    RaceNetworkRequestMode race_network_request_mode,
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    scoped_refptr<ServiceWorkerVersion> version) {
  switch (race_network_request_mode) {
    case RaceNetworkRequestMode::kForced:
      if (StartRaceNetworkRequest(context_wrapper, version)) {
        return;
      }
      break;
    case RaceNetworkRequestMode::kDefault:
      if (MaybeStartRaceNetworkRequest(context_wrapper, version)) {
        return;
      }
      break;
    case RaceNetworkRequestMode::kSkipped:
      break;
  }

  bool respect_navigation_preload = base::GetFieldTrialParamByFeatureAsBool(
      features::kServiceWorkerAutoPreload, "respect_navigation_preload",
      /*default_value=*/true);

  if (respect_navigation_preload) {
    // Prioritize NavigationPreload than AutoPreload if the
    // respect_navigation_preload feature param is true.
    if (MaybeStartNavigationPreload(context_wrapper)) {
      return;
    }
    if (MaybeStartAutoPreload(context_wrapper, version)) {
      return;
    }
  } else {
    if (MaybeStartAutoPreload(context_wrapper, version)) {
      return;
    }
    if (MaybeStartNavigationPreload(context_wrapper)) {
      return;
    }
  }
}

bool ServiceWorkerMainResourceLoader::MaybeStartAutoPreload(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!base::FeatureList::IsEnabled(features::kServiceWorkerAutoPreload)) {
    return false;
  }

  bool use_allowlist = base::GetFieldTrialParamByFeatureAsBool(
      features::kServiceWorkerAutoPreload, "use_allowlist",
      /*default_value=*/false);
  if (use_allowlist && !HasRaceNetworkRequestEligibleScript(version)) {
    return false;
  }

  // Hosts to disable AutoPreload feature. This mechanism is needed to address
  // the case when the AutoPreload behavior is problematic for some websites and
  // those should be opted out from the feature.
  const static base::NoDestructor<base::flat_set<std::string>> blocked_hosts(
      base::SplitString(
          base::GetFieldTrialParamValueByFeature(
              features::kServiceWorkerAutoPreload, "blocked_hosts"),
          ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
  if (blocked_hosts->contains(resource_request_.url.host())) {
    return false;
  }

  bool result = StartRaceNetworkRequest(context, version);
  if (result) {
    SetDispatchedPreloadType(DispatchedPreloadType::kAutoPreload);
    // When the AutoPreload is triggered, set the commit responsibility
    // because the response is always committed by the fetch handler
    // regardless of the race result, except for the case when the fetch
    // handler result is fallback. The fallback case is handled after
    // receiving the fetch handler result.
    SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
    version->set_fetch_handler_bypass_option(
        blink::mojom::ServiceWorkerFetchHandlerBypassOption::kAutoPreload);
  }

  return result;
}

bool ServiceWorkerMainResourceLoader::MaybeStartRaceNetworkRequest(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    scoped_refptr<ServiceWorkerVersion> version) {
  bool is_enabled_by_feature_flag = IsEligibleForRaceNetworkRequest(version);
  bool is_enabled_by_origin_trial =
      IsEligibleForRaceNetworkRequestByOriginTrial(version);

  if (!(is_enabled_by_feature_flag || is_enabled_by_origin_trial)) {
    // Even if the feature is not enabled, if the SW has an eligible script, set
    // the option as |kRaceNetworkRequestHoldback| for the measuring purpose.
    if (HasRaceNetworkRequestEligibleScript(version)) {
      version->set_fetch_handler_bypass_option(
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::
              kRaceNetworkRequestHoldback);
    }
    return false;
  }

  bool result = StartRaceNetworkRequest(context, version);
  if (is_enabled_by_origin_trial) {
    version->CountFeature(
        blink::mojom::WebFeature::
            kServiceWorkerBypassFetchHandlerForAllWithRaceNetworkRequestByOriginTrial);
  } else if (is_enabled_by_feature_flag) {
    version->CountFeature(
        blink::mojom::WebFeature::
            kServiceWorkerBypassFetchHandlerForAllWithRaceNetworkRequest);
  }

  if (result) {
    SetDispatchedPreloadType(DispatchedPreloadType::kRaceNetworkRequest);
  }

  return result;
}

bool ServiceWorkerMainResourceLoader::StartRaceNetworkRequest(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    scoped_refptr<ServiceWorkerVersion> version) {
  // Set fetch_handler_bypass_option to tell the renderer that
  // RaceNetworkRequest is enabled.
  version->set_fetch_handler_bypass_option(
      blink::mojom::ServiceWorkerFetchHandlerBypassOption::kRaceNetworkRequest);

  // RaceNetworkRequest only supports GET method.
  if (resource_request_.method != net::HttpRequestHeaders::kGetMethod) {
    return false;
  }

  // RaceNetworkRequest is triggered only in a main frame.
  if (resource_request_.destination !=
      network::mojom::RequestDestination::kDocument) {
    return false;
  }

  // RaceNetworkRequest is triggered only if the scheme is HTTP or HTTPS.
  // crbug.com/1477990
  if (!resource_request_.url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Create URLLoader related assets to handle the request triggered by
  // RaceNetworkRequset.
  mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client;
  forwarded_race_network_request_url_loader_factory_.emplace(
      forwarding_client.InitWithNewPipeAndPassReceiver(),
      ServiceWorkerFetchDispatcher::CreateNetworkURLLoaderFactory(
          context, frame_tree_node_id_));
  CHECK(!race_network_request_url_loader_client_);
  race_network_request_url_loader_client_.emplace(
      resource_request_, AsWeakPtr(), std::move(forwarding_client));

  // If the initial state is not kWaitForBody, that means creating data pipes
  // failed. Do not start RaceNetworkRequest this case.
  if (race_network_request_url_loader_client_->state() !=
      ServiceWorkerRaceNetworkRequestURLLoaderClient::State::kWaitForBody) {
    return false;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote_factory;
  forwarded_race_network_request_url_loader_factory_->Clone(
      remote_factory.InitWithNewPipeAndPassReceiver());
  fetch_dispatcher_->set_race_network_request_token(
      base::UnguessableToken::Create());
  fetch_dispatcher_->set_race_network_request_loader_factory(
      std::move(remote_factory));

  mojo::PendingRemote<network::mojom::URLLoaderClient> client_to_pass;
  race_network_request_url_loader_client_->Bind(&client_to_pass);
  CHECK(!race_network_request_url_loader_factory_);
  race_network_request_url_loader_factory_ =
      ServiceWorkerFetchDispatcher::CreateNetworkURLLoaderFactory(
          context, frame_tree_node_id_);

  // Perform fetch
  CHECK_EQ(commit_responsibility(), FetchResponseFrom::kNoResponseYet);
  race_network_request_url_loader_factory_->CreateLoaderAndStart(
      forwarded_race_network_request_url_loader_factory_
          ->InitURLLoaderNewPipeAndPassReceiver(),
      GlobalRequestID::MakeBrowserInitiated().request_id,
      // Since RaceNetworkRequest may not involve the fetch handler for the
      // navigation, requests SSLInfo here to be attached with the response.
      // This is required to show the HTTPS padlock by the browser.
      NavigationURLLoader::GetURLLoaderOptions(
          resource_request_.is_outermost_main_frame),
      resource_request_, std::move(client_to_pass),
      net::MutableNetworkTrafficAnnotationTag(
          ServiceWorkerRaceNetworkRequestURLLoaderClient::
              NetworkTrafficAnnotationTag()));

  return true;
}

bool ServiceWorkerMainResourceLoader::MaybeStartNavigationPreload(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper) {
  if (fetch_dispatcher_->MaybeStartNavigationPreload(
          resource_request_, context_wrapper, frame_tree_node_id_)) {
    SetDispatchedPreloadType(DispatchedPreloadType::kNavigationPreload);
    return true;
  }

  return false;
}

void ServiceWorkerMainResourceLoader::CommitResponseHeaders(
    const network::mojom::URLResponseHeadPtr& response_head) {
  DCHECK(url_loader_client_.is_bound());
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::CommitResponseHeaders",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "response_code", response_head->headers->response_code(), "status_text",
      response_head->headers->GetStatusText());
  TransitionToStatus(Status::kSentHeader);
}

void ServiceWorkerMainResourceLoader::CommitResponseBody(
    const network::mojom::URLResponseHeadPtr& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  TransitionToStatus(Status::kSentBody);
  url_loader_client_->OnReceiveResponse(response_head.Clone(),
                                        std::move(response_body),
                                        std::move(cached_metadata));
}

void ServiceWorkerMainResourceLoader::CommitEmptyResponseAndComplete() {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (CreateDataPipe(nullptr, producer_handle, consumer_handle) !=
      MOJO_RESULT_OK) {
    CommitCompleted(net::ERR_INSUFFICIENT_RESOURCES,
                    "Can't create empty data pipe");
    return;
  }

  producer_handle.reset();  // The data pipe is empty.
  CommitResponseBody(response_head_, std::move(consumer_handle), absl::nullopt);
  CommitCompleted(net::OK, "No body exists.");
}

void ServiceWorkerMainResourceLoader::CommitCompleted(int error_code,
                                                      const char* reason) {
  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::CommitCompleted", this,
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      net::ErrorToString(error_code), "reason", TRACE_STR_COPY(reason));

  DCHECK(url_loader_client_.is_bound());
  TransitionToStatus(Status::kCompleted);
  if (error_code == net::OK) {
    switch (commit_responsibility()) {
      case FetchResponseFrom::kNoResponseYet:
      case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      case FetchResponseFrom::kAutoPreloadHandlingFallback:
        NOTREACHED();
        break;
      case FetchResponseFrom::kServiceWorker:
        RecordTimingMetricsForFetchHandlerHandledCase();
        break;
      case FetchResponseFrom::kWithoutServiceWorker:
        RecordTimingMetricsForRaceNetworkRequestCase();
        break;
    }
  }

  // |stream_waiter_| calls this when done.
  stream_waiter_.reset();

  url_loader_client_->OnComplete(
      network::URLLoaderCompletionStatus(error_code));
}

void ServiceWorkerMainResourceLoader::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version,
    blink::EmbeddedWorkerStatus initial_worker_status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::DidPrepareFetchEvent",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "initial_worker_status",
      EmbeddedWorkerInstance::StatusToString(initial_worker_status));

  devtools_attached_ = version->embedded_worker()->devtools_attached();
}

void ServiceWorkerMainResourceLoader::DidDispatchFetchEvent(
    blink::ServiceWorkerStatusCode status,
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result,
    blink::mojom::FetchAPIResponsePtr response,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
    blink::mojom::ServiceWorkerFetchEventTimingPtr timing,
    scoped_refptr<ServiceWorkerVersion> version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  TRACE_EVENT_WITH_FLOW2(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::DidDispatchFetchEvent",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "status",
      blink::ServiceWorkerStatusToString(status), "result",
      ComposeFetchEventResultString(fetch_result, *response));

  bool is_fallback =
      fetch_result ==
      ServiceWorkerFetchDispatcher::FetchEventResult::kShouldFallback;

  // When AutoPreload is dispatched, set the fetch handler end time and record
  // loading metrics.
  if (dispatched_preload_type() == DispatchedPreloadType::kAutoPreload) {
    race_network_request_url_loader_client_
        ->MaybeRecordResponseReceivedToFetchHandlerEndTiming(
            base::TimeTicks::Now(), /*is_fallback=*/is_fallback);
  }

  // Transition the state if the fetch result is fallback. This is a special
  // treatment for RaceNetworkRequest and AutoPreload.
  if (is_fallback) {
    switch (commit_responsibility()) {
      case FetchResponseFrom::kNoResponseYet:
        // If the RaceNetworkRequest or AutoPreload is triggered but the
        // response is not handled yet, ask RaceNetworkRequestURLLoaderClient to
        // handle the response regardless of the response status not to dispatch
        // additional network request for fallback.
        switch (dispatched_preload_type()) {
          case DispatchedPreloadType::kRaceNetworkRequest:
          case DispatchedPreloadType::kAutoPreload:
            SetCommitResponsibility(FetchResponseFrom::kWithoutServiceWorker);
            break;
          default:
            break;
        }
        break;
      case FetchResponseFrom::kServiceWorker:
        switch (dispatched_preload_type()) {
          case DispatchedPreloadType::kAutoPreload:
            // If the AutoPreload is triggered and the response is already
            // received, but the fetch result is fallback, set the intermediate
            // state to let RaceNetworkRequestURLLoaderClient to commit the
            // response.
            SetCommitResponsibility(
                FetchResponseFrom::kAutoPreloadHandlingFallback);
            break;
          default:
            break;
        }
        break;
      case FetchResponseFrom::kWithoutServiceWorker:
        break;
      case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      case FetchResponseFrom::kAutoPreloadHandlingFallback:
        NOTREACHED_NORETURN();
    }
  }

  switch (commit_responsibility()) {
    case FetchResponseFrom::kNoResponseYet:
      SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
      break;
    case FetchResponseFrom::kServiceWorker:
      break;
    case FetchResponseFrom::kWithoutServiceWorker:
      // If the response of RaceNetworkRequest is already handled, discard the
      // fetch handler result but consume data pipes here not to make data for
      // the fetch handler being stuck.
      if (!body_as_stream.is_null() && body_as_stream->stream.is_valid() &&
          race_network_request_url_loader_client_) {
        race_network_request_url_loader_client_->DrainData(
            std::move(body_as_stream->stream));
      }
      return;
    case FetchResponseFrom::kAutoPreloadHandlingFallback:
      // |kAutoPreloadHandlingFallback| is the intermediate state to transfer
      // the commit responsibility from the fetch handler to the network
      // request (kServiceWorker). If the fetch handler result is fallback,
      // manually set the network request (kWithoutServiceWorker).
      SetCommitResponsibility(FetchResponseFrom::kWithoutServiceWorker);
      // If the network request is faster than the fetch handler, the response
      // from the network is processed but not committed. We have to explicitly
      // commit and complete the response. Otherwise
      // |ServiceWorkerRaceNetworkRequestURLLoaderClient::CommitResponse()| will
      // be called.
      race_network_request_url_loader_client_
          ->CommitAndCompleteResponseIfDataTransferFinished();
      return;
    case FetchResponseFrom::kSubresourceLoaderIsHandlingRedirect:
      NOTREACHED_NORETURN();
  }

  // Cancel the in-flight request processing for the fallback.
  if (commit_responsibility() == FetchResponseFrom::kServiceWorker &&
      race_network_request_url_loader_client_) {
    race_network_request_url_loader_client_->CancelWriteData(
        commit_responsibility());
  }
  RecordFetchResponseFrom();

  DCHECK_EQ(status_, Status::kStarted);

  ServiceWorkerMetrics::RecordFetchEventStatus(true /* is_main_resource */,
                                               status);
  if (!container_host_) {
    // The navigation or shared worker startup is cancelled. Just abort.
    CommitCompleted(net::ERR_ABORTED, "No container host");
    return;
  }

  fetch_event_timing_ = std::move(timing);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    // Dispatching the event to the service worker failed. Do a last resort
    // attempt to load the page via network as if there was no service worker.
    // It'd be more correct and simpler to remove this path and show an error
    // page, but the risk is that the user will be stuck if there's a persistent
    // failure.
    container_host_->NotifyControllerLost();
    if (fallback_callback_) {
      std::move(fallback_callback_)
          .Run(true /* reset_subresource_loader_params */,
               net::LoadTimingInfo());
    }
    return;
  }

  // Record the timing of when the fetch event is dispatched on the worker
  // thread. This is used for PerformanceResourceTiming#fetchStart and
  // PerformanceResourceTiming#requestStart, but it's still under spec
  // discussion.
  // See https://github.com/w3c/resource-timing/issues/119 for more details.
  // Exposed as PerformanceResourceTiming#fetchStart.
  response_head_->load_timing.service_worker_ready_time =
      fetch_event_timing_->dispatch_event_time;
  // Exposed as PerformanceResourceTiming#requestStart.
  response_head_->load_timing.send_start =
      fetch_event_timing_->dispatch_event_time;
  // Recorded for the DevTools.
  response_head_->load_timing.send_end =
      fetch_event_timing_->dispatch_event_time;

  // Records the metrics only if the code has been executed successfully in
  // the service workers because we aim to see the fallback ratio and timing.
  RecordFetchEventHandlerMetrics(fetch_result);

  if (is_fallback) {
    TransitionToStatus(Status::kCompleted);
    RecordTimingMetricsForNetworkFallbackCase();
    if (fallback_callback_) {
      std::move(fallback_callback_)
          .Run(false /* reset_subresource_loader_params */,
               response_head_->load_timing);
    }
    return;
  }

  DCHECK_EQ(fetch_result,
            ServiceWorkerFetchDispatcher::FetchEventResult::kGotResponse);

  // A response with status code 0 is Blink telling us to respond with
  // network error.
  if (response->status_code == 0) {
    // TODO(falken): Use more specific errors. Or just add ERR_SERVICE_WORKER?
    CommitCompleted(net::ERR_FAILED, "Zero response status");
    return;
  }

  StartResponse(std::move(response), std::move(version),
                std::move(body_as_stream));
}

void ServiceWorkerMainResourceLoader::StartResponse(
    blink::mojom::FetchAPIResponsePtr response,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(status_, Status::kStarted);

  blink::ServiceWorkerLoaderHelpers::SaveResponseInfo(*response,
                                                      response_head_.get());

  response_head_->did_service_worker_navigation_preload =
      dispatched_preload_type() == DispatchedPreloadType::kNavigationPreload;
  response_head_->load_timing.receive_headers_start = base::TimeTicks::Now();
  response_head_->load_timing.receive_headers_end =
      response_head_->load_timing.receive_headers_start;
  response_source_ = response->response_source;
  response_head_->load_timing.service_worker_fetch_start =
      fetch_event_timing_->dispatch_event_time;
  response_head_->load_timing.service_worker_respond_with_settled =
      fetch_event_timing_->respond_with_settled_time;

  if (resource_request_.request_initiator &&
      (resource_request_.request_initiator->IsSameOriginWith(
           resource_request_.url) ||
       network::TimingAllowOriginCheck(
           response_head_->parsed_headers->timing_allow_origin,
           *resource_request_.request_initiator))) {
    response_head_->timing_allow_passed = true;
  }

  // Make the navigated page inherit the SSLInfo from its controller service
  // worker's script. This affects the HTTPS padlock, etc, shown by the
  // browser. See https://crbug.com/392409 for details about this design.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  DCHECK(version->GetMainScriptResponse());
  response_head_->ssl_info = version->GetMainScriptResponse()->ssl_info;

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  absl::optional<net::RedirectInfo> redirect_info =
      blink::ServiceWorkerLoaderHelpers::ComputeRedirectInfo(resource_request_,
                                                             *response_head_);
  if (redirect_info) {
    TRACE_EVENT_WITH_FLOW2(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "redirect", "redirect url", redirect_info->new_url.spec());
    HandleRedirect(*redirect_info, response_head_);
    return;
  }

  // We have a non-redirect response. Send the headers to the client.
  CommitResponseHeaders(response_head_);

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "stream response");
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(body_as_stream->callback_receiver));
    CommitResponseBody(response_head_, std::move(body_as_stream->stream),
                       absl::nullopt);
    // StreamWaiter will call CommitCompleted() when done.
    return;
  }

  // Handle a blob response body.
  if (response->blob) {
    DCHECK(response->blob->blob.is_valid());
    body_as_blob_.Bind(std::move(response->blob->blob));
    mojo::ScopedDataPipeConsumerHandle data_pipe;
    int error = blink::ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
        &body_as_blob_, response->blob->size,
        base::BindOnce(&ServiceWorkerMainResourceLoader::OnBlobReadingComplete,
                       weak_factory_.GetWeakPtr()),
        &data_pipe);
    if (error != net::OK) {
      CommitCompleted(error, "Failed to read blob body");
      return;
    }
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "blob response");

    CommitResponseBody(response_head_, std::move(data_pipe), absl::nullopt);
    // We continue in OnBlobReadingComplete().
    return;
  }

  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerMainResourceLoader::StartResponse", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "result", "no body");

  CommitEmptyResponseAndComplete();
}

void ServiceWorkerMainResourceLoader::HandleRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHeadPtr& response_head) {
  response_head->encoded_data_length = 0;
  url_loader_client_->OnReceiveRedirect(redirect_info, response_head->Clone());
  // Our client is the navigation loader, which will start a new URLLoader for
  // the redirect rather than calling FollowRedirect(), so we're done here.
  TransitionToStatus(Status::kCompleted);
}

// URLLoader implementation----------------------------------------

void ServiceWorkerMainResourceLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  NOTIMPLEMENTED();
}

void ServiceWorkerMainResourceLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void ServiceWorkerMainResourceLoader::PauseReadingBodyFromNet() {}

void ServiceWorkerMainResourceLoader::ResumeReadingBodyFromNet() {}

void ServiceWorkerMainResourceLoader::OnBlobReadingComplete(int net_error) {
  CommitCompleted(net_error, "Blob has been read.");
  body_as_blob_.reset();
}

void ServiceWorkerMainResourceLoader::OnConnectionClosed() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::OnConnectionClosed",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // The fetch dispatcher or stream waiter may still be running. Don't let them
  // do callbacks back to this loader, since it is now done with the request.
  // TODO(falken): Try to move this to CommitCompleted(), since the same
  // justification applies there too.
  weak_factory_.InvalidateWeakPtrs();
  fetch_dispatcher_.reset();
  stream_waiter_.reset();
  receiver_.reset();

  // Respond to the request if it's not yet responded to.
  if (status_ != Status::kCompleted)
    CommitCompleted(net::ERR_ABORTED, "Disconnected pipe before completed");

  url_loader_client_.reset();
  DeleteIfNeeded();
}

void ServiceWorkerMainResourceLoader::DeleteIfNeeded() {
  if (!receiver_.is_bound() && is_detached_)
    delete this;
}

std::string
ServiceWorkerMainResourceLoader::GetInitialServiceWorkerStatusString() {
  CHECK(initial_service_worker_status_);
  switch (*initial_service_worker_status_) {
    case InitialServiceWorkerStatus::kRunning:
      return "RUNNING";
    case InitialServiceWorkerStatus::kStarting:
      return "STARTING";
    case InitialServiceWorkerStatus::kStopping:
      return "STOPPING";
    case InitialServiceWorkerStatus::kStopped:
      return "STOPPED";
    case InitialServiceWorkerStatus::kWarmingUp:
      return "WARMING_UP";
    case InitialServiceWorkerStatus::kWarmedUp:
      return "WARMED_UP";
  }
}

void ServiceWorkerMainResourceLoader::
    RecordTimingMetricsForFetchHandlerHandledCase() {
  if (!IsEligibleForRecordingTimingMetrics()) {
    return;
  }
  CHECK(initial_service_worker_status_);
  RecordFindRegistrationToCompletedTrace();
  RecordFindRegistrationToRequestStartTiming();
  RecordRequestStartToForwardServiceWorkerTiming();
  RecordForwardServiceWorkerToWorkerReadyTiming();
  RecordWorkerReadyToFetchHandlerStartTiming();
  RecordFetchHandlerStartToFetchHandlerEndTiming();
  RecordFetchHandlerEndToResponseReceivedTiming();
  RecordResponseReceivedToCompletedTiming();
  RecordFindRegistrationToCompletedTiming();
  RecordRequestStartToCompletedTiming(
      response_head_->load_timing.request_start);
}

void ServiceWorkerMainResourceLoader::
    RecordTimingMetricsForNetworkFallbackCase() {
  if (!IsEligibleForRecordingTimingMetrics()) {
    return;
  }
  CHECK(initial_service_worker_status_);
  RecordFindRegistrationToCompletedTrace();
  RecordFindRegistrationToRequestStartTiming();
  RecordRequestStartToForwardServiceWorkerTiming();
  RecordForwardServiceWorkerToWorkerReadyTiming();
  RecordWorkerReadyToFetchHandlerStartTiming();
  RecordFetchHandlerStartToFetchHandlerEndTiming();
  RecordFindRegistrationToFallbackNetworkTiming();
  RecordStartToFallbackNetworkTiming();
  RecordFetchHandlerEndToFallbackNetworkTiming();
}

void ServiceWorkerMainResourceLoader::
    RecordTimingMetricsForRaceNetworkRequestCase() {
  DCHECK(race_network_request_url_loader_client_);
  if (!IsEligibleForRecordingTimingMetrics()) {
    return;
  }
  CHECK(initial_service_worker_status_);
  RecordFindRegistrationToCompletedTrace();
  RecordFindRegistrationToRequestStartTiming();
  RecordFindRegistrationToCompletedTiming();
  RecordRequestStartToCompletedTiming(
      race_network_request_url_loader_client_->GetLoadTimingInfo()
          .request_start);
}

bool ServiceWorkerMainResourceLoader::IsEligibleForRecordingTimingMetrics() {
  // We only record these metrics for top-level navigation.
  if (resource_request_.destination !=
      network::mojom::RequestDestination::kDocument) {
    return false;
  }

  // |fetch_event_timing_| is recorded in renderer so we can get reasonable
  // metrics only when TimeTicks are consistent across processes.
  if (!base::TimeTicks::IsHighResolution() ||
      !base::TimeTicks::IsConsistentAcrossProcesses()) {
    return false;
  }

  // Don't record metrics when DevTools is attached to reduce noise.
  if (devtools_attached_) {
    return false;
  }

  // Don't record metrics when DevTools specify force_update_on_page_load to
  // reduce noise.
  if (find_registration_start_time_.is_null()) {
    return false;
  }

  DCHECK(!completion_time_.is_null());

  return true;
}

void ServiceWorkerMainResourceLoader::RecordFindRegistrationToCompletedTrace() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "ServiceWorker", kHistogramLoadTiming, this,
      find_registration_start_time_, "url", resource_request_.url);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", kHistogramLoadTiming, this, completion_time_);
}

void ServiceWorkerMainResourceLoader::
    RecordFindRegistrationToRequestStartTiming() {
  const base::TimeTicks request_start =
      response_head_->load_timing.request_start;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "ServiceWorker", "FindRegistrationToRequestStart", this,
      find_registration_start_time_, "url", resource_request_.url);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "FindRegistrationToRequestStart", this, request_start);

  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".FindRegistrationToRequestStart"}),
      request_start - find_registration_start_time_);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".FindRegistrationToRequestStart.",
                    GetInitialServiceWorkerStatusString()}),
      request_start - find_registration_start_time_);
  base::UmaHistogramEnumeration(
      "ServiceWorker.LoadTiming.MainFrame.MainResource."
      "InitialServiceWorkerStatus",
      *initial_service_worker_status_);
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.LoadTiming.MainFrame.MainResource."
                    "InitialServiceWorkerStatus.",
                    ComposeNavigationTypeString(resource_request_)}),
      *initial_service_worker_status_);
}

void ServiceWorkerMainResourceLoader::
    RecordRequestStartToForwardServiceWorkerTiming() {
  const net::LoadTimingInfo& load_timing = response_head_->load_timing;
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".StartToForwardServiceWorker"}),
      load_timing.service_worker_start_time - load_timing.request_start);
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".StartToForwardServiceWorker.",
                    GetInitialServiceWorkerStatusString()}),
      load_timing.service_worker_start_time - load_timing.request_start);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "RequestStartToForwardServiceWorker", this,
      load_timing.request_start);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "RequestStartToForwardServiceWorker", this,
      load_timing.service_worker_start_time);
}

void ServiceWorkerMainResourceLoader::
    RecordForwardServiceWorkerToWorkerReadyTiming() {
  const net::LoadTimingInfo& load_timing = response_head_->load_timing;
  const std::string navigation_type_string =
      ComposeNavigationTypeString(resource_request_);
  base::TimeDelta time = load_timing.service_worker_ready_time -
                         load_timing.service_worker_start_time;
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kHistogramLoadTiming, ".ForwardServiceWorkerToWorkerReady2"}),
      time);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming,
                    ".ForwardServiceWorkerToWorkerReady2.",
                    GetInitialServiceWorkerStatusString()}),
      time);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming,
                    ".ForwardServiceWorkerToWorkerReady2.",
                    navigation_type_string}),
      time);
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kHistogramLoadTiming, ".ForwardServiceWorkerToWorkerReady2.",
           GetInitialServiceWorkerStatusString(), ".", navigation_type_string}),
      time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "ServiceWorker",
      base::StrCat({"ForwardServiceWorkerToWorkerReady.",
                    GetInitialServiceWorkerStatusString(), ".",
                    navigation_type_string})
          .c_str(),
      this, load_timing.service_worker_start_time,
      "initial_service_worker_status", GetInitialServiceWorkerStatusString());
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker",
      base::StrCat({"ForwardServiceWorkerToWorkerReady.",
                    GetInitialServiceWorkerStatusString(), ".",
                    navigation_type_string})
          .c_str(),
      this, load_timing.service_worker_ready_time);
}

void ServiceWorkerMainResourceLoader::
    RecordWorkerReadyToFetchHandlerStartTiming() {
  DCHECK(fetch_event_timing_);
  const net::LoadTimingInfo& load_timing = response_head_->load_timing;
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".WorkerReadyToFetchHandlerStart"}),
      fetch_event_timing_->dispatch_event_time -
          load_timing.service_worker_ready_time);
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".WorkerReadyToFetchHandlerStart.",
                    GetInitialServiceWorkerStatusString()}),
      fetch_event_timing_->dispatch_event_time -
          load_timing.service_worker_ready_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "WorkerReadyToFetchHandlerStart", this,
      load_timing.service_worker_ready_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "WorkerReadyToFetchHandlerStart", this,
      fetch_event_timing_->dispatch_event_time);
}

void ServiceWorkerMainResourceLoader::
    RecordFetchHandlerStartToFetchHandlerEndTiming() {
  DCHECK(fetch_event_timing_);
  base::UmaHistogramTimes(base::StrCat({kHistogramLoadTiming,
                                        ".FetchHandlerStartToFetchHandlerEnd"}),
                          fetch_event_timing_->respond_with_settled_time -
                              fetch_event_timing_->dispatch_event_time);
  base::UmaHistogramTimes(base::StrCat({kHistogramLoadTiming,
                                        ".FetchHandlerStartToFetchHandlerEnd.",
                                        GetInitialServiceWorkerStatusString()}),
                          fetch_event_timing_->respond_with_settled_time -
                              fetch_event_timing_->dispatch_event_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerStartToFetchHandlerEnd", this,
      fetch_event_timing_->dispatch_event_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerStartToFetchHandlerEnd", this,
      fetch_event_timing_->respond_with_settled_time);
}

void ServiceWorkerMainResourceLoader::
    RecordFetchHandlerEndToResponseReceivedTiming() {
  DCHECK(fetch_event_timing_);
  const net::LoadTimingInfo& load_timing = response_head_->load_timing;
  base::UmaHistogramTimes(base::StrCat({kHistogramLoadTiming,
                                        ".FetchHandlerEndToResponseReceived"}),
                          load_timing.receive_headers_end -
                              fetch_event_timing_->respond_with_settled_time);
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".FetchHandlerEndToResponseReceived.",
                    GetInitialServiceWorkerStatusString()}),
      load_timing.receive_headers_end -
          fetch_event_timing_->respond_with_settled_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerEndToResponseReceived", this,
      fetch_event_timing_->respond_with_settled_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerEndToResponseReceived", this,
      load_timing.receive_headers_end);
}

void ServiceWorkerMainResourceLoader::
    RecordResponseReceivedToCompletedTiming() {
  const net::LoadTimingInfo& load_timing = response_head_->load_timing;
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".ResponseReceivedToCompleted2"}),
      completion_time_ - load_timing.receive_headers_end);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".ResponseReceivedToCompleted2.",
                    GetInitialServiceWorkerStatusString()}),
      completion_time_ - load_timing.receive_headers_end);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP1(
      "ServiceWorker", "ResponseReceivedToCompleted", this,
      load_timing.receive_headers_end, "fetch_response_source",
      blink::ServiceWorkerLoaderHelpers::FetchResponseSourceToSuffix(
          response_source_));
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "ResponseReceivedToCompleted", this, completion_time_);
  // Same as above, breakdown by response source.
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kHistogramLoadTiming, ".ResponseReceivedToCompleted2.",
           blink::ServiceWorkerLoaderHelpers::FetchResponseSourceToSuffix(
               response_source_)}),
      completion_time_ - load_timing.receive_headers_end);
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kHistogramLoadTiming, ".ResponseReceivedToCompleted2.",
           blink::ServiceWorkerLoaderHelpers::FetchResponseSourceToSuffix(
               response_source_),
           ".", GetInitialServiceWorkerStatusString()}),
      completion_time_ - load_timing.receive_headers_end);
}

void ServiceWorkerMainResourceLoader::
    RecordFindRegistrationToCompletedTiming() {
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".FindRegistrationToCompleted"}),
      completion_time_ - find_registration_start_time_);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".FindRegistrationToCompleted.",
                    GetInitialServiceWorkerStatusString()}),
      completion_time_ - find_registration_start_time_);
}

void ServiceWorkerMainResourceLoader::RecordRequestStartToCompletedTiming(
    const base::TimeTicks& request_start) {
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".StartToCompleted"}),
      completion_time_ - request_start);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".StartToCompleted.",
                    GetInitialServiceWorkerStatusString()}),
      completion_time_ - request_start);
}

void ServiceWorkerMainResourceLoader::
    RecordFindRegistrationToFallbackNetworkTiming() {
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {kHistogramLoadTiming, ".FindRegistrationToFallbackNetwork"}),
      completion_time_ - find_registration_start_time_);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".FindRegistrationToFallbackNetwork.",
                    GetInitialServiceWorkerStatusString()}),
      completion_time_ - find_registration_start_time_);
}

void ServiceWorkerMainResourceLoader::RecordStartToFallbackNetworkTiming() {
  const net::LoadTimingInfo& load_timing = response_head_->load_timing;
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".StartToFallbackNetwork"}),
      completion_time_ - load_timing.request_start);
  base::UmaHistogramMediumTimes(
      base::StrCat({kHistogramLoadTiming, ".StartToFallbackNetwork.",
                    GetInitialServiceWorkerStatusString()}),
      completion_time_ - load_timing.request_start);
}

void ServiceWorkerMainResourceLoader::
    RecordFetchHandlerEndToFallbackNetworkTiming() {
  DCHECK(fetch_event_timing_);
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".FetchHandlerEndToFallbackNetwork"}),
      completion_time_ - fetch_event_timing_->respond_with_settled_time);
  base::UmaHistogramTimes(
      base::StrCat({kHistogramLoadTiming, ".FetchHandlerEndToFallbackNetwork.",
                    GetInitialServiceWorkerStatusString()}),
      completion_time_ - fetch_event_timing_->respond_with_settled_time);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerEndToFallbackNetwork", this,
      fetch_event_timing_->respond_with_settled_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker", "FetchHandlerEndToFallbackNetwork", this,
      completion_time_);
}

void ServiceWorkerMainResourceLoader::RecordFetchEventHandlerMetrics(
    ServiceWorkerFetchDispatcher::FetchEventResult fetch_result) {
  base::UmaHistogramEnumeration(
      "ServiceWorker.MainFrame.MainResource.FetchResult", fetch_result);

  // Time spent by fetch handlers per |fetch_result|.
  base::UmaHistogramTimes(
      base::StrCat({
          kHistogramLoadTiming,
          ".FetchHandlerStartToFetchHandlerEndByFetchResult",
          ServiceWorkerFetchDispatcher::FetchEventResultToSuffix(fetch_result),
      }),
      fetch_event_timing_->respond_with_settled_time -
          fetch_event_timing_->dispatch_event_time);
}

void ServiceWorkerMainResourceLoader::TransitionToStatus(Status new_status) {
#if DCHECK_IS_ON()
  switch (new_status) {
    case Status::kNotStarted:
      NOTREACHED();
      break;
    case Status::kStarted:
      DCHECK_EQ(status_, Status::kNotStarted);
      break;
    case Status::kSentHeader:
      DCHECK_EQ(status_, Status::kStarted);
      break;
    case Status::kSentBody:
      DCHECK_EQ(status_, Status::kSentHeader);
      break;
    case Status::kCompleted:
      DCHECK(
          // Network fallback before interception.
          status_ == Status::kNotStarted ||
          // Network fallback after interception.
          status_ == Status::kStarted ||
          // Pipe creation failure for empty response.
          status_ == Status::kSentHeader ||
          // Success case or error while sending the response's body.
          status_ == Status::kSentBody);
      break;
  }
#endif  // DCHECK_IS_ON()

  status_ = new_status;
  if (new_status == Status::kCompleted)
    completion_time_ = base::TimeTicks::Now();
}

bool ServiceWorkerMainResourceLoader::IsMainResourceLoader() {
  return true;
}

ServiceWorkerMainResourceLoaderWrapper::ServiceWorkerMainResourceLoaderWrapper(
    std::unique_ptr<ServiceWorkerMainResourceLoader> loader)
    : loader_(std::move(loader)) {}

ServiceWorkerMainResourceLoaderWrapper::
    ~ServiceWorkerMainResourceLoaderWrapper() {
  if (loader_)
    loader_.release()->DetachedFromRequest();
}

}  // namespace content
