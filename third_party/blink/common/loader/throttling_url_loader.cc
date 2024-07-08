// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/throttling_url_loader.h"

#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace blink {

namespace {

void RemoveModifiedHeadersBeforeMerge(
    net::HttpRequestHeaders* modified_headers) {
  DCHECK(modified_headers);
  modified_headers->RemoveHeader(net::HttpRequestHeaders::kAcceptLanguage);
}

// Merges |removed_headers_B| into |removed_headers_A|.
void MergeRemovedHeaders(std::vector<std::string>* removed_headers_A,
                         const std::vector<std::string>& removed_headers_B) {
  for (auto& header : removed_headers_B) {
    if (!base::Contains(*removed_headers_A, header))
      removed_headers_A->emplace_back(std::move(header));
  }
}

#if DCHECK_IS_ON()
void CheckThrottleWillNotCauseCorsPreflight(
    const std::set<std::string>& initial_headers,
    const std::set<std::string>& initial_cors_exempt_headers,
    const net::HttpRequestHeaders& headers,
    const net::HttpRequestHeaders& cors_exempt_headers,
    const std::vector<std::string> cors_exempt_header_list) {
  // There are many ways for the renderer to cache the list, e.g. for workers,
  // and it might have been cached before the renderer receives a message with
  // the list. This isn't guaranteed because the caching paths aren't triggered
  // by mojo calls that are associated with the method that receives the list.
  // Since the renderer just checks to help catch develper bugs, if the list
  // isn't received don't DCHECK. Most of the time it will which is all we need
  // on bots.
  if (cors_exempt_header_list.empty())
    return;

  base::flat_set<std::string> cors_exempt_header_flat_set(
      cors_exempt_header_list);
  for (auto& header : headers.GetHeaderVector()) {
    if (!base::Contains(initial_headers, header.key) &&
        !network::cors::IsCorsSafelistedHeader(header.key, header.value)) {
      bool is_cors_exempt = cors_exempt_header_flat_set.count(header.key);
      NOTREACHED_IN_MIGRATION()
          << "Throttle added cors unsafe header " << header.key
          << (is_cors_exempt
                  ? " . Header is cors exempt so should have "
                    "been added to RequestHeaders::cors_exempt_headers "
                    "instead of "
                    "of RequestHeaders::cors_exempt_headers."
                  : "");
    }
  }

  for (auto& header : cors_exempt_headers.GetHeaderVector()) {
    if (cors_exempt_header_flat_set.count(header.key) == 0 &&
        !base::Contains(initial_cors_exempt_headers, header.key)) {
      NOTREACHED_IN_MIGRATION()
          << "Throttle added cors exempt header " << header.key
          << " but it wasn't configured as cors exempt by the browser. See "
             "content::StoragePartitionImpl::InitNetworkContext() and "
             "content::ContentBrowserClient::ConfigureNetworkContextParams().";
    }
  }
}
#endif

void RecordHistogram(const std::string& stage,
                     base::Time start,
                     const std::string& metric_type) {
  base::TimeDelta delta = base::Time::Now() - start;
  base::UmaHistogramTimes(
      base::StrCat({"Net.URLLoaderThrottle", metric_type, ".", stage}), delta);
}

void RecordDeferTimeHistogram(const std::string& stage,
                              base::Time start,
                              const char* throttle_name) {
  constexpr char kMetricType[] = "DeferTime";
  RecordHistogram(stage, start, kMetricType);
  if (throttle_name != nullptr) {
    RecordHistogram(base::StrCat({stage, ".", throttle_name}), start,
                    kMetricType);
  }
}

void RecordExecutionTimeHistogram(const std::string& stage, base::Time start) {
  RecordHistogram(stage, start, "ExecutionTime");
}

}  // namespace

const char ThrottlingURLLoader::kFollowRedirectReason[] = "FollowRedirect";

