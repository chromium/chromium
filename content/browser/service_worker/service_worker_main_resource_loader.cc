// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_main_resource_loader.h"

#include <optional>
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
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/response_head_update_params.h"
#include "content/browser/service_worker/service_worker_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_loader_helpers.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_synthetic_response_manager.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/features.h"
#include "content/common/fetch/fetch_request_type_converters.h"
#include "content/common/service_worker/race_network_request_url_loader_client.h"
#include "content/common/service_worker/service_worker_resource_loader.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-shared.h"
#include "services/network/public/mojom/service_worker_router_info.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

namespace content {

namespace {

using SyntheticResponseStatus =
    ServiceWorkerSyntheticResponseManager::SyntheticResponseStatus;

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
bool HasAutoPreloadEligibleScript(scoped_refptr<ServiceWorkerVersion> version) {
  return content::service_worker_loader_helpers::
      FetchHandlerBypassedHashStrings()
          .contains(version->sha256_script_checksum());
}

bool IsStaticRouterRaceRequestFixEnabled() {
  return base::FeatureList::IsEnabled(
      features::kServiceWorkerStaticRouterRaceRequestFix);
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
    std::string fetch_event_client_id,
    base::WeakPtr<ServiceWorkerClient> service_worker_client,
    base::TimeTicks find_registration_start_time)
    : fallback_callback_(std::move(fallback_callback)),
      service_worker_client_(std::move(service_worker_client)),
      is_browser_startup_completed_(
          GetContentClient()->browser()->IsBrowserStartupComplete()),
      frame_tree_node_type_(
          service_worker_client_->GetFrameTreeNodeTypeStringBeforeCommit()),
      find_registration_start_time_(std::move(find_registration_start_time)),
      fetch_event_client_id_(std::move(fetch_event_client_id)) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerMainResourceLoader::ServiceWorkerMainResourceLoader", this,
      TRACE_EVENT_FLAG_FLOW_OUT);