class ThrottlingURLLoader::ForwardingThrottleDelegate
    : public URLLoaderThrottle::Delegate {
 public:
  ForwardingThrottleDelegate(ThrottlingURLLoader* loader,
                             URLLoaderThrottle* throttle)
      : loader_(loader), throttle_(throttle) {}
  ForwardingThrottleDelegate(const ForwardingThrottleDelegate&) = delete;
  ForwardingThrottleDelegate& operator=(const ForwardingThrottleDelegate&) =
      delete;
  ~ForwardingThrottleDelegate() override = default;

  // URLLoaderThrottle::Delegate:
  void CancelWithError(int error_code,
                       std::string_view custom_reason) override {
    CancelWithExtendedError(error_code, 0, custom_reason);
  }

  void CancelWithExtendedError(int error_code,
                               int extended_reason_code,
                               std::string_view custom_reason) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->CancelWithExtendedError(error_code, extended_reason_code,
                                     custom_reason);
  }

  void Resume() override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->StopDeferringForThrottle(throttle_);
  }

  void UpdateDeferredResponseHead(
      network::mojom::URLResponseHeadPtr new_response_head,
      mojo::ScopedDataPipeConsumerHandle body) override {
    if (!loader_)
      return;
    ScopedDelegateCall scoped_delegate_call(this);
    loader_->UpdateDeferredResponseHead(std::move(new_response_head),
                                        std::move(body));
  }

  void InterceptResponse(
      mojo::PendingRemote<network::mojom::URLLoader> new_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          new_client_receiver,
      mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>*
          original_client_receiver,
      mojo::ScopedDataPipeConsumerHandle* body) override {
    if (!loader_)
      return;

    ScopedDelegateCall scoped_delegate_call(this);
    loader_->InterceptResponse(std::move(new_loader),
                               std::move(new_client_receiver), original_loader,
                               original_client_receiver, body);
  }

  void Detach() { loader_ = nullptr; }

  void DidRestartForCriticalClientHint() override {
    loader_->DidRestartForCriticalClientHint();
  }

 private:
  // This class helps ThrottlingURLLoader to keep track of whether it is being
  // called by its throttles.
  // If ThrottlingURLLoader is destoyed while any of the throttles is calling
  // into it, it delays destruction of the throttles. That way throttles don't
  // need to worry about any delegate calls may destory them synchronously.
  class ScopedDelegateCall {
   public:
    explicit ScopedDelegateCall(ForwardingThrottleDelegate* owner)
        : owner_(owner) {
      DCHECK(owner_->loader_);

      owner_->loader_->inside_delegate_calls_++;
    }

    ScopedDelegateCall(const ScopedDelegateCall&) = delete;
    ScopedDelegateCall& operator=(const ScopedDelegateCall&) = delete;

    ~ScopedDelegateCall() {
      // The loader may have been detached and destroyed.
      if (owner_->loader_)
        owner_->loader_->inside_delegate_calls_--;
    }

   private:
    const raw_ptr<ForwardingThrottleDelegate> owner_;
  };

  raw_ptr<ThrottlingURLLoader, DanglingUntriaged> loader_;
  const raw_ptr<URLLoaderThrottle> throttle_;
};

ThrottlingURLLoader::StartInfo::StartInfo(
    scoped_refptr<network::SharedURLLoaderFactory> in_url_loader_factory,
    int32_t in_request_id,
    uint32_t in_options,
    network::ResourceRequest* in_url_request,
    scoped_refptr<base::SequencedTaskRunner> in_task_runner,
    std::optional<std::vector<std::string>> in_cors_exempt_header_list)
    : url_loader_factory(std::move(in_url_loader_factory)),
      request_id(in_request_id),
      options(in_options),
      url_request(*in_url_request),
      task_runner(std::move(in_task_runner)) {
  cors_exempt_header_list = std::move(in_cors_exempt_header_list);
}

ThrottlingURLLoader::StartInfo::~StartInfo() = default;

ThrottlingURLLoader::ResponseInfo::ResponseInfo(
    network::mojom::URLResponseHeadPtr in_response_head)
    : response_head(std::move(in_response_head)) {}

ThrottlingURLLoader::ResponseInfo::~ResponseInfo() = default;

ThrottlingURLLoader::RedirectInfo::RedirectInfo(
    const net::RedirectInfo& in_redirect_info,
    network::mojom::URLResponseHeadPtr in_response_head)
    : redirect_info(in_redirect_info),
      response_head(std::move(in_response_head)) {}

ThrottlingURLLoader::RedirectInfo::~RedirectInfo() = default;

ThrottlingURLLoader::PriorityInfo::PriorityInfo(
    net::RequestPriority in_priority,
    int32_t in_intra_priority_value)
    : priority(in_priority), intra_priority_value(in_intra_priority_value) {}