  scoped_refptr<ServiceWorkerVersion> active_worker =
      service_worker_client_->controller();
  if (active_worker) {
    auto running_status = active_worker->running_status();
    initial_service_worker_status_ = ConvertToServiceWorkerStatus(
        running_status, active_worker->IsWarmingUp(),
        active_worker->IsWarmedUp());
    response_head_->initial_service_worker_status =
        initial_service_worker_status_;
    base::WeakPtr<ServiceWorkerContextCore> core = active_worker->context();
    if (running_status == blink::EmbeddedWorkerStatus::kStopping && core) {
      base::UmaHistogramBoolean(
          "ServiceWorker.LoadTiming.MainFrame.MainResource."
          "ServiceWorkerIsStopped.WaitingForWarmUp",
          core->IsWaitingForWarmUp(active_worker->key()));
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
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  TRACE_EVENT_WITH_FLOW1("ServiceWorker",
                         "ServiceWorkerMainResourceLoader::StartRequest", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", request.url.spec());
  DCHECK(blink::ServiceWorkerLoaderHelpers::IsMainRequestDestination(
      request.destination));
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  request_id_ = request_id;
  options_ = options;
  resource_request_ = request;
  traffic_annotation_ = traffic_annotation;

  if (service_worker_client_ &&
      service_worker_client_->fetch_request_window_id()) {
    resource_request_.fetch_window_id =
        std::make_optional(service_worker_client_->fetch_request_window_id());
  }

  DCHECK(!receiver_.is_bound());
  DCHECK(!url_loader_client_.is_bound());
  receiver_.Bind(std::move(loader));
  receiver_.set_disconnect_handler(
      base::BindOnce(&ServiceWorkerMainResourceLoader::OnConnectionClosed,
                     base::Unretained(this)));
  url_loader_client_.Bind(std::move(client));

  TransitionToStatus(Status::kStarted);
  CHECK_EQ(commit_responsibility(), FetchResponseFrom::kNoResponseYet);

  if (!service_worker_client_) {
    // We lost |service_worker_client_| (for the client) somehow before
    // dispatching FetchEvent.
    CommitCompleted(net::ERR_ABORTED, "No container host");
    return;
  }

  scoped_refptr<ServiceWorkerVersion> active_worker =
      service_worker_client_->controller();
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

  if (MaybeStartSyntheticNetworkRequest(context, active_worker)) {
    return;
  }

  RaceNetworkRequestMode race_network_request_mode =
      RaceNetworkRequestMode::kDefault;
  std::optional<network::mojom::ServiceWorkerRouterSourceType> source_type;
  // Check if registered static router rules match the request.
  if (active_worker->router_evaluator()) {
    CHECK(active_worker->router_evaluator()->IsValid());
    auto running_status = active_worker->running_status();
    auto worker_status = ConvertToServiceWorkerStatus(
        running_status, active_worker->IsWarmingUp(),
        active_worker->IsWarmedUp());

    // Set router information of matched rule for DevTools.
    response_head_->service_worker_router_info =
        network::mojom::ServiceWorkerRouterInfo::New();
    auto* router_info = response_head_->service_worker_router_info.get();
    router_info->route_rule_num =
        active_worker->router_evaluator()->rules().rules.size();
    router_info->evaluation_worker_status = worker_status;

    base::ElapsedTimer router_evaluation_timer;
    response_head_->load_timing.service_worker_router_evaluation_start =
        base::TimeTicks::Now();
    auto eval_result = active_worker->router_evaluator()->Evaluate(
        resource_request_, running_status);
    router_info->router_evaluation_time = router_evaluation_timer.Elapsed();
    // ServiceWorkerStaticRouter_Evaluate is counted only here.
    // That is because when the static routing API is used, this code will
    // always be executed even for no fetch handler case and an empty fetch
    // handler case.  Otherwise, the static routing API won't be applied for
    // them not only here but also in the subresource load.
    active_worker->CountFeature(
        blink::mojom::WebFeature::kServiceWorkerStaticRouter_Evaluate);
    if (eval_result) {  // matched the rule.
      const auto& sources = eval_result->sources;
      source_type = sources[0].type;
      set_matched_router_source_type(*source_type);
      router_info->rule_id_matched = eval_result->id;
      router_info->matched_source_type = source_type;

      switch (*source_type) {
        case network::mojom::ServiceWorkerRouterSourceType::kNetwork: {
          response_head_->service_worker_router_info->actual_source_type =
              network::mojom::ServiceWorkerRouterSourceType::kNetwork;
          // `initial_service_worker_status_` should be set if `active_worker`
          // exists.
          CHECK(initial_service_worker_status_.has_value());
          ResponseHeadUpdateParams head_update_params;
          head_update_params.router_info =
              std::move(response_head_->service_worker_router_info);
          head_update_params.load_timing_info = response_head_->load_timing;
          head_update_params.initial_service_worker_status =
              initial_service_worker_status_.value();
          Fallback(std::move(head_update_params));

          // If the kServiceWorkerStaticRouterStartServiceWorker feature is
          // enabled, it starts the ServiceWorker manually since we don't
          // instantiate ServiceWorkerFetchDispatcher, which involves the
          // ServiceWorker startup.
          // This is done asynchronously because this is for subresources and
          // not on the critical path for main resource loading.
          if (base::FeatureList::IsEnabled(
                  features::kServiceWorkerStaticRouterStartServiceWorker)) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(
                    [](scoped_refptr<ServiceWorkerVersion> active_worker) {
                      if (active_worker->running_status() !=
                          blink::EmbeddedWorkerStatus::kRunning) {
                        active_worker->StartWorker(
                            ServiceWorkerMetrics::EventType::STATIC_ROUTER,
                            base::DoNothing());
                      }
                    },
                    active_worker));
          }
          return;
        }
        case network::mojom::ServiceWorkerRouterSourceType::
            kRaceNetworkAndFetchEvent:
          race_network_request_mode = RaceNetworkRequestMode::kForced;
          break;
        case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
          race_network_request_mode = RaceNetworkRequestMode::kSkipped;
          break;
        case network::mojom::ServiceWorkerRouterSourceType::kCache:
          CreateAndRunCacheMatcher(sources[0].cache_source->cache_name,
                                   active_worker);
          return;
        case network::mojom::ServiceWorkerRouterSourceType::
            kRaceNetworkAndCache: {
          race_network_request_mode = RaceNetworkRequestMode::kForced;
          CreateAndRunCacheMatcher(
              sources[0].race_network_and_cache_source->cache_source.cache_name,
              active_worker);
          break;
        }
      }
    }
  }

  if (!source_type.has_value() ||
      *source_type !=
          network::mojom::ServiceWorkerRouterSourceType::kRaceNetworkAndCache) {
    // Dispatch the fetch event.
    fetch_dispatcher_ = std::make_unique<ServiceWorkerFetchDispatcher>(
        blink::mojom::FetchAPIRequest::From(resource_request_),
        resource_request_.destination, /*client_id=*/fetch_event_client_id_,
        /*resulting_client_id=*/
        service_worker_client_->client_uuid_for_resulting_client_id(),
        active_worker,
        base::BindOnce(&ServiceWorkerMainResourceLoader::DidPrepareFetchEvent,
                       weak_factory_.GetWeakPtr(), active_worker,
                       active_worker->running_status()),
        base::BindOnce(&ServiceWorkerMainResourceLoader::DidDispatchFetchEvent,
                       weak_factory_.GetWeakPtr()));
  }

  if (service_worker_client_->IsContainerForWindowClient()) {
    MaybeDispatchPreload(race_network_request_mode, context, active_worker);
  }

  if (race_network_request_mode == RaceNetworkRequestMode::kForced) {
    CHECK(source_type.has_value());
    switch (*source_type) {
      case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      case network::mojom::ServiceWorkerRouterSourceType::kCache:
      case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
        NOTREACHED();
      case network::mojom::ServiceWorkerRouterSourceType::
          kRaceNetworkAndFetchEvent:
        if (base::FeatureList::IsEnabled(
                features::
                    kServiceWorkerStaticRouterRaceNetworkRequestPerformanceImprovement)) {
          active_worker->CountFeature(
              blink::mojom::WebFeature::
                  kServiceWorkerStaticRouter_RaceNetworkAndFetchHandlerImprovement);
        }
        break;
      case network::mojom::ServiceWorkerRouterSourceType::kRaceNetworkAndCache:
        return;
    }
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
        SetDispatchedPreloadType(DispatchedPreloadType::kRaceNetworkRequest);
      }
      break;
    case RaceNetworkRequestMode::kDefault:
      // Prioritize NavigationPreload than AutoPreload.
      // https://github.com/explainers-by-googlers/service-worker-auto-preload#how-is-it-different-from-the-navigation-preload-api
      if (MaybeStartNavigationPreload(context_wrapper)) {
        return;
      }
      if (MaybeStartAutoPreload(context_wrapper, version)) {
        return;
      }
      break;
    case RaceNetworkRequestMode::kSkipped:
      MaybeStartNavigationPreload(context_wrapper);
      break;
  }
}

bool ServiceWorkerMainResourceLoader::MaybeStartAutoPreload(
    scoped_refptr<ServiceWorkerContextWrapper> context,
    scoped_refptr<ServiceWorkerVersion> version) {
  if (!base::FeatureList::IsEnabled(features::kServiceWorkerAutoPreload)) {
    return false;
  }

  // AutoPreload is triggered only in a main frame.
  if (!resource_request_.is_outermost_main_frame) {
    return false;
  }

  // If WebRequest API is used in this browser context, do not start AutoPreload
  // because the auto preload request may not be actually consumed and canceled.
  // WebRequest API itercepts it as a failed request, and calls
  // `OnErrorOccurred()`, while that is not actually an error.
  //
  // TODO(crbug.com/362539771): `HasWebRequestAPIProxy()` returns true not only
  // when there is an extension having WebRequest API permission but also when
  // having other permissions i.e. DeclarativeNetRequest. We should figure out
  // which permissions could call error handlers if SWAutoPreload is dispatched
  // but not consumed, and find a way to make this limitation more relaxed to
  // improve the coverage.
  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kServiceWorkerAutoPreload, "has_web_request_api_proxy",
          /*default_value=*/true) &&
      (GetContentClient()->browser()->HasWebRequestAPIProxy(
          context->browser_context()))) {
    return false;
  }

  bool use_allowlist = base::GetFieldTrialParamByFeatureAsBool(
      features::kServiceWorkerAutoPreload, "use_allowlist",
      /*default_value=*/false);
  if (use_allowlist && !HasAutoPreloadEligibleScript(version)) {
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

  // If |enable_only_when_service_worker_not_running| is true, preload requests
  // are dispatched only when the ServiceWorker is not running. When it's
  // running, preload requests for both main resource and subresources are not
  // dispatched.
  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kServiceWorkerAutoPreload,
          "enable_only_when_service_worker_not_running",
          /*default_value=*/false) &&
      version->running_status() == blink::EmbeddedWorkerStatus::kRunning) {
    return false;
  }

  bool result = StartRaceNetworkRequest(context, version);
  if (result) {
    version->CountFeature(blink::mojom::WebFeature::kServiceWorkerAutoPreload);
    SetDispatchedPreloadType(DispatchedPreloadType::kAutoPreload);
    // When the AutoPreload is triggered, set the commit responsibility
    // because the response is always committed by the fetch handler
    // regardless of the race result, except for the case when the fetch
    // handler result is fallback. The fallback case is handled after
    // receiving the fetch handler result.
    SetCommitResponsibility(FetchResponseFrom::kServiceWorker);
  }

  // If |enable_subresource_preload| feature param is true, preload requests
  // are dispatched on any subresources, otherwise preload requests won't be
  // dispatched for subresources.
  version->set_fetch_handler_bypass_option(
      base::GetFieldTrialParamByFeatureAsBool(
          features::kServiceWorkerAutoPreload, "enable_subresource_preload",
          /*default_value=*/true)
          ? blink::mojom::ServiceWorkerFetchHandlerBypassOption::kAutoPreload
          : blink::mojom::ServiceWorkerFetchHandlerBypassOption::kDefault);

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

  // RaceNetworkRequest is triggered only if the scheme is HTTP or HTTPS.
  // crbug.com/1477990
  if (!resource_request_.url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  if (!service_worker_client_) {
    return false;
  }

  // Create URLLoader related assets to handle the request triggered by
  // RaceNetworkRequset.
  mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client;
  forwarded_race_network_request_url_loader_factory_.emplace(
      forwarding_client.InitWithNewPipeAndPassReceiver(),
      service_worker_client_->CreateNetworkURLLoaderFactory(
          ServiceWorkerClient::CreateNetworkURLLoaderFactoryType::
              kRaceNetworkRequest,
          context->storage_partition(), resource_request_));
  CHECK(!race_network_request_url_loader_client_);
  race_network_request_url_loader_client_.emplace(
      resource_request_, AsWeakPtr(), std::move(forwarding_client));

  // If the initial state is not kWaitForBody, that means creating data pipes
  // failed. Do not start RaceNetworkRequest this case.
  switch (race_network_request_url_loader_client_->state()) {
    case ServiceWorkerRaceNetworkRequestURLLoaderClient::State::kWaitForBody:
      break;
    default:
      return false;
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> remote_factory;
  forwarded_race_network_request_url_loader_factory_->Clone(
      remote_factory.InitWithNewPipeAndPassReceiver());
  if (fetch_dispatcher_) {
    fetch_dispatcher_->set_race_network_request_token(
        base::UnguessableToken::Create());
    fetch_dispatcher_->set_race_network_request_loader_factory(
        std::move(remote_factory));
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient> client_to_pass;
  race_network_request_url_loader_client_->Bind(&client_to_pass);
  CHECK(!race_network_request_url_loader_factory_);
  race_network_request_url_loader_factory_ =
      service_worker_client_->CreateNetworkURLLoaderFactory(
          ServiceWorkerClient::CreateNetworkURLLoaderFactoryType::
              kRaceNetworkRequest,
          context->storage_partition(), resource_request_);

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
  if (fetch_dispatcher_ &&
      fetch_dispatcher_->MaybeStartNavigationPreload(
          resource_request_, context_wrapper, service_worker_client_)) {
    SetDispatchedPreloadType(DispatchedPreloadType::kNavigationPreload);
    return true;
  }

  return false;
}

void ServiceWorkerMainResourceLoader::CommitResponseBody(
    const network::mojom::URLResponseHeadPtr& response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TransitionToStatus(Status::kSentBody);

  // When a `response_head` is not `response_head_`, set the
  // `service_worker_router_info` and relevant fields in `load_timing` manually
  // to pass the correct routing information. Currently, this is only applicable
  // to when `race-network-and-fetch` is specified, and when this method is
  // called from `ServiceWorkerRaceNetworkRequestURLLoaderClient`.
  if (response_head_.get() != response_head.get()) {
    if (response_head_->service_worker_router_info) {
      response_head->service_worker_router_info =
          std::move(response_head_->service_worker_router_info);
    }

    if (!response_head_->load_timing.service_worker_router_evaluation_start
             .is_null()) {
      response_head->load_timing.service_worker_router_evaluation_start =
          response_head_->load_timing.service_worker_router_evaluation_start;
    }
    response_head->initial_service_worker_status =
        initial_service_worker_status_;

    // Update receive_headers_start and receive_headers_end to record
    // histograms.
    if (response_head_->load_timing.receive_headers_start.is_null()) {
      CHECK(!response_head->load_timing.receive_headers_start.is_null());
      response_head_->load_timing.receive_headers_start =
          response_head->load_timing.receive_headers_start;
      response_head_->load_timing.receive_headers_end =
          response_head->load_timing.receive_headers_end;
    }
  }

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
  CommitResponseBody(response_head_, std::move(consumer_handle), std::nullopt);
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
      case FetchResponseFrom::kServiceWorker:
        RecordTimingMetricsForFetchHandlerHandledCase();
        break;
      case FetchResponseFrom::kWithoutServiceWorker:
        if (dispatched_preload_type() ==
            DispatchedPreloadType::kRaceNetworkRequest) {
          RecordTimingMetricsForRaceNetworkRequestCase();
        }
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

  // When kRaceNetworkRequest preload is triggered, it's possible that the
  // response is already committed without waiting for the fetch event result.
  // Invalidate and destruct if the class already detached from the request.
  if (IsStaticRouterRaceRequestFixEnabled()) {
    has_fetch_event_finished_ = true;
    if (dispatched_preload_type() ==
            DispatchedPreloadType::kRaceNetworkRequest &&
        is_detached_ && status_ == Status::kCompleted) {
      InvalidateAndDeleteIfNeeded();
      return;
    }
  }

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

  // To determine the actual source type  when static routing API is used,
  // we first set the `actual_source_type` to `kNetwork`, since it is where
  // we fallback, or when we face an error. We will switch back the
  // `actual_source_type` when we are confident that the source will be on that
  // route.
  if (response_head_->service_worker_router_info &&
      response_head_->service_worker_router_info->matched_source_type) {
    response_head_->service_worker_router_info->actual_source_type =
        network::mojom::ServiceWorkerRouterSourceType::kNetwork;
  }

  bool is_race_network_request_aborted = false;
  if (race_network_request_url_loader_client_) {
    switch (race_network_request_url_loader_client_->state()) {
      case ServiceWorkerRaceNetworkRequestURLLoaderClient::State::kAborted:
        is_race_network_request_aborted = true;
        break;
      default:
        break;
    }
  }

  // Transition the state if the fetch result is fallback. This is a special
  // treatment for the case when RaceNetworkRequest and AutoPreload successfully
  // dispatched the network request.
  if (is_fallback && !is_race_network_request_aborted) {
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
        NOTREACHED();
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
      NOTREACHED();
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
  if (!service_worker_client_) {
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
    // The `SubresourceLoaderParams` previously returned by `loader_callback`
    // will be reset by `NavigationURLLoaderImpl` by detecting the controller
    // lost.
    service_worker_client_->NotifyControllerLost();
    Fallback(ResponseHeadUpdateParams());
    return;
  }

  if (IsMatchedRouterSourceType(
          network::mojom::ServiceWorkerRouterSourceType::kCache) ||
      IsMatchedRouterSourceType(network::mojom::ServiceWorkerRouterSourceType::
                                    kRaceNetworkAndCache)) {
    CHECK(cache_matcher_);
    CHECK(response_head_->service_worker_router_info);
    response_head_->load_timing.service_worker_cache_lookup_start =
        cache_matcher_->cache_lookup_start();
    response_head_->service_worker_router_info->cache_lookup_time =
        cache_matcher_->cache_lookup_duration();
  }

  // Record the timing of when the fetch event is dispatched on the worker
  // thread, when the fetch start for service worker should exist.
  // This means that the static routing API is not used, or the API is used
  // with `fetch-event` or `race`. This is used for
  // PerformanceResourceTiming#fetchStart and
  // PerformanceResourceTiming#requestStart.
  if (ShouldRecordServiceWorkerFetchStart() || is_synthetic_response_used_) {
    response_head_->load_timing.service_worker_ready_time =
        fetch_event_timing_->dispatch_event_time;
    // Exposed as PerformanceResourceTiming#requestStart.
    response_head_->load_timing.send_start =
        fetch_event_timing_->dispatch_event_time;
    // Recorded for the DevTools.
    response_head_->load_timing.send_end =
        fetch_event_timing_->dispatch_event_time;
  }

  // Records the metrics only if the code has been executed successfully in
  // the service workers because we aim to see the fallback ratio and timing.
  RecordFetchEventHandlerMetrics(fetch_result);

  if (is_fallback) {
    TransitionToStatus(Status::kCompleted);
    RecordTimingMetricsForNetworkFallbackCase();

    CHECK(initial_service_worker_status_.has_value());
    ResponseHeadUpdateParams head_update_params;
    head_update_params.load_timing_info = response_head_->load_timing;
    head_update_params.router_info =
        std::move(response_head_->service_worker_router_info);
    head_update_params.initial_service_worker_status =
        initial_service_worker_status_.value();
    Fallback(std::move(head_update_params));
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

  // Determine the actual route type of static routing API when it is used.
  // If `race-network-and-fetch` was specified, we are setting `kFetchEvent`
  // since executing this code means that the fetch event won.
  // If `race-network-and-cache` was specified, set `kCache`as the
  // `actual_source_type`, since the cache won the race. For other
  // cases (`kCache`, `kFetchEvent`), the `matched_source_type` will be the
  // `actual_source_type`.
  if (auto* route_info = response_head_->service_worker_router_info.get()) {
    if (route_info->matched_source_type &&
        *route_info->matched_source_type ==
            network::mojom::ServiceWorkerRouterSourceType::
                kRaceNetworkAndFetchEvent) {
      route_info->actual_source_type =
          network::mojom::ServiceWorkerRouterSourceType::kFetchEvent;
    } else if (route_info->matched_source_type &&
               *route_info->matched_source_type ==
                   network::mojom::ServiceWorkerRouterSourceType::
                       kRaceNetworkAndCache) {
      route_info->actual_source_type =
          network::mojom::ServiceWorkerRouterSourceType::kCache;
    } else {
      route_info->actual_source_type = route_info->matched_source_type;
    }
  }

  StartResponse(std::move(response), std::move(version),
                std::move(body_as_stream));
}

void ServiceWorkerMainResourceLoader::Fallback(
    ResponseHeadUpdateParams response_header_params) {
  CHECK(url_loader_client_.is_bound());
  CHECK(receiver_.is_bound());
  mojo::PendingRemote<network::mojom::URLLoaderClient> client =
      url_loader_client_.Unbind();
  mojo::PendingReceiver<network::mojom::URLLoader> receiver =
      receiver_.Unbind();

  if (fallback_callback_) {
    if (network::mojom::URLLoaderFactory* factory =
            std::move(fallback_callback_)
                .Run(std::move(response_header_params))) {
      // Fallback to the default factory, and pass the original parameters/mojo
      // pipes of the initial request received in `StartRequest()`.
      factory->CreateLoaderAndStart(std::move(receiver), request_id_, options_,
                                    resource_request_, std::move(client),
                                    traffic_annotation_);
      return;
    }
  }

  // The fallback factory isn't available. The pending remote/receiver are
  // destroyed here and the loading is terminated.
}

bool ServiceWorkerMainResourceLoader::MaybeStartSyntheticNetworkRequest(
    scoped_refptr<ServiceWorkerContextWrapper> context_wrapper,
    scoped_refptr<ServiceWorkerVersion> version) {
  is_synthetic_response_used_ =
      service_worker_loader_helpers::IsEligibleForSyntheticResponse(
          resource_request_.url) &&
      resource_request_.is_outermost_main_frame;
  if (!is_synthetic_response_used_) {
    return false;
  }
  if (!service_worker_client_) {
    return false;
  }

  synthetic_response_manager_.emplace(
      service_worker_client_->CreateNetworkURLLoaderFactory(
          ServiceWorkerClient::CreateNetworkURLLoaderFactoryType::
              kSyntheticNetworkRequest,
          context_wrapper->storage_partition(), resource_request_),
      version);

  // Initiate the network request. If the request URL is eligible for the
  // SyntheticResponse feature, the request is always expected to be called.
  //
  // Here is how the response is used:
  // - In the initial navigation, there is no local response header to be
  //   returned early to the client. The response of this request will be used
  //   to store the header clone and pass through to the client.
  // - The response header is kept on memory by ServiceWorkerVersion. Subsequent
  //   navigations use it as a initial response locally and pass it to the
  //   client with an empty body.
  // - In subsequent navigations, append the response body to the response.
  synthetic_response_manager_->StartRequest(
      GlobalRequestID::MakeBrowserInitiated().request_id,
      NavigationURLLoader::GetURLLoaderOptions(
          resource_request_.is_outermost_main_frame),
      resource_request_,
      base::BindRepeating(&ServiceWorkerMainResourceLoader::
                              OnReceiveResponseFromSyntheticNetworkRequest,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(
          &ServiceWorkerMainResourceLoader::OnCompleteSyntheticNetworkRequest,
          weak_factory_.GetWeakPtr()));

  switch (synthetic_response_manager_->Status()) {
    case SyntheticResponseStatus::kNotReady:
      // When it's not ready, the header is not stored yet. That means we don't
      // create a synthetic response locally, and wait for the response from the
      // network.
      break;
    case SyntheticResponseStatus::kReady:
      synthetic_response_manager_->StartSyntheticResponse(base::BindOnce(
          &ServiceWorkerMainResourceLoader::DidDispatchFetchEvent,
          weak_factory_.GetWeakPtr()));
      break;
  }

  return true;
}

void ServiceWorkerMainResourceLoader::
    OnReceiveResponseFromSyntheticNetworkRequest(
        network::mojom::URLResponseHeadPtr response_head,
        mojo::ScopedDataPipeConsumerHandle body) {
  CHECK(synthetic_response_manager_);
  // When `kNotReady`, the response is not returned with the local response
  // yet. Return the response from the network to the client here.
  CHECK_EQ(synthetic_response_manager_->Status(),
           SyntheticResponseStatus::kNotReady);
  SetCommitResponsibility(FetchResponseFrom::kWithoutServiceWorker);
  CHECK(url_loader_client_.is_bound());
  CommitResponseBody(response_head, std::move(body), std::nullopt);
}

void ServiceWorkerMainResourceLoader::OnCompleteSyntheticNetworkRequest(
    const network::URLLoaderCompletionStatus& status) {
  CHECK(synthetic_response_manager_);
  CommitCompleted(status.error_code, "Synthetic response");
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
  if (response_head_->load_timing.receive_headers_start.is_null()) {
    response_head_->load_timing.receive_headers_start = base::TimeTicks::Now();
    response_head_->load_timing.receive_headers_end =
        response_head_->load_timing.receive_headers_start;
  }
  response_source_ = response->response_source;
  if (ShouldRecordServiceWorkerFetchStart()) {
    response_head_->load_timing.service_worker_fetch_start =
        fetch_event_timing_->dispatch_event_time;
    response_head_->load_timing.service_worker_respond_with_settled =
        fetch_event_timing_->respond_with_settled_time;
  }

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

#ifndef NDEBUG
  // This check is failing against real users. If you found out why, please add
  // a regression test.
  CHECK(version->policy_container_host());
#endif
  // TODO(https://crbug.com/339200481): Find out why some ServiceWorker versions
  // have null policy container host.
  if (version->policy_container_host()) {
    response_head_->client_address_space =
        version->policy_container_host()->ip_address_space();
  }

  // Handle a redirect response. ComputeRedirectInfo returns non-null redirect
  // info if the given response is a redirect.
  std::optional<net::RedirectInfo> redirect_info =
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

  CHECK(url_loader_client_.is_bound());

  // Handle a stream response body.
  if (!body_as_stream.is_null() && body_as_stream->stream.is_valid()) {
    TRACE_EVENT_WITH_FLOW1(
        "ServiceWorker", "ServiceWorkerMainResourceLoader::StartResponse", this,
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "result",
        "stream response");
    stream_waiter_ = std::make_unique<StreamWaiter>(
        this, std::move(body_as_stream->callback_receiver));
    CommitResponseBody(response_head_, std::move(body_as_stream->stream),
                       std::nullopt);
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

    CommitResponseBody(response_head_, std::move(data_pipe), std::nullopt);
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
    const std::optional<GURL>& new_url) {
  NOTIMPLEMENTED();
}

void ServiceWorkerMainResourceLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  NOTIMPLEMENTED();
}

void ServiceWorkerMainResourceLoader::OnBlobReadingComplete(int net_error) {
  CommitCompleted(net_error, "Blob has been read.");
  body_as_blob_.reset();
}

void ServiceWorkerMainResourceLoader::SetCommitResponsibility(
    FetchResponseFrom fetch_response_from) {
  // Set the actual source type used in Static Routing API when
  // `race-network-and-fetch` or `race-network-and-cache` is used. Determine
  // this by checking the commit responsibility. If it's not the service worker,
  // the network has won. This check is conducted here since in the case of
  // `knetwork`, it does not call `DidDispatchFetchEvent`, where we set the
  // `actual_source_type` for the other sources, and the `response_head_` is
  // already passed on.
  if (response_head_ && response_head_->service_worker_router_info &&
      response_head_->service_worker_router_info->matched_source_type &&
      (*response_head_->service_worker_router_info->matched_source_type ==
           network::mojom::ServiceWorkerRouterSourceType::
               kRaceNetworkAndFetchEvent ||
       *response_head_->service_worker_router_info->matched_source_type ==
           network::mojom::ServiceWorkerRouterSourceType::
               kRaceNetworkAndCache) &&
      fetch_response_from == FetchResponseFrom::kWithoutServiceWorker) {
    response_head_->service_worker_router_info->actual_source_type =
        network::mojom::ServiceWorkerRouterSourceType::kNetwork;
  }
  ServiceWorkerResourceLoader::SetCommitResponsibility(fetch_response_from);
}

void ServiceWorkerMainResourceLoader::OnConnectionClosed() {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerMainResourceLoader::OnConnectionClosed",
      this, TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  InvalidateAndDeleteIfNeeded();
}

void ServiceWorkerMainResourceLoader::InvalidateAndDeleteIfNeeded() {
  // Postpone the invalidation and destruction if both conditions are satisfied:
  // 1) RaceNetworkRequest is dispatched and the network wins the race.
  // 2) The fetch event result is not received yet.
  // The postponed things will be done in DidDispatchFetchEvent().
  if (IsStaticRouterRaceRequestFixEnabled()) {
    if (dispatched_preload_type() ==
            DispatchedPreloadType::kRaceNetworkRequest &&
        race_network_request_url_loader_client_.has_value() &&
        !has_fetch_event_finished_) {
      CHECK(fetch_dispatcher_);
      return;
    }
  }

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

network::mojom::ServiceWorkerStatus
ServiceWorkerMainResourceLoader::ConvertToServiceWorkerStatus(
    blink::EmbeddedWorkerStatus embedded_status,
    bool is_warming_up,
    bool is_warmed_up) {
  switch (embedded_status) {
    case blink::EmbeddedWorkerStatus::kRunning:
      return network::mojom::ServiceWorkerStatus::kRunning;
    case blink::EmbeddedWorkerStatus::kStarting:
      if (is_warming_up) {
        return network::mojom::ServiceWorkerStatus::kWarmingUp;
      } else if (is_warmed_up) {
        return network::mojom::ServiceWorkerStatus::kWarmedUp;
      } else {
        return network::mojom::ServiceWorkerStatus::kStarting;
      }
    case blink::EmbeddedWorkerStatus::kStopping:
      return network::mojom::ServiceWorkerStatus::kStopping;
    case blink::EmbeddedWorkerStatus::kStopped:
      return network::mojom::ServiceWorkerStatus::kStopped;
  }
}

std::string
ServiceWorkerMainResourceLoader::GetInitialServiceWorkerStatusString() {
  CHECK(initial_service_worker_status_);
  switch (*initial_service_worker_status_) {
    case network::mojom::ServiceWorkerStatus::kRunning:
      return "RUNNING";
    case network::mojom::ServiceWorkerStatus::kStarting:
      return "STARTING";
    case network::mojom::ServiceWorkerStatus::kStopping:
      return "STOPPING";
    case network::mojom::ServiceWorkerStatus::kStopped:
      return "STOPPED";
    case network::mojom::ServiceWorkerStatus::kWarmingUp:
      return "WARMING_UP";
    case network::mojom::ServiceWorkerStatus::kWarmedUp:
      return "WARMED_UP";
  }
}

std::string ServiceWorkerMainResourceLoader::GetFrameTreeNodeTypeString() {
  return frame_tree_node_type_;
}

void ServiceWorkerMainResourceLoader::RecordFindRegistrationTiming(
    bool is_fallback) {
  RecordFindRegistrationToCompletedTrace();
  RecordFindRegistrationToRequestStartTiming();
  if (is_fallback) {
    RecordFindRegistrationToFallbackNetworkTiming();
  } else {
    RecordFindRegistrationToCompletedTiming();
  }
}

void ServiceWorkerMainResourceLoader::
    RecordTimingMetricsForFetchHandlerHandledCase() {
  if (!IsEligibleForRecordingTimingMetrics()) {
    return;
  }
  CHECK(initial_service_worker_status_);
  RecordFindRegistrationTiming(/*is_fallback=*/false);
  RecordRequestStartToForwardServiceWorkerTiming();
  RecordForwardServiceWorkerToWorkerReadyTiming();
  RecordWorkerReadyToFetchHandlerStartTiming();
  RecordFetchHandlerStartToFetchHandlerEndTiming();
  RecordFetchHandlerEndToResponseReceivedTiming();
  RecordResponseReceivedToCompletedTiming();
  RecordRequestStartToCompletedTiming(
      response_head_->load_timing.request_start);
}

void ServiceWorkerMainResourceLoader::
    RecordTimingMetricsForNetworkFallbackCase() {
  if (!IsEligibleForRecordingTimingMetrics()) {
    return;
  }
  CHECK(initial_service_worker_status_);
  RecordFindRegistrationTiming(/*is_fallback=*/true);
  RecordRequestStartToForwardServiceWorkerTiming();
  RecordForwardServiceWorkerToWorkerReadyTiming();
  RecordWorkerReadyToFetchHandlerStartTiming();
  RecordFetchHandlerStartToFetchHandlerEndTiming();
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
  RecordFindRegistrationTiming(/*is_fallback=*/false);
  RecordResponseReceivedToCompletedTiming();
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

  if (!ShouldRecordServiceWorkerFetchStart()) {
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
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.LoadTiming.MainFrame.MainResource."
                    "InitialServiceWorkerStatus.",
                    ComposeNavigationTypeString(resource_request_), ".",
                    GetFrameTreeNodeTypeString()}),
      *initial_service_worker_status_);
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.LoadTiming.MainFrame.MainResource."
                    "InitialServiceWorkerStatus.",
                    "AnyOriginNavigation.", GetFrameTreeNodeTypeString()}),
      *initial_service_worker_status_);
  base::UmaHistogramEnumeration(
      base::StrCat({"ServiceWorker.LoadTiming.MainFrame.MainResource."
                    "InitialServiceWorkerStatus.",
                    ComposeNavigationTypeString(resource_request_), ".",
                    GetFrameTreeNodeTypeString(),
                    is_browser_startup_completed_
                        ? ".BrowserStartupCompleted"
                        : ".BrowserStartupNotCompleted"}),
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
  const std::string is_browser_startup_completed_str =
      is_browser_startup_completed_ ? "BrowserStartupCompleted"
                                    : "BrowserStartupNotCompleted";
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
                    navigation_type_string, ".",
                    is_browser_startup_completed_str})
          .c_str(),
      this, load_timing.service_worker_start_time,
      "initial_service_worker_status", GetInitialServiceWorkerStatusString());
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      "ServiceWorker",
      base::StrCat({"ForwardServiceWorkerToWorkerReady.",
                    GetInitialServiceWorkerStatusString(), ".",
                    navigation_type_string, ".",
                    is_browser_startup_completed_str})
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
    case Status::kStarted:
      DCHECK_EQ(status_, Status::kNotStarted);
      break;
    case Status::kSentBody:
      DCHECK_EQ(status_, Status::kStarted);
      break;
    case Status::kCompleted:
      DCHECK(
          // Network fallback before interception.
          status_ == Status::kNotStarted ||
          // Network fallback after interception.
          status_ == Status::kStarted ||
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

void ServiceWorkerMainResourceLoader::CreateAndRunCacheMatcher(
    const std::optional<std::string>& cache_name,
    scoped_refptr<ServiceWorkerVersion> active_worker) {
  cache_matcher_ = std::make_unique<ServiceWorkerCacheStorageMatcher>(
      cache_name, blink::mojom::FetchAPIRequest::From(resource_request_),
      active_worker,
      base::BindOnce(&ServiceWorkerMainResourceLoader::DidDispatchFetchEvent,
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
                    features::kServiceWorkerStaticRouterStartServiceWorker)) {
              active_worker->StartWorker(
                  ServiceWorkerMetrics::EventType::STATIC_ROUTER,
                  base::DoNothing());
            }
          },
          active_worker));
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