// static
std::unique_ptr<ThrottlingURLLoader> ThrottlingURLLoader::CreateLoaderAndStart(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest* url_request,
    network::mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<std::vector<std::string>> cors_exempt_header_list,
    ClientReceiverDelegate* client_receiver_delegate) {
  DCHECK(url_request);
  std::unique_ptr<ThrottlingURLLoader> loader(
      new ThrottlingURLLoader(std::move(throttles), client, traffic_annotation,
                              client_receiver_delegate));
  loader->Start(std::move(factory), request_id, options, url_request,
                std::move(task_runner), std::move(cors_exempt_header_list));
  return loader;
}

ThrottlingURLLoader::~ThrottlingURLLoader() {
  TRACE_EVENT_WITH_FLOW0("loading", "ThrottlingURLLoader::~ThrottlingURLLoader",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
  if (inside_delegate_calls_ > 0) {
    // A throttle is calling into this object. In this case, delay destruction
    // of the throttles, so that throttles don't need to worry about any
    // delegate calls may destroy them synchronously.
    for (auto& entry : throttles_)
      entry.delegate->Detach();

    auto throttles =
        std::make_unique<std::vector<ThrottleEntry>>(std::move(throttles_));
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(throttles));
  }
}

void ThrottlingURLLoader::FollowRedirectForcingRestart() {
  url_loader_.ResetWithReason(
      network::mojom::URLLoader::kClientDisconnectReason,
      kFollowRedirectReason);
  client_receiver_.reset();
  CHECK(throttle_will_redirect_redirect_url_.is_empty());

  UpdateRequestHeaders(start_info_->url_request);

  removed_headers_.clear();
  modified_headers_.Clear();
  modified_cors_exempt_headers_.Clear();

  StartNow();
}

void ThrottlingURLLoader::ResetForFollowRedirect(
    network::ResourceRequest& resource_request,
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers) {
  MergeRemovedHeaders(&removed_headers_, removed_headers);
  RemoveModifiedHeadersBeforeMerge(&modified_headers_);
  modified_headers_.MergeFrom(modified_headers);
  modified_cors_exempt_headers_.MergeFrom(modified_cors_exempt_headers);
  // Call UpdateRequestHeaders() after headers are merged.
  UpdateRequestHeaders(resource_request);

  url_loader_.ResetWithReason(
      network::mojom::URLLoader::kClientDisconnectReason,
      kFollowRedirectReason);
}

void ThrottlingURLLoader::RestartWithFactory(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    uint32_t url_loader_options) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  url_loader_.reset();
  client_receiver_.reset();
  start_info_->url_loader_factory = std::move(factory);
  start_info_->options = url_loader_options;
  body_.reset();
  cached_metadata_.reset();
  StartNow();
}

void ThrottlingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers) {
  MergeRemovedHeaders(&removed_headers_, removed_headers);
  RemoveModifiedHeadersBeforeMerge(&modified_headers_);
  modified_headers_.MergeFrom(modified_headers);
  modified_cors_exempt_headers_.MergeFrom(modified_cors_exempt_headers);

  if (!throttle_will_start_redirect_url_.is_empty()) {
    throttle_will_start_redirect_url_ = GURL();
    // This is a synthesized redirect, so no need to tell the URLLoader.
    UpdateRequestHeaders(start_info_->url_request);
    StartNow();
    return;
  }

  if (url_loader_) {
    std::optional<GURL> new_url;
    if (!throttle_will_redirect_redirect_url_.is_empty())
      new_url = throttle_will_redirect_redirect_url_;
    url_loader_->FollowRedirect(removed_headers_, modified_headers_,
                                modified_cors_exempt_headers_, new_url);
    throttle_will_redirect_redirect_url_ = GURL();
  }

  removed_headers_.clear();
  modified_headers_.Clear();
  modified_cors_exempt_headers_.Clear();
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {
  if (!url_loader_) {
    if (!loader_completed_) {
      // Only check |deferred_stage_| if this resource has not been redirected
      // by a throttle.
      if (throttle_will_start_redirect_url_.is_empty() &&
          throttle_will_redirect_redirect_url_.is_empty()) {
        DCHECK_EQ(DEFERRED_START, deferred_stage_);
      }

      priority_info_ =
          std::make_unique<PriorityInfo>(priority, intra_priority_value);
    }
    return;
  }

  url_loader_->SetPriority(priority, intra_priority_value);
}

network::mojom::URLLoaderClientEndpointsPtr ThrottlingURLLoader::Unbind() {
  return network::mojom::URLLoaderClientEndpoints::New(
      url_loader_.Unbind(), client_receiver_.Unbind());
}

ThrottlingURLLoader::ThrottlingURLLoader(
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles,
    network::mojom::URLLoaderClient* client,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ClientReceiverDelegate* client_receiver_delegate)
    : forwarding_client_(client),
      client_receiver_delegate_(std::move(client_receiver_delegate)),
      traffic_annotation_(traffic_annotation) {
  TRACE_EVENT_WITH_FLOW0("loading", "ThrottlingURLLoader::ThrottlingURLLoader",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
  throttles_.reserve(throttles.size());
  for (auto& throttle : throttles)
    throttles_.emplace_back(this, std::move(throttle));
}

void ThrottlingURLLoader::Start(
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest* url_request,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::optional<std::vector<std::string>> cors_exempt_header_list) {
  TRACE_EVENT_WITH_FLOW0("loading", "ThrottlingURLLoader::Start",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  bool deferred = false;
  DCHECK(deferring_throttles_.empty());
  if (!throttles_.empty()) {
    original_url_ = url_request->url;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;

#if DCHECK_IS_ON()
      std::set<std::string> initial_headers, initial_cors_exempt_headers;
      if (cors_exempt_header_list) {
        for (auto& header : url_request->headers.GetHeaderVector())
          initial_headers.insert(header.key);

        for (auto& header : url_request->cors_exempt_headers.GetHeaderVector())
          initial_cors_exempt_headers.insert(header.key);
      }
#endif

      base::Time start = base::Time::Now();
      throttle->WillStartRequest(url_request, &throttle_deferred);
      RecordExecutionTimeHistogram(GetStageNameForHistogram(DEFERRED_START),
                                   start);

#if DCHECK_IS_ON()
      if (cors_exempt_header_list) {
        CheckThrottleWillNotCauseCorsPreflight(
            initial_headers, initial_cors_exempt_headers, url_request->headers,
            url_request->cors_exempt_headers, *cors_exempt_header_list);
      }
#endif

      if (original_url_ != url_request->url) {
        DCHECK(throttle_will_start_redirect_url_.is_empty())
            << "ThrottlingURLLoader doesn't support multiple throttles "
               "changing the URL.";
        if (original_url_.SchemeIsHTTPOrHTTPS() &&
            !url_request->url.SchemeIsHTTPOrHTTPS()) {
          NOTREACHED_IN_MIGRATION()
              << "A URLLoaderThrottle can't redirect from http(s) to "
              << "a non http(s) scheme.";
        } else {
          throttle_will_start_redirect_url_ = url_request->url;
        }
        // Restore the original URL so that all throttles see the same original
        // URL.
        url_request->url = original_url_;
      }
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }
  }

  start_info_ = std::make_unique<StartInfo>(factory, request_id, options,
                                            url_request, std::move(task_runner),
                                            std::move(cors_exempt_header_list));

  if (deferred)
    deferred_stage_ = DEFERRED_START;
  else
    StartNow();
}

void ThrottlingURLLoader::StartNow() {
  TRACE_EVENT_WITH_FLOW0("loading", "ThrottlingURLLoader::StartNow",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(start_info_);
  if (!throttle_will_start_redirect_url_.is_empty()) {
    auto first_party_url_policy =
        start_info_->url_request.update_first_party_url_on_redirect
            ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
            : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL;

    net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        start_info_->url_request.method, start_info_->url_request.url,
        start_info_->url_request.site_for_cookies, first_party_url_policy,
        start_info_->url_request.referrer_policy,
        start_info_->url_request.referrer.spec(),
        // Use status code 307 to preserve the method, so POST requests work.
        net::HTTP_TEMPORARY_REDIRECT, throttle_will_start_redirect_url_,
        std::nullopt, false, false, false);

    // Set Critical-CH restart info and clear for next redirect.
    redirect_info.critical_ch_restart_time = critical_ch_restart_time_;
    critical_ch_restart_time_ = base::TimeTicks();

    bool should_clear_upload = false;
    net::RedirectUtil::UpdateHttpRequest(
        start_info_->url_request.url, start_info_->url_request.method,
        redirect_info, std::nullopt, std::nullopt,
        &start_info_->url_request.headers, &should_clear_upload);

    if (should_clear_upload) {
      start_info_->url_request.request_body = nullptr;
    }

    // Set the new URL in the ResourceRequest struct so that it is the URL
    // that's requested.
    start_info_->url_request.url = throttle_will_start_redirect_url_;

    auto response_head = network::mojom::URLResponseHead::New();
    std::string header_string = base::StringPrintf(
        "HTTP/1.1 %i Internal Redirect\n"
        "Location: %s",
        net::HTTP_TEMPORARY_REDIRECT,
        throttle_will_start_redirect_url_.spec().c_str());

    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header_string));
    response_head->encoded_data_length = header_string.size();
    start_info_->task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&ThrottlingURLLoader::OnReceiveRedirect,
                       weak_factory_.GetWeakPtr(), std::move(redirect_info),
                       std::move(response_head)));
    return;
  }

  if (start_info_->url_request.keepalive) {
    base::UmaHistogramBoolean("FetchKeepAlive.Renderer.Total.Started", true);
  }
  DCHECK(start_info_->url_loader_factory);
  start_info_->url_loader_factory->CreateLoaderAndStart(
      url_loader_.BindNewPipeAndPassReceiver(start_info_->task_runner),
      start_info_->request_id, start_info_->options, start_info_->url_request,
      client_receiver_.BindNewPipeAndPassRemote(start_info_->task_runner),
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_));

  // TODO(https://crbug.com/919736): Remove this call.
  client_receiver_.internal_state()->EnableBatchDispatch();

  client_receiver_.set_disconnect_handler(base::BindOnce(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));

  if (priority_info_) {
    auto priority_info = std::move(priority_info_);
    url_loader_->SetPriority(priority_info->priority,
                             priority_info->intra_priority_value);
  }

  // Initialize with the request URL, may be updated when on redirects
  response_url_ = start_info_->url_request.url;
}

void ThrottlingURLLoader::RestartWithURLResetNow() {
  url_loader_.reset();
  client_receiver_.reset();
  throttle_will_start_redirect_url_ = original_url_;
  StartNow();
}

bool ThrottlingURLLoader::HandleThrottleResult(URLLoaderThrottle* throttle,
                                               bool throttle_deferred,
                                               bool* should_defer) {
  DCHECK(!deferring_throttles_.count(throttle));
  if (loader_completed_)
    return false;
  if (throttle_deferred) {
    *should_defer = true;
    deferring_throttles_.insert({throttle, base::Time::Now()});
  }
  return true;
}

void ThrottlingURLLoader::StopDeferringForThrottle(
    URLLoaderThrottle* throttle) {
  auto iter = deferring_throttles_.find(throttle);
  if (iter == deferring_throttles_.end())
    return;

  if (deferred_stage_ != DEFERRED_NONE) {
    const char* name = nullptr;
    if (deferred_stage_ == DEFERRED_START) {
      name = throttle->NameForLoggingWillStartRequest();
    } else if (deferred_stage_ == DEFERRED_RESPONSE) {
      name = throttle->NameForLoggingWillProcessResponse();
    }
    RecordDeferTimeHistogram(GetStageNameForHistogram(deferred_stage_),
                             iter->second, name);
  }
  deferring_throttles_.erase(iter);
  if (deferring_throttles_.empty() && !loader_completed_)
    Resume();
}

void ThrottlingURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void ThrottlingURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(deferring_throttles_.empty());
  TRACE_EVENT_WITH_FLOW1("loading", "ThrottlingURLLoader::OnReceiveResponse",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "url", response_url_.possibly_invalid_spec());
  if (client_receiver_delegate_) {
    client_receiver_delegate_->OnReceiveResponse(
        std::move(response_head), std::move(body), std::move(cached_metadata));
    return;
  }

  if (start_info_ && start_info_->url_request.keepalive) {
    base::UmaHistogramBoolean("FetchKeepAlive.Renderer.Total.ReceivedResponse",
                              true);
  }
  base::ElapsedTimer timer;
  did_receive_response_ = true;
  body_ = std::move(body);
  cached_metadata_ = std::move(cached_metadata);

  // Dispatch BeforeWillProcessResponse().
  if (!throttles_.empty()) {
    URLLoaderThrottle::RestartWithURLReset has_pending_restart(false);
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      base::Time start = base::Time::Now();
      auto weak_ptr = weak_factory_.GetWeakPtr();
      throttle->BeforeWillProcessResponse(response_url_, *response_head,
                                          &has_pending_restart);
      if (!weak_ptr) {
        return;
      }
      RecordExecutionTimeHistogram("BeforeWillProcessResponse", start);
      if (!HandleThrottleResult(throttle)) {
        return;
      }
    }

    if (has_pending_restart) {
      RestartWithURLResetNow();
      return;
    }
  }

  // Dispatch WillProcessResponse().
  if (!throttles_.empty()) {
    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      base::Time start = base::Time::Now();
      auto weak_ptr = weak_factory_.GetWeakPtr();
      throttle->WillProcessResponse(response_url_, response_head.get(),
                                    &throttle_deferred);
      if (!weak_ptr) {
        return;
      }
      RecordExecutionTimeHistogram(GetStageNameForHistogram(DEFERRED_RESPONSE),
                                   start);
      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_RESPONSE;
      response_info_ = std::make_unique<ResponseInfo>(std::move(response_head));
      client_receiver_.Pause();
      return;
    }
  }

  forwarding_client_->OnReceiveResponse(
      std::move(response_head), std::move(body_), std::move(cached_metadata_));
  base::UmaHistogramTimes("Net.URLLoaderThrottle.OnReceiveResponseTime",
                          timer.Elapsed());
}

void ThrottlingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  DCHECK(deferring_throttles_.empty());
  if (start_info_ && start_info_->url_request.keepalive) {
    base::UmaHistogramBoolean("FetchKeepAlive.Renderer.Total.Redirected", true);
  }

  if (!throttles_.empty()) {
    URLLoaderThrottle::RestartWithURLReset has_pending_restart(false);
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      auto weak_ptr = weak_factory_.GetWeakPtr();
      std::vector<std::string> removed_headers;
      net::HttpRequestHeaders modified_headers;
      net::HttpRequestHeaders modified_cors_exempt_headers;
      net::RedirectInfo redirect_info_copy = redirect_info;
      throttle->BeforeWillRedirectRequest(
          &redirect_info_copy, *response_head, &has_pending_restart,
          &removed_headers, &modified_headers, &modified_cors_exempt_headers);

      if (!weak_ptr)
        return;
    }

    if (has_pending_restart) {
      RestartWithURLResetNow();
      return;
    }

    bool deferred = false;
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      bool throttle_deferred = false;
      auto weak_ptr = weak_factory_.GetWeakPtr();
      std::vector<std::string> removed_headers;
      net::HttpRequestHeaders modified_headers;
      net::HttpRequestHeaders modified_cors_exempt_headers;
      net::RedirectInfo redirect_info_copy = redirect_info;
      base::Time start = base::Time::Now();
      throttle->WillRedirectRequest(
          &redirect_info_copy, *response_head, &throttle_deferred,
          &removed_headers, &modified_headers, &modified_cors_exempt_headers);

      if (!weak_ptr)
        return;

      RecordExecutionTimeHistogram(GetStageNameForHistogram(DEFERRED_REDIRECT),
                                   start);
#if DCHECK_IS_ON()
      if (start_info_->cors_exempt_header_list) {
        CheckThrottleWillNotCauseCorsPreflight(
            std::set<std::string>(), std::set<std::string>(), modified_headers,
            modified_cors_exempt_headers,
            *start_info_->cors_exempt_header_list);
      }
#endif

      if (redirect_info_copy.new_url != redirect_info.new_url) {
        DCHECK(throttle_will_redirect_redirect_url_.is_empty())
            << "ThrottlingURLLoader doesn't support multiple throttles "
               "changing the URL.";
        throttle_will_redirect_redirect_url_ = redirect_info_copy.new_url;
      }

      if (!HandleThrottleResult(throttle, throttle_deferred, &deferred))
        return;

      MergeRemovedHeaders(&removed_headers_, removed_headers);
      RemoveModifiedHeadersBeforeMerge(&modified_headers_);
      modified_headers_.MergeFrom(modified_headers);
      modified_cors_exempt_headers_.MergeFrom(modified_cors_exempt_headers);
    }

    if (deferred) {
      deferred_stage_ = DEFERRED_REDIRECT;
      redirect_info_ = std::make_unique<RedirectInfo>(redirect_info,
                                                      std::move(response_head));
      // |client_receiver_| can be unbound if the redirect came from a
      // throttle.
      if (client_receiver_.is_bound())
        client_receiver_.Pause();
      return;
    }
  }

  // Update the request in case |FollowRedirectForcingRestart()| is called, and
  // needs to use the request updated for the redirect.
  network::ResourceRequest& request = start_info_->url_request;
  request.url = redirect_info.new_url;
  request.method = redirect_info.new_method;
  request.site_for_cookies = redirect_info.new_site_for_cookies;
  request.referrer = GURL(redirect_info.new_referrer);
  request.referrer_policy = redirect_info.new_referrer_policy;
  if (request.trusted_params) {
    request.trusted_params->isolation_info =
        request.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(request.url));
  }

  // TODO(dhausknecht) at this point we do not actually know if we commit to the
  // redirect or if it will be cancelled. FollowRedirect would be a more
  // suitable place to set this URL but there we do not have the data.
  response_url_ = redirect_info.new_url;
  if (client_receiver_delegate_) {
    client_receiver_delegate_->EndReceiveRedirect(redirect_info,
                                                  std::move(response_head));
    return;
  }
  forwarding_client_->OnReceiveRedirect(redirect_info,
                                        std::move(response_head));
}

void ThrottlingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);

  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void ThrottlingURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kThrottlingURLLoader);

  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void ThrottlingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_EQ(DEFERRED_NONE, deferred_stage_);
  DCHECK(!loader_completed_);
  if (client_receiver_delegate_) {
    client_receiver_delegate_->OnComplete(status);
    return;
  }

  // Only dispatch WillOnCompleteWithError() if status is not OK.
  if (!throttles_.empty() && status.error_code != net::OK) {
    for (auto& entry : throttles_) {
      auto* throttle = entry.throttle.get();
      base::Time start = base::Time::Now();
      auto weak_ptr = weak_factory_.GetWeakPtr();
      throttle->WillOnCompleteWithError(status);
      if (!weak_ptr) {
        return;
      }
      RecordExecutionTimeHistogram("WillOnCompleteWithError", start);
      if (!HandleThrottleResult(throttle)) {
        return;
      }
    }
  }

  // This is the last expected message. Pipe closure before this is an error
  // (see OnClientConnectionError). After this it is expected and should be
  // ignored. The owner of |this| is expected to destroy |this| when
  // OnComplete() and all data has been read. Destruction of |this| will
  // destroy |url_loader_| appropriately.
  loader_completed_ = true;
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::OnClientConnectionError() {
  CancelWithError(net::ERR_ABORTED, "");
}

void ThrottlingURLLoader::CancelWithError(int error_code,
                                          std::string_view custom_reason) {
  CancelWithExtendedError(error_code, 0, custom_reason);
}

void ThrottlingURLLoader::CancelWithExtendedError(
    int error_code,
    int extended_reason_code,
    std::string_view custom_reason) {
  if (loader_completed_)
    return;

  network::URLLoaderCompletionStatus status;
  status.error_code = error_code;
  status.completion_time = base::TimeTicks::Now();
  status.extended_error_code = extended_reason_code;

  deferred_stage_ = DEFERRED_NONE;
  DisconnectClient(custom_reason);
  if (client_receiver_delegate_) {
    client_receiver_delegate_->CancelWithStatus(status);
    return;
  }
  forwarding_client_->OnComplete(status);
}

void ThrottlingURLLoader::Resume() {
  if (loader_completed_ || deferred_stage_ == DEFERRED_NONE)
    return;

  auto prev_deferred_stage = deferred_stage_;
  deferred_stage_ = DEFERRED_NONE;
  switch (prev_deferred_stage) {
    case DEFERRED_START: {
      StartNow();
      break;
    }
    case DEFERRED_REDIRECT: {
      // |client_receiver_| can be unbound if the redirect came from a
      // throttle.
      if (client_receiver_.is_bound())
        client_receiver_.Resume();
      // TODO(dhausknecht) at this point we do not actually know if we commit to
      // the redirect or if it will be cancelled. FollowRedirect would be a more
      // suitable place to set this URL but there we do not have the data.
      response_url_ = redirect_info_->redirect_info.new_url;
      forwarding_client_->OnReceiveRedirect(
          redirect_info_->redirect_info,
          std::move(redirect_info_->response_head));
      // Note: |this| may be deleted here.
      break;
    }
    case DEFERRED_RESPONSE: {
      client_receiver_.Resume();
      forwarding_client_->OnReceiveResponse(
          std::move(response_info_->response_head), std::move(body_),
          std::move(cached_metadata_));
      // Note: |this| may be deleted here.
      break;
    }
    case DEFERRED_NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void ThrottlingURLLoader::SetPriority(net::RequestPriority priority) {
  if (url_loader_)
    url_loader_->SetPriority(priority, -1);
}

void ThrottlingURLLoader::UpdateRequestHeaders(
    network::ResourceRequest& resource_request) {
  for (const std::string& header : removed_headers_) {
    resource_request.headers.RemoveHeader(header);
    resource_request.cors_exempt_headers.RemoveHeader(header);
  }
  resource_request.headers.MergeFrom(modified_headers_);
  resource_request.cors_exempt_headers.MergeFrom(modified_cors_exempt_headers_);
}

void ThrottlingURLLoader::UpdateDeferredResponseHead(
    network::mojom::URLResponseHeadPtr new_response_head,
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(response_info_);
  DCHECK(!body_);
  DCHECK_EQ(DEFERRED_RESPONSE, deferred_stage_);
  response_info_->response_head = std::move(new_response_head);
  body_ = std::move(body);
}

void ThrottlingURLLoader::PauseReadingBodyFromNet() {
  if (url_loader_) {
    url_loader_->PauseReadingBodyFromNet();
  }
}

void ThrottlingURLLoader::ResumeReadingBodyFromNet() {
  if (url_loader_) {
    url_loader_->ResumeReadingBodyFromNet();
  }
}

void ThrottlingURLLoader::InterceptResponse(
    mojo::PendingRemote<network::mojom::URLLoader> new_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient> new_client_receiver,
    mojo::PendingRemote<network::mojom::URLLoader>* original_loader,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>*
        original_client_receiver,
    mojo::ScopedDataPipeConsumerHandle* body) {
  response_intercepted_ = true;

  body->swap(body_);
  if (original_loader) {
    url_loader_->ResumeReadingBodyFromNet();
    *original_loader = url_loader_.Unbind();
  }
  url_loader_.Bind(std::move(new_loader));

  if (original_client_receiver)
    *original_client_receiver = client_receiver_.Unbind();
  client_receiver_.Bind(std::move(new_client_receiver),
                        start_info_->task_runner);
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &ThrottlingURLLoader::OnClientConnectionError, base::Unretained(this)));
}

void ThrottlingURLLoader::DisconnectClient(std::string_view custom_reason) {
  client_receiver_.reset();

  if (!custom_reason.empty()) {
    url_loader_.ResetWithReason(
        network::mojom::URLLoader::kClientDisconnectReason,
        std::string(custom_reason));
  } else {
    url_loader_.reset();
  }

  loader_completed_ = true;
}

const char* ThrottlingURLLoader::GetStageNameForHistogram(DeferredStage stage) {
  switch (stage) {
    case DEFERRED_START:
      return "WillStartRequest";
    case DEFERRED_REDIRECT:
      return "WillRedirectRequest";
    case DEFERRED_RESPONSE:
      return "WillProcessResponse";
    case DEFERRED_NONE:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(
    ThrottlingURLLoader* loader,
    std::unique_ptr<URLLoaderThrottle> the_throttle)
    : throttle(std::move(the_throttle)),
      delegate(std::make_unique<ForwardingThrottleDelegate>(loader,
                                                            throttle.get())) {
  throttle->set_delegate(delegate.get());
}

ThrottlingURLLoader::ThrottleEntry::ThrottleEntry(ThrottleEntry&& other) =
    default;

ThrottlingURLLoader::ThrottleEntry::~ThrottleEntry() {
  // `delegate` is destroyed before `throttle`; clear the pointer so the
  // throttle cannot inadvertently use-after-free the delegate.
  throttle->set_delegate(nullptr);
}

ThrottlingURLLoader::ThrottleEntry& ThrottlingURLLoader::ThrottleEntry::
operator=(ThrottleEntry&& other) = default;

}  // namespace blink
