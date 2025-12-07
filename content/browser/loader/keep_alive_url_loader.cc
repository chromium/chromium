// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/keep_alive_attribution_request_helper.h"
#include "content/browser/renderer_host/mixed_content_checker.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/keep_alive_request_tracker.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_request.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/features.h"

namespace features {

// See third_party/blink/renderer/core/fetch/retry_options.idl for details of
// each parameter.
// Note that we allow script to set their own policies for retry, but these
// browser-configured limits act as both lower/upper bounds and also default
// values if not set by script.

const base::FeatureParam<size_t> kMaxRetryCount{&blink::features::kFetchRetry,
                                                "max_retry_count", 10};
const base::FeatureParam<base::TimeDelta> kMinRetryDelta{
    &blink::features::kFetchRetry, "min_retry_delta", base::Milliseconds(500)};
const base::FeatureParam<double> kMinRetryBackoffFactor{
    &blink::features::kFetchRetry, "min_retry_backoff", 1.0};
const base::FeatureParam<base::TimeDelta> kMaxRetryAge{
    &blink::features::kFetchRetry, "max_retry_age", base::Days(1)};
// TODO(crbug.com/417930271): This should be reviewed beyond OT.
const base::FeatureParam<bool> kAddRetryHeader{&blink::features::kFetchRetry,
                                               "add_retry_header", true};

}  // namespace features

namespace content {
namespace {

// Very simple ThreadChecker to use as a static variable. Used because using
// `base::ThreadChecker` directly is not permitted and the static keyword cannot
// be applied to THREAD_CHECKER. When not under DCHECK this class will be empty
// and do nothing.
class WrappedThreadChecker {
 public:
  void Check() { DCHECK_CALLED_ON_VALID_THREAD(thread_checker); }

 private:
  THREAD_CHECKER(thread_checker);
};

constexpr net::NetworkTrafficAnnotationTag kKeepAliveRetryAnnotationTag =
    net::DefineNetworkTrafficAnnotation("keepalive_fetch_retry", R"(
  semantics {
    sender: "KeepAlive Fetch Retry Mechanism"
    description:
      "This request is an automated retry of a fetch request that was "
      "originally initiated by a website with the `keepalive: true` flag "
      "and `retryOptions` specifying that retries should occur. This "
      "retry is triggered by the browser process if the initial attempt or "
      "a subsequent retry fails, and can occur even if the originating "
      "renderer process has been closed. The purpose is to increase the "
      "likelihood of successful data delivery for critical beacons, logs, "
      "or other data the website intended to send reliably."
    trigger:
      "An initial keepalive fetch request (or a previous retry of it) "
      "failed due to a network error or a specific HTTP 5xx server error "
      "(if configured in RetryOptions). The RetryOptions associated with "
      "the original request dictated that a retry should be attempted. "
      "This specific request is triggered by an automated timer within the "
      "browser process's KeepAliveURLLoader after a calculated backoff period."
    data:
      "The same HTTP method, URL, headers, and body as the original "
      "keepalive request that is being retried. This data is determined "
      "by the website that initiated the original fetch request."
    destination: WEBSITE
    internal {
        contacts {
          email: "chrome-loading@google.com"
        }
      }
    user_data {
      type: ARBITRARY_DATA
      type: SENSITIVE_URL
    }
    last_reviewed: "2025-05-27"

  }
  policy {
    cookies_allowed: YES
    cookies_store: "Same as the original request. Governed by the "
                   "credentials mode ('omit', 'same-origin', 'include') "
                   "of the original fetch request."
    setting:
      "This retry mechanism is a sub-feature of the Fetch API's "
      "`keepalive` and the proposed `retryOptions`. Websites enable this "
      "by constructing fetch requests with these options. Users can "
      "disable JavaScript or clear/block cookies for sites, which would "
      "affect the original request and consequently prevent retries. "
      "There isn't a separate browser setting to disable only the retry "
      "aspect of keepalive fetches."
    policy_exception_justification:
      "This is an automated follow-up to a user-initiated (via website "
      "JavaScript) keepalive request."
  }
)");

// Internally enforces a limit to allow a loader outlive its renderer after
// receiving disconnection notification from the renderer.
//
// Defaults to 30s, the same as pre-migration's timeout.
constexpr base::TimeDelta kDefaultDisconnectedKeepAliveURLLoaderTimeout =
    base::Seconds(30);

base::TimeDelta GetDisconnectedKeepAliveURLLoaderTimeout() {
  return base::Seconds(GetFieldTrialParamByFeatureAsInt(
      blink::features::kKeepAliveInBrowserMigration,
      "disconnected_loader_timeout_seconds",
      base::checked_cast<int32_t>(
          kDefaultDisconnectedKeepAliveURLLoaderTimeout.InSeconds())));
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must remain in sync with FetchLaterBrowserMetricType in
// tools/metrics/histograms/enums.xml.
enum class FetchLaterBrowserMetricType {
  kAbortedByInitiator = 0,
  kStartedAfterInitiatorDisconnected = 1,
  kStartedByInitiator = 2,
  kCancelledAfterTimeLimit = 3,
  kStartedWhenShutdown = 4,
  kMaxValue = kStartedWhenShutdown,
};

void LogFetchLaterMetric(const FetchLaterBrowserMetricType& type) {
  base::UmaHistogramEnumeration("FetchLater.Browser.Metrics", type);
}

// A ContentSecurityPolicy context for KeepAliveURLLoader.
class KeepAliveURLLoaderCSPContext final : public network::CSPContext {
 public:
  // network::CSPContext override:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) final {
    // TODO(crbug.com/40236167): Support reporting violation w/o renderer.
  }
  void SanitizeDataForUseInCspViolation(
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const final {
    // TODO(crbug.com/40236167): Support reporting violation w/o renderer.
  }
};

// Checks if `url` is allowed by the set of Content-Security-Policy `policies`.
// Violation will not be reported back to renderer, as this function must be
// called after renderer is gone.
// TODO(crbug.com/40263403): Isolated world's CSP is not handled.
bool IsRedirectAllowedByCSP(
    const std::vector<network::mojom::ContentSecurityPolicyPtr>& policies,
    const GURL& url,
    const GURL& url_before_redirects,
    bool has_followed_redirect) {
  // Sets the CSP Directive for fetch() requests. See
  // https://w3c.github.io/webappsec-csp/#directive-connect-src
  // https://fetch.spec.whatwg.org/#destination-table
  auto directive = network::mojom::CSPDirectiveName::ConnectSrc;
  // Sets empty as source location is only used when reporting back to renderer.
  auto empty_source_location = network::mojom::SourceLocation::New();
  auto disposition = network::CSPContext::CheckCSPDisposition::CHECK_ALL_CSP;

  // When reaching here, renderer should have be gone, or at least
  // `KeepAliveURLLoader::forwarding_client_` is disconnected.
  return KeepAliveURLLoaderCSPContext()
      .IsAllowedByCsp(policies, directive, url, url_before_redirects,
                      has_followed_redirect, empty_source_location, disposition,
                      /*is_form_submission=*/false)
      .IsAllowed();
}

bool IsNetErrorEligibleForRetry(int net_error) {
  // Check if the error we encountered is likely transient / can succeed with
  // another attempt.
  return  // Generic transient errors.
      net_error == net::ERR_TIMED_OUT ||
      net_error == net::ERR_CONNECTION_TIMED_OUT ||
      net_error == net::ERR_CONNECTION_CLOSED ||
      net_error == net::ERR_CONNECTION_REFUSED ||
      net_error == net::ERR_CONNECTION_RESET ||
      net_error == net::ERR_CONNECTION_FAILED ||
      net_error == net::ERR_ADDRESS_UNREACHABLE ||
      net_error == net::ERR_NETWORK_CHANGED ||
      // Proxy/tunnel-specific connection issues.
      net_error == net::ERR_TUNNEL_CONNECTION_FAILED ||
      net_error == net::ERR_PROXY_CONNECTION_FAILED ||
      net_error == net::ERR_SOCKS_CONNECTION_FAILED ||
      net_error == net::ERR_HTTP2_PING_FAILED ||
      net_error == net::ERR_HTTP2_PROTOCOL_ERROR ||
      net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
      // DNS failures.
      net_error == net::ERR_NAME_NOT_RESOLVED ||
      net_error == net::ERR_INTERNET_DISCONNECTED ||
      net_error == net::ERR_NAME_RESOLUTION_FAILED;
}

bool IsServerGuaranteedToBeNotReachedYet(int net_error) {
  return net_error == net::ERR_CONNECTION_REFUSED ||
         net_error == net::ERR_ADDRESS_UNREACHABLE ||
         net_error == net::ERR_TUNNEL_CONNECTION_FAILED ||
         net_error == net::ERR_PROXY_CONNECTION_FAILED ||
         net_error == net::ERR_SOCKS_CONNECTION_FAILED ||
         net_error == net::ERR_NAME_NOT_RESOLVED ||
         net_error == net::ERR_NAME_RESOLUTION_FAILED;
}

}  // namespace

// A wrapper class around the target URLLoaderClient connected to Renderer,
// where the owning KeepAliveURLLoader forwards the network loading results to.
class KeepAliveURLLoader::ForwardingClient final
    : public network::mojom::URLLoaderClient {
 public:
  ForwardingClient(
      KeepAliveURLLoader* loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client)
      : keep_alive_url_loader_(std::move(loader)),
        target_(std::move(forwarding_client)) {
    CHECK(keep_alive_url_loader_);
    // For FetchLater requests, `target_` is not bound to renderer.
    if (target_) {
      target_.set_disconnect_handler(base::BindOnce(
          &ForwardingClient::OnDisconnected, base::Unretained(this)));
    }
  }
  // Not copyable.
  ForwardingClient(const ForwardingClient&) = delete;
  ForwardingClient& operator=(const ForwardingClient&) = delete;

  int32_t request_id() const { return keep_alive_url_loader_->request_id_; }
  bool IsConnected() const { return !!target_; }
  void OnDisconnected();

  // network::mojom::URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    TRACE_EVENT("loading",
                "KeepAliveURLLoader::ForwardingClient::OnReceiveEarlyHints",
                "request_id", request_id());

    if (IsConnected()) {
      // The renderer is alive, forwards the action.
      target_->OnReceiveEarlyHints(std::move(early_hints));
      return;
    }
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    TRACE_EVENT("loading",
                "KeepAliveURLLoader::ForwardingClient::OnUploadProgress",
                "request_id", request_id());

    if (IsConnected()) {
      // The renderer is alive, forwards the action.
      target_->OnUploadProgress(current_position, total_size,
                                std::move(callback));
      return;
    }
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    TRACE_EVENT("loading",
                "KeepAliveURLLoader::ForwardingClient::OnTransferSizeUpdated",
                "request_id", request_id());

    if (IsConnected()) {
      // The renderer is alive, forwards the action.
      target_->OnTransferSizeUpdated(transfer_size_diff);
      return;
    }
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(IsConnected());
    target_->OnReceiveRedirect(redirect_info, std::move(head));
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(IsConnected());
    target_->OnReceiveResponse(std::move(head), std::move(body),
                               std::move(cached_metadata));
  }

  void OnComplete(
      const network::URLLoaderCompletionStatus& completion_status) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CHECK(IsConnected());
    target_->OnComplete(completion_status);
  }

 private:
  // Cannot be nullptr as it owns `this`.
  const raw_ptr<KeepAliveURLLoader> keep_alive_url_loader_;
  // The target where overridden the `network::mojom::URLLoaderClient` methods
  // should forward to.
  // Its receiver resides in the Renderer.
  mojo::Remote<network::mojom::URLLoaderClient> target_;
};

// Stores the chain of redriects, response, and completion status, such that
// they can be forwarded to renderer after handled in browser first.
// See also `ForwardURLLoad()`.
struct KeepAliveURLLoader::StoredURLLoad {
  StoredURLLoad() = default;

  // Not copyable.
  StoredURLLoad(const StoredURLLoad&) = delete;
  StoredURLLoad& operator=(const StoredURLLoad&) = delete;

  // Stores data for a redirect received from `OnReceiveRedirect()`.
  struct RedirectData {
    RedirectData(const net::RedirectInfo& redirect_info,
                 network::mojom::URLResponseHeadPtr response_head)
        : info(redirect_info), head(std::move(response_head)) {}
    // Not copyable.
    RedirectData(const RedirectData&) = delete;
    RedirectData& operator=(const RedirectData&) = delete;

    // A copy of the RedirectInfo.
    net::RedirectInfo info;
    // The original URLResponseHead not yet passed to renderer.
    network::mojom::URLResponseHeadPtr head;
  };

  // Stores data for a response received from `OnReceiveResponse()`.
  struct ResponseData {
    ResponseData(network::mojom::URLResponseHeadPtr response_head,
                 mojo::ScopedDataPipeConsumerHandle body_handle,
                 std::optional<mojo_base::BigBuffer> cached_metadata)
        : head(std::move(response_head)),
          body(std::move(body_handle)),
          metadata(std::move(cached_metadata)) {}
    // Not copyable.
    ResponseData(const ResponseData&) = delete;
    ResponseData& operator=(const ResponseData&) = delete;

    // The original URLResponseHead not yet passed to renderer.
    network::mojom::URLResponseHeadPtr head;
    // The original body handle not yet passed to renderer.
    mojo::ScopedDataPipeConsumerHandle body;
    // The original cached metadata not yet passed to renderer.
    std::optional<mojo_base::BigBuffer> metadata;
  };

  // Stores all intermediate redirect data received from `OnReceiveRedirect()`.
  std::queue<std::unique_ptr<RedirectData>> redirects;
  // Stores the response data received from `OnReceiveResponse()` for later use
  // in renderer.
  std::unique_ptr<ResponseData> response = nullptr;
  // Stores the completion status received from `OnComplete()` for later use in
  // renderer.
  std::optional<network::URLLoaderCompletionStatus> completion_status =
      std::nullopt;
  // Tells whether any of the above field has been used (forwarded to renderer).
  bool forwarding = false;
};

KeepAliveURLLoader::KeepAliveURLLoader(
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    scoped_refptr<PolicyContainerHost> policy_container_host,
    WeakDocumentPtr weak_document_ptr,
    net::NetworkIsolationKey network_isolation_key,
    std::optional<ukm::SourceId> ukm_source_id,
    StoragePartitionImpl* storage_partition,
    URLLoaderThrottlesGetter throttles_getter,
    base::PassKey<KeepAliveURLLoaderService>,
    std::unique_ptr<KeepAliveAttributionRequestHelper>
        attribution_request_helper)
    : request_id_(request_id),
      devtools_request_id_(base::UnguessableToken::Create().ToString()),
      options_(options),
      resource_request_(resource_request),
      forwarding_client_(
          std::make_unique<ForwardingClient>(this,
                                             std::move(forwarding_client))),
      traffic_annotation_(net::NetworkTrafficAnnotationTag(traffic_annotation)),
      network_loader_factory_(std::move(network_loader_factory)),
      stored_url_load_(std::make_unique<StoredURLLoad>()),
      policy_container_host_(std::move(policy_container_host)),
      weak_document_ptr_(std::move(weak_document_ptr)),
      network_isolation_key_(network_isolation_key),
      ukm_source_id_(ukm_source_id),
      request_tracker_(
          GetContentClient()->browser()->MaybeCreateKeepAliveRequestTracker(
              resource_request,
              ukm_source_id,
              base::BindRepeating(&KeepAliveURLLoader::IsContextDetached,
                                  // `this` owns `request_tracker_`, so it is
                                  // safe to use.
                                  base::Unretained(this)))),
      storage_partition_(storage_partition),
      initial_url_(resource_request.url),
      last_url_(resource_request.url),
      throttles_getter_(throttles_getter),
      attribution_request_helper_(std::move(attribution_request_helper)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(network_loader_factory_);
  CHECK(policy_container_host_);
  CHECK(!resource_request.trusted_params);
  CHECK(storage_partition_);
  TRACE_EVENT("loading", "KeepAliveURLLoader::KeepAliveURLLoader", "request_id",
              request_id_, "url", last_url_);
  TRACE_EVENT_BEGIN("loading", "KeepAliveURLLoader",
                    perfetto::Track(request_id_), "url", last_url_);

  original_resource_request_ = resource_request_;

  LogFetchKeepAliveRequestMetric("Total");
  if (IsFetchLater()) {
    base::UmaHistogramBoolean("FetchLater.Browser.Total", true);
  }
}

void KeepAliveURLLoader::Start() {
  StartInternal(/*is_retry=*/false);
}

void KeepAliveURLLoader::StartInternal(bool is_retry) {
  CHECK(!is_started_ || is_retry);
  TRACE_EVENT("loading", "KeepAliveURLLoader::Start", "request_id",
              request_id_);
  is_started_ = true;
  if (!is_retry) {
    first_request_start_time_ = base::TimeTicks::Now();
  }

  LogFetchKeepAliveRequestMetric(is_retry ? "Retried" : "Started");
  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::kRequestStarted);
  }
  if (IsFetchLater()) {
    if (is_retry) {
      base::UmaHistogramBoolean("FetchLater.Browser.Total.Retried", true);
    } else {
      base::UmaHistogramBoolean("FetchLater.Browser.Total.Started", true);
    }
    // Logs to DevTools only if the initiator is still alive.
    if (RenderFrameHostImpl* rfh = GetInitiator(); rfh) {
      devtools_instrumentation::OnFetchKeepAliveRequestWillBeSent(
          rfh->frame_tree_node(), devtools_request_id_, resource_request_);
    }
  }

  GetContentClient()->browser()->OnKeepaliveRequestStarted(
      storage_partition_->browser_context());

  // Asks the network service to create a URL loader with passed in params.
  url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      network_loader_factory_, throttles_getter_.Run(), request_id_, options_,
      &resource_request_, forwarding_client_.get(),
      is_retry ? kKeepAliveRetryAnnotationTag : traffic_annotation_,
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      /*cors_exempt_header_list=*/std::nullopt,
      // `this`'s lifetime is at least the same as `url_loader_`.
      /*client_receiver_delegate=*/this);

  // `url_loader_` now re-runs a subset of throttles that have been run
  // in renderer, which is necessary to handle in-browser redirects.
  // There is already a similar use case that also runs throttles in browser in
  // `SearchPrefetchRequest::StartPrefetchRequest()`. The review discussion in
  // https://crrev.com/c/2552723/3 suggests that running them again in browser
  // is fine.
}

KeepAliveURLLoader::~KeepAliveURLLoader() {
  TRACE_EVENT("loading", "KeepAliveURLLoader::~KeepAliveURLLoader",
              "request_id", request_id_);
  // End "KeepAliveURLLoader" trace event.
  TRACE_EVENT_END("loading", perfetto::Track(request_id_));

  // Allows logging to start as early as possible.
  request_tracker_.reset();
  disconnected_loader_timer_.Stop();
  if (IsStarted()) {
    GetContentClient()->browser()->OnKeepaliveRequestFinished();
  }
}

void KeepAliveURLLoader::set_on_delete_callback(
    OnDeleteCallback on_delete_callback) {
  on_delete_callback_ = std::move(on_delete_callback);
}

void KeepAliveURLLoader::set_check_retry_eligibility_callback(
    CheckRetryEligibilityCallback check_retry_eligibility_callback) {
  check_retry_eligibility_callback_ =
      std::move(check_retry_eligibility_callback);
}

void KeepAliveURLLoader::set_on_retry_scheduled_callback(
    OnRetryScheduledCallback on_retry_scheduled_callback) {
  on_retry_scheduled_callback_ = std::move(on_retry_scheduled_callback);
}

base::WeakPtr<KeepAliveURLLoader> KeepAliveURLLoader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool KeepAliveURLLoader::IsStarted() const {
  return is_started_;
}

bool KeepAliveURLLoader::IsAttemptingRetry(bool include_failed_retry) const {
  return retry_state_ != RetryState::kNotAttemptingRetry &&
         (include_failed_retry || retry_state_ != RetryState::kRetryFailed);
}

RenderFrameHostImpl* KeepAliveURLLoader::GetInitiator() const {
  return static_cast<RenderFrameHostImpl*>(
      weak_document_ptr_.AsRenderFrameHostIfValid());
}

bool KeepAliveURLLoader::IsContextDetached() const {
  return !GetInitiator();
}

void KeepAliveURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::FollowRedirect", "request_id",
              request_id_, "url", new_url);

  if (new_url != std::nullopt) {
    mojo::ReportBadMessage(
        "Unexpected `new_url` in KeepAliveURLLoader::FollowRedirect(): "
        "must be null");
    return;
  }

  if (IsRendererConnected()) {
    // Continue forwarding the stored data to renderer.
    // Note: we may or may not have response at this point.
    ForwardURLLoad();
    // DO NOT touch any members after this line. `this` may be already deleted
    // if `OnComplete()` has been triggered.
    return;
  }
  // No need to forward anymore as the target renderer is gone.
  DeleteSelf();
  // DO NOT touch any members after this line. `this` is deleted.
}

void KeepAliveURLLoader::SetPriority(net::RequestPriority priority,
                                     int intra_priority_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::SetPriority", "request_id",
              request_id_);

  // Let `url_loader_` handles how to forward the action to the network service.
  url_loader_->SetPriority(priority, intra_priority_value);
}

void KeepAliveURLLoader::EndReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  did_encounter_redirect_ = true;
  CHECK_GT(redirect_limit_, 0u);
  if (--redirect_limit_ == 0) {
    // Don't process the redirect if we've reached our limit.
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
    return;
  }

  TRACE_EVENT("loading", "KeepAliveURLLoader::EndReceiveRedirect", "request_id",
              request_id_);
  if (request_tracker_) {
    // Note: redirect may have been cancelled by throttle before reaching here.
    request_tracker_->AdvanceToNextStage(
        request_tracker_->GetNextRedirectStageType());
  }

  // Throttles from content-embedder has already been run for this redirect.
  // See also the call sequence from renderer:
  // https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit?pli=1#heading=h.d006i46pmq9

  if (IsFetchLater()) {
    // Logs to DevTools only if the initiator is still alive.
    if (auto* rfh = GetInitiator(); rfh) {
      auto redirect_head_info = network::ExtractDevToolsInfo(*head);
      std::pair<const GURL&, const network::mojom::URLResponseHeadDevToolsInfo&>
          redirect_info_for_devtools{last_url_, *redirect_head_info};
      devtools_instrumentation::OnFetchKeepAliveRequestWillBeSent(
          rfh->frame_tree_node(), devtools_request_id_, resource_request_,
          redirect_info_for_devtools);
    }
  }

  scoped_refptr<net::HttpResponseHeaders> headers = head->headers;

  // Stores the redirect data for later use by renderer.
  stored_url_load_->redirects.emplace(
      std::make_unique<StoredURLLoad::RedirectData>(redirect_info,
                                                    std::move(head)));

  // Runs additional redirect checks before proceeding.
  if (net::Error err = WillFollowRedirect(redirect_info); err != net::OK) {
    OnComplete(network::URLLoaderCompletionStatus(err));
    return;
  }

  if (attribution_request_helper_) {
    attribution_request_helper_->OnReceiveRedirect(headers,
                                                   redirect_info.new_url);
  }

  // TODO(crbug.com/40236167): Figure out how to deal with lost
  // ResourceFetcher's counter & dev console logging (renderer is dead).

  resource_request_.url = redirect_info.new_url;
  resource_request_.site_for_cookies = redirect_info.new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info.new_referrer);
  resource_request_.referrer_policy = redirect_info.new_referrer_policy;
  // Ask the network service to follow the redirect.
  last_url_ = GURL(redirect_info.new_url);
  // TODO(crbug.com/40880984): Remove Authorization header upon cross-origin
  // redirect.
  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnReceiveRedirectProcessed(this);
  }

  // Asks `url_loader_` to directly forward the action to the network service.
  // The modified headers are stored there, if exists.
  //
  // Note: there may be throttles running in IO thread, which may send signals
  // in between `FollowRedirect()` and the next `OnReceiveRedirect()` or
  // `OnReceiveResponse()`.
  url_loader_->FollowRedirect(
      /*removed_headers=*/{}, /*modified_headers=*/{},
      /*modified_cors_exempt_headers=*/{});
}

void KeepAliveURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnReceiveResponse", "request_id",
              request_id_, "url", last_url_);

  LogFetchKeepAliveRequestMetric("Succeeded");
  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::kResponseReceived);
  }

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnReceiveResponse(this);
  }

  if (IsFetchLater()) {
    // Logs to DevTools only if the initiator is still alive.
    if (auto* rfh = GetInitiator(); rfh) {
      devtools_instrumentation::OnFetchKeepAliveResponseReceived(
          rfh->frame_tree_node(), devtools_request_id_, last_url_, *response);
    }
  }

  if (attribution_request_helper_) {
    attribution_request_helper_->OnReceiveResponse(response->headers.get());
    attribution_request_helper_.reset();
  }

  // In case the renderer is alive, the stored response data will be forwarded
  // at the end of `ForwardURLLoad()`.
  stored_url_load_->response = std::make_unique<StoredURLLoad::ResponseData>(
      std::move(response), std::move(body), std::move(cached_metadata));

  if (IsRendererConnected()) {
    // Starts to forward the stored redirects/response to renderer.
    // Note that `OnComplete()` may be triggered in between the forwarding.
    ForwardURLLoad();
    // DO NOT touch any members after this line. `this` may be already deleted
    // if `OnComplete()` has been triggered.
    return;
  }

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnReceiveResponseProcessed(this);
  }

  // When reaching here, the loader counterpart in renderer is already
  // disconnected before receiving response. However, the renderer may still be
  // alive, and waiting for DevTools logger to finish with `OnComplete()`. For
  // examples:
  // 1. Calling fetchLater(url, {activateAfter: 0}) will immediately dispose
  //    renderer loader, which is independent of renderer lifetime and requires
  //    OnComplete().
  // 2. Calling fetch(url, {keepalive: true}) in unload may lead to disconnected
  //    renderer loader + dead renderer, which requires no OnComplete().
  //
  // Only for case 1, we want to wait for a limited time to allow `OnComplete()`
  // to trigger, but also prevents this loader from staying alive indefinitely.
  if (IsFetchLater() && !disconnected_loader_timer_.IsRunning()) {
    disconnected_loader_timer_.Start(
        FROM_HERE, GetDisconnectedKeepAliveURLLoaderTimeout(),
        base::BindOnce(&KeepAliveURLLoader::OnDisconnectedLoaderTimerFired,
                       // `this` owns `disconnected_loader_timer_`.
                       base::Unretained(this)));
    return;
  }
  // Otherwise, no need to wait for `OnComplete()`.
  // This loader should be deleted immediately to avoid hanged requests taking
  // up resources.
  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

void KeepAliveURLLoader::NotifyOnCompleteForTestAndDevTools(
    const network::URLLoaderCompletionStatus& completion_status) {
  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnComplete(this, completion_status);
  }

  if (IsFetchLater()) {
    // Logs to DevTools only if the initiator is still alive.
    if (RenderFrameHostImpl* rfh = GetInitiator(); rfh) {
      devtools_instrumentation::OnFetchKeepAliveRequestComplete(
          rfh->frame_tree_node(), devtools_request_id_, completion_status);
    }
  }
}

void KeepAliveURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnComplete", "request_id",
              request_id_);
  if (IsAttemptingRetry(/*include_failed_retry=*/false)) {
    retry_state_ = RetryState::kRetryFailed;
  }

  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::kLoaderCompleted,
        completion_status);
  }

  if (completion_status.error_code != net::OK) {
    // If the request succeeds, it should've been logged in `OnReceiveResponse`.
    LogFetchKeepAliveRequestMetric("Failed");

    if (RetryOrDelayErrorIfNeeded(
            completion_status,
            base::BindOnce(&KeepAliveURLLoader::OnCompleteInternal,
                           // `this` owns `max_age_handler_timer_`.
                           base::Unretained(this), completion_status))) {
      // Retry or delayed error processing is scheduled. Don't process the
      // cancellation at this time, but still notify test observers and
      // devtools.
      NotifyOnCompleteForTestAndDevTools(completion_status);
      return;
    }
  }

  NotifyOnCompleteForTestAndDevTools(completion_status);
  OnCompleteInternal(completion_status);
}

void KeepAliveURLLoader::OnCompleteInternal(
    const network::URLLoaderCompletionStatus& completion_status) {
  // Note that we don't need to reset the attribution helper if we retry.
  if (completion_status.error_code != net::OK) {
    if (attribution_request_helper_) {
      attribution_request_helper_->OnError();
      attribution_request_helper_.reset();
    }
  }

  // In case the renderer is alive, the stored status will be forwarded
  // at the end of `ForwardURLLoad()`.
  stored_url_load_->completion_status = completion_status;

  if (IsRendererConnected()) {
    if (HasReceivedResponse()) {
      // Do nothing. `completion_status` will be forwarded at the end of
      // `ForwardURLLoad()`.
      return;
    }

    // Either (1) an error happens in between redirect handling in browser, or
    // (2) the redirects and response have all been forwarded.
    // Starts forwarding stored redirects and the completion status to renderer.
    ForwardURLLoad();
    // DO NOT touch any members after this line. `this` is already deleted.
    return;
  }

  // TODO(crbug.com/40236167): Handle in the browser process.
  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnCompleteProcessed(this, completion_status);
  }

  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

size_t KeepAliveURLLoader::GetMaxAttemptsForRetry() const {
  CHECK(resource_request_.fetch_retry_options.has_value());
  return std::min(
      features::kMaxRetryCount.Get(),
      static_cast<size_t>(resource_request_.fetch_retry_options->max_attempts));
}

base::TimeDelta KeepAliveURLLoader::GetMaxAgeForRetry() const {
  CHECK(resource_request_.fetch_retry_options.has_value());
  if (!resource_request_.fetch_retry_options->max_age.has_value()) {
    return features::kMaxRetryAge.Get();
  }

  return std::min(features::kMaxRetryAge.Get(),
                  resource_request_.fetch_retry_options->max_age.value());
}

base::TimeDelta KeepAliveURLLoader::GetInitialTimeDeltaForRetry() const {
  CHECK(resource_request_.fetch_retry_options.has_value());
  if (!resource_request_.fetch_retry_options->initial_delay.has_value()) {
    return features::kMinRetryDelta.Get();
  }
  return std::max(features::kMinRetryDelta.Get(),
                  resource_request_.fetch_retry_options->initial_delay.value());
}

double KeepAliveURLLoader::GetBackoffFactorForRetry() const {
  CHECK(resource_request_.fetch_retry_options.has_value());
  if (!resource_request_.fetch_retry_options->backoff_factor.has_value()) {
    return features::kMinRetryBackoffFactor.Get();
  }
  return std::max(
      features::kMinRetryBackoffFactor.Get(),
      resource_request_.fetch_retry_options->backoff_factor.value());
}

base::TimeDelta KeepAliveURLLoader::UpdateNextRetryDelay() {
  if (last_retry_delay_ == base::TimeDelta()) {
    last_retry_delay_ = GetInitialTimeDeltaForRetry();
  } else {
    last_retry_delay_ *= GetBackoffFactorForRetry();
  }
  return last_retry_delay_;
}

bool KeepAliveURLLoader::IsEligibleForRetry(
    std::optional<network::URLLoaderCompletionStatus> completion_status) const {
  auto retry_options = resource_request_.fetch_retry_options;
  if (!retry_options.has_value()) {
    // The fetch must opt-in to retry.
    return false;
  }

  // Don't retry if it's not HTTPs, there's already a retry scheduled, or we're
  // over the limit.
  if (!resource_request_.url.SchemeIs(url::kHttpsScheme) ||
      retry_state_ == RetryState::kRetryScheduled ||
      retry_state_ == RetryState::kWaitingForSameNetworkIsolationKeyDocument ||
      retry_count_ >= GetMaxAttemptsForRetry() ||
      (first_request_start_time_ != base::TimeTicks() &&
       base::TimeTicks::Now() - first_request_start_time_ >
           GetMaxAgeForRetry())) {
    return false;
  }

  if (!retry_options->retry_non_idempotent &&
      !net::HttpUtil::IsMethodIdempotent(resource_request_.method)) {
    // Don't retry non-idempotent method unless the fetch explicitly opted-in to
    // do so.
    return false;
  }

  if (!retry_options->retry_after_unload && IsContextDetached()) {
    return false;
  }

  CHECK(!check_retry_eligibility_callback_.is_null());
  if (!check_retry_eligibility_callback_.Run()) {
    return false;
  }

  if (HasReceivedResponse()) {
    // If we've previously received and stored a response from the server, we
    // can't hold on to the response pipe for too long because it might congest
    // the network. So, just give up from retrying in this case.
    return false;
  }

  if (!completion_status.has_value()) {
    // No completion status. This can only happen when we hit the renderer
    // disconnect timeout before getting any results. The request should be
    // eligible to retry, except if explicitly opting in to retry only if the
    // server is guaranteed to be not reached yet. We can't guarantee that in
    // this case, because we don't know if the server has been reached yet or
    // not.
    return !retry_options->retry_only_if_server_unreached;
  }

  if (completion_status->resolve_error_info.is_secure_network_error) {
    // Don't retry if the error was a secure DNS network error,
    // since the retry may interfere with the captive portal probe state.
    // TODO(crbug.com/40104002): Explore how to allow retries for secure
    // DNS network errors without interfering with the captive portal
    // probe state.
    return false;
  }

  if (retry_options->retry_only_if_server_unreached) {
    // Only retry in this case if we've never encountered redirect yet (since if
    // we've been redirected, we must have reached the redirector server
    // before), and the error indicates that the server is not reached yet.
    return !did_encounter_redirect_ &&
           IsServerGuaranteedToBeNotReachedYet(completion_status->error_code);
  }

  return IsNetErrorEligibleForRetry(completion_status->error_code);
}

bool KeepAliveURLLoader::MaybeScheduleRetry(
    std::optional<network::URLLoaderCompletionStatus> completion_status) {
  if (!IsEligibleForRetry(completion_status)) {
    return false;
  }

  // We can retry. Reset the URLLoader to avoid scheduling another retry if we
  // received another error signal after this (e.g. OnComplete with error
  // happened, then the disconnection triggers CancelWithStatus).
  url_loader_.reset();

  // Set a timer to delete self when the max age has been reached. Note that
  // we check if the timer is already set here, because it could've been set
  // already by a previous retry attempt (so there's no use to set it again),
  // or it was already set to process an error we encountered in a past attempt
  // (which should still be kept, in case this retry attempt went past max
  // age, at which point we should still send that latest error).
  if (!max_age_handler_timer_.IsRunning()) {
    base::TimeDelta current_age =
        (base::TimeTicks::Now() - first_request_start_time_);
    max_age_handler_timer_.Start(
        FROM_HERE, GetMaxAgeForRetry() - current_age,
        base::BindOnce(&KeepAliveURLLoader::DeleteSelf,
                       // `this` owns `max_age_handler_timer_`.
                       base::Unretained(this)));
  }

  // Update the retry-tracking states. Note that there's no need to reset any
  // of the actual request-related state, since the retry is attempted from the
  // last request attempt, and no state has been updated in response of the
  // failed result yet. All states relating to previous attempts (e.g. stored
  // loads storing previous redirects) only contain results from successful
  // redirects/responses so there's no need to reset.
  retry_count_++;
  CHECK_LE(retry_count_, GetMaxAttemptsForRetry());
  retry_state_ = RetryState::kRetryScheduled;

  // Schedule the retry.
  retry_timer_.Start(FROM_HERE, UpdateNextRetryDelay(),
                     base::BindOnce(&KeepAliveURLLoader::AttemptRetryIfAllowed,
                                    base::Unretained(this)));

  CHECK(!on_retry_scheduled_callback_.is_null());
  on_retry_scheduled_callback_.Run();

  return true;
}

void KeepAliveURLLoader::AttemptRetryIfAllowed() {
  if (retry_state_ == RetryState::kRetryFailed) {
    return;
  }
  CHECK(retry_state_ == RetryState::kRetryScheduled ||
        retry_state_ == RetryState::kWaitingForSameNetworkIsolationKeyDocument);
  // Don't retry when there's no active document with a same network isolation
  // key as the initiator of the load, to avoid privacy concerns of revealing
  // information about the user (that their browser is up, and their current
  // IP address) to the destination origin, while there is no active document.
  if (!storage_partition_->GetActiveDocumentCount(network_isolation_key_)) {
    // No active document with the same NetworkIsolationKey exists. Wait until
    // we see such a document, or delete ourselves when we can't attempt a retry
    // anymore (we reached the max age of retries, which will run self-deletion
    // when we first scheduled the retry attempt).
    retry_state_ = RetryState::kWaitingForSameNetworkIsolationKeyDocument;
    return;
  }

  // TODO(crbug.com/417930271): Check if we're offline, and if so, wait until
  // we're online before attempting.
  retry_state_ = RetryState::kRetryInProgress;
  // Reset the request IDs.
  request_id_ = GlobalRequestID::MakeBrowserInitiated().request_id;
  devtools_request_id_ = base::UnguessableToken::Create().ToString();

  // Retry using the original request, even if the failure happens after
  // redirects.
  resource_request_ = original_resource_request_;
  if (features::kAddRetryHeader.Get()) {
    // Add retry information in the header.
    resource_request_.headers.SetHeader(kRetryAttemptsHeader,
                                        base::NumberToString(retry_count_));
  }

  // TODO(crbug.com/417930271): Track the retry as a state in the
  // KeepAliveRequestTracker too.
  request_tracker_ =
      GetContentClient()->browser()->MaybeCreateKeepAliveRequestTracker(
          resource_request_, ukm_source_id_,
          base::BindRepeating(&KeepAliveURLLoader::IsContextDetached,
                              // `this` owns `request_tracker_`, so it is
                              // safe to use.
                              base::Unretained(this)));

  StartInternal(/*is_retry=*/true);
}

void KeepAliveURLLoader::DidObserveNewlyActiveDocumentWithNIK(
    const net::NetworkIsolationKey& nik) {
  if (nik == network_isolation_key_ &&
      retry_state_ == RetryState::kWaitingForSameNetworkIsolationKeyDocument) {
    // We previously wanted to retry but couldn't due to there being no active
    // document with the same Network Isolation Key. Now that we observe such a
    // document, we can attempt the retry.
    retry_state_ = RetryState::kRetryScheduled;
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&KeepAliveURLLoader::AttemptRetryIfAllowed,
                                  base::Unretained(this)));
  }
}

bool KeepAliveURLLoader::HasReceivedResponse() const {
  return stored_url_load_ && stored_url_load_->response != nullptr;
}

void KeepAliveURLLoader::ForwardURLLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsRendererConnected());
  CHECK(stored_url_load_);

  // Don't delete self if we need to forward response to the renderer. This is
  // ok since it's either not an error, or we've already reached max age and are
  // forwarding the errors.
  max_age_handler_timer_.Stop();

  // Forwards the redirects/response/completion in the exact sequence.
  stored_url_load_->forwarding = true;

  if (!stored_url_load_->redirects.empty()) {
    // All redirects have been handled in the browser. However, redirects must
    // also be processed by the renderer so that it knows what URL the
    // response come from when parsing the response.
    //
    // Note: The renderer might get shut down before
    // `forwarding_client_->OnReceiveRedirect()` finish all redirect handling.
    // In such case, the handling will be taken over by browser from
    // `OnRendererConnectionError()`.
    forwarding_client_->OnReceiveRedirect(
        stored_url_load_->redirects.front()->info,
        std::move(stored_url_load_->redirects.front()->head));
    stored_url_load_->redirects.pop();

    if (observer_for_testing_) {
      CHECK_IS_TEST();
      observer_for_testing_->OnReceiveRedirectForwarded(this);
    }
    // The rest of `stored_url_load_->redirects` will be forwarded in the next
    // visit to this method when `FollowRedirect()` is called by the renderer.
    return;
  }

  if (stored_url_load_->response) {
    // Note: The receiver may fail to finish reading the entire
    // `stored_url_load_->response`response`, so response caching is not
    // guaranteed.
    // Note: The renderer might get shut down before
    // `forwarding_client_->OnReceiveResponse()` finish response handling.
    // In such case, the attributionsrc handling cannot be dropped and should be
    // taken over by browser in `OnRendererConnectionError().
    forwarding_client_->OnReceiveResponse(
        std::move(stored_url_load_->response->head),
        std::move(stored_url_load_->response->body),
        std::move(stored_url_load_->response->metadata));
    stored_url_load_->response = nullptr;

    if (observer_for_testing_) {
      CHECK_IS_TEST();
      observer_for_testing_->OnReceiveResponseForwarded(this);
    }
  }

  if (stored_url_load_->completion_status.has_value()) {
    forwarding_client_->OnComplete(*(stored_url_load_->completion_status));
    if (observer_for_testing_) {
      CHECK_IS_TEST();
      observer_for_testing_->OnCompleteForwarded(
          this, *(stored_url_load_->completion_status));
    }
    stored_url_load_ = nullptr;

    DeleteSelf();
    // DO NOT touch any members after this line. `this` is already deleted.
  }
}

bool KeepAliveURLLoader::IsForwardURLLoadStarted() const {
  return stored_url_load_ && stored_url_load_->forwarding;
}

bool KeepAliveURLLoader::IsRendererConnected() const {
  CHECK(forwarding_client_);
  return forwarding_client_->IsConnected();
}

net::Error KeepAliveURLLoader::WillFollowRedirect(
    const net::RedirectInfo& redirect_info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40236167): Add logic to handle redirecting to extensions
  // from `ChromeContentRendererClient::IsSafeRedirectTarget()`.
  if (!IsSafeRedirectTarget(last_url_, redirect_info.new_url)) {
    return net::ERR_UNSAFE_REDIRECT;
  }

  if (resource_request_.redirect_mode == network::mojom::RedirectMode::kError) {
    return net::ERR_FAILED;
  }

  if (resource_request_.redirect_mode !=
      network::mojom::RedirectMode::kManual) {
    // Checks if redirecting to `redirect_info.new_url` is allowed by
    // ContentSecurityPolicy from the request initiator document.
    if (!IsRedirectAllowedByCSP(
            policy_container_host_->policies().content_security_policies,
            redirect_info.new_url, initial_url_, last_url_ != initial_url_)) {
      return net::ERR_BLOCKED_BY_CSP;
    }

    // Checks if redirecting to `redirect_info.new_url` is allowed by
    // MixedContent checker.
    // TODO(crbug.com/40941240): Figure out how to check without a frame.
    if (auto* rfh = GetInitiator();
        rfh && MixedContentChecker::ShouldBlockFetchKeepAlive(
                   rfh, redirect_info.new_url,
                   /*for_redirect=*/true)) {
      return net::ERR_FAILED;
    }
  }

  return net::OK;
}

bool KeepAliveURLLoader::RetryOrDelayErrorIfNeeded(
    const network::URLLoaderCompletionStatus& status,
    base::OnceClosure closure) {
  auto retry_options = resource_request_.fetch_retry_options;
  if (!retry_options.has_value()) {
    // Ignore fetches that do not opt-in to retry.
    return false;
  }

  // Schedule retry if needed.
  if (MaybeScheduleRetry(status)) {
    return true;
  }

  base::TimeDelta current_age =
      (base::TimeTicks::Now() - first_request_start_time_);
  if (IsRendererConnected() && current_age < GetMaxAgeForRetry()) {
    // A retry is not attempted, but we can only notify the renderer about the
    // error when we reach the max age, to avoid exposing information about the
    // error through timing. Note that we only do this when the renderer is
    // still connected and waiting for the error info. If the renderer is
    // already disconnected, we can just continue processing the error and free
    // up resources by deleting ourself. Note also that this will replace the
    // previous action set in the timer (which is either to send the error from
    // a previous attempt, or to delete self, which should not take precedent
    // over this).
    max_age_handler_timer_.Start(FROM_HERE, GetMaxAgeForRetry() - current_age,
                                 std::move(closure));

    // Reset the URLLoader to avoid receiving another error signal after this
    // (e.g. OnComplete with error happened, then the disconnection triggers
    // CancelWithStatus).
    url_loader_.reset();
    return true;
  }

  return false;
}

void KeepAliveURLLoader::CancelWithStatus(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::CancelWithStatus", "request_id",
              request_id_);
  if (IsAttemptingRetry(/*include_failed_retry=*/false)) {
    retry_state_ = RetryState::kRetryFailed;
  }

  if (!stored_url_load_->completion_status.has_value()) {
    // Only logs if there is no error logged by `OnComplete()` yet.
    LogFetchKeepAliveRequestMetric("Failed");
  }

  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::kRequestFailed, status);
  }

  if (RetryOrDelayErrorIfNeeded(
          status, base::BindOnce(&KeepAliveURLLoader::CancelWithStatusInternal,
                                 // `this` owns `max_age_handler_timer_`.
                                 base::Unretained(this), status))) {
    // Retry or delayed error processing is scheduled. Don't process the
    // cancellation at this time.
    return;
  }

  CancelWithStatusInternal(status);
}

void KeepAliveURLLoader::CancelWithStatusInternal(
    const network::URLLoaderCompletionStatus& status) {
  // This method can be triggered when one of the followings happen:
  // 1. Network -> `url_loader_` gets disconnected.
  // 2. `url_loader_` gets cancelled by throttles.
  // 3. `url_loader_` terminates itself.

  if (IsRendererConnected()) {
    if (!IsForwardURLLoadStarted()) {
      // The loader is cancelled before this loader forwards anything to
      // renderer. It should make an ateempt to forward any previous loads.
      ForwardURLLoad();
      // DO NOT touch any members after this line. `this` may be deleted.
      return;
    }
    // Otherwise, let `ForwardURLLoad()` continue forwarding the rest.
    return;
  }

  // We should let the renderer know it's closed by deleting `this`.
  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

// Browser -> Renderer connection
void KeepAliveURLLoader::ForwardingClient::OnDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::ForwardingClient::OnDisconnected",
              "request_id", request_id());

  // Dropping the client as renderer is gone.
  target_.reset();

  if (!keep_alive_url_loader_->IsForwardURLLoadStarted() &&
      !keep_alive_url_loader_->HasReceivedResponse()) {
    // The renderer disconnects before this loader forwards anything to it.
    // But the in-browser request processing may not complete yet.

    // TODO(crbug.com/40259706): Ensure that attributionsrc response handling is
    // taken over by browser.
    return;
  }

  // Renderer disconnects in-between forwarding, no need to call
  // `ForwardURLLoad()` anymore.
  keep_alive_url_loader_->DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

// Browser <- Renderer connection.
void KeepAliveURLLoader::OnURLLoaderDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnURLLoaderDisconnected",
              "request_id", request_id_);
  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::
            kLoaderDisconnectedFromRenderer);
  }
  if (!IsStarted()) {
    // May be the last chance to start a deferred loader.
    LogFetchLaterMetric(
        FetchLaterBrowserMetricType::kStartedAfterInitiatorDisconnected);
    Start();
  }
  // For other types of keepalive requests, this loader does not care about
  // whether messages can be received from renderer or not.

  // Prevents this loader from staying alive indefinitely.
  if (!disconnected_loader_timer_.IsRunning()) {
    disconnected_loader_timer_.Start(
        FROM_HERE, GetDisconnectedKeepAliveURLLoaderTimeout(),
        base::BindOnce(&KeepAliveURLLoader::OnDisconnectedLoaderTimerFired,
                       // `this` owns `disconnected_loader_timer_`.
                       base::Unretained(this)));
  }
}

void KeepAliveURLLoader::OnDisconnectedLoaderTimerFired() {
  if (IsAttemptingRetry(/*include_failed_retry=*/false)) {
    retry_state_ = RetryState::kRetryFailed;
  }
  if (resource_request_.fetch_retry_options.has_value() &&
      resource_request_.fetch_retry_options->retry_after_unload &&
      (IsAttemptingRetry(/*include_failed_retry=*/false) ||
       MaybeScheduleRetry(/*completion_status=*/std::nullopt))) {
    // A retry is already pending or we just scheduled a retry. Don't delete
    // the loader, and instead keep it around for the retry.
    return;
  }

  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::
            kRequestCancelledAfterTimeLimit);
  }
  if (IsFetchLater()) {
    LogFetchLaterMetric(FetchLaterBrowserMetricType::kCancelledAfterTimeLimit);
  }
  DeleteSelf();
}

void KeepAliveURLLoader::Shutdown() {
  base::UmaHistogramBoolean(
      "FetchKeepAlive.Requests2.Shutdown.IsStarted.Browser", IsStarted());
  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::kBrowserShutdown);
  }
  if (!IsStarted()) {
    CHECK(IsFetchLater());
    LogFetchLaterMetric(FetchLaterBrowserMetricType::kStartedWhenShutdown);
    // At this point, browser is shutting down, and renderer termination has not
    // reached browser. It is the last chance to start loading the request from
    // here.
    Start();
  }
}

bool KeepAliveURLLoader::IsFetchLater() const {
  return base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI) &&
         resource_request_.is_fetch_later_api;
}

void KeepAliveURLLoader::SendNow() {
  if (!IsFetchLater()) {
    mojo::ReportBadMessage("Unexpected call to KeepAliveURLLoader::SendNow()");
    return;
  }
  LogFetchLaterMetric(FetchLaterBrowserMetricType::kStartedByInitiator);
  if (!IsStarted()) {
    Start();
  }
}

void KeepAliveURLLoader::Cancel() {
  if (!IsFetchLater()) {
    mojo::ReportBadMessage("Unexpected call to KeepAliveURLLoader::Cancel()");
    return;
  }
  if (request_tracker_) {
    request_tracker_->AdvanceToNextStage(
        KeepAliveRequestTracker::RequestStageType::kRequestCancelledByRenderer);
  }
  LogFetchLaterMetric(FetchLaterBrowserMetricType::kAbortedByInitiator);
  DeleteSelf();
}

void KeepAliveURLLoader::DeleteSelf() {
  CHECK(on_delete_callback_);
  std::move(on_delete_callback_).Run();
}

void KeepAliveURLLoader::SetObserverForTesting(
    scoped_refptr<TestObserver> observer) {
  observer_for_testing_ = observer;
}

void KeepAliveURLLoader::LogFetchKeepAliveRequestMetric(
    std::string_view request_state_name) {
  if (IsFetchLater()) {
    return;
  }

  auto resource_type =
      static_cast<blink::mojom::ResourceType>(resource_request_.resource_type);
  FetchKeepAliveRequestMetricType sample_type;
  // See also blink::PopulateResourceRequest().
  switch (resource_type) {
    case blink::mojom::ResourceType::kXhr:
      sample_type = FetchKeepAliveRequestMetricType::kFetch;
      break;
    // Includes BEACON/PING/ATTRIBUTION_SRC types
    case blink::mojom::ResourceType::kPing:
      sample_type = FetchKeepAliveRequestMetricType::kPing;
      break;
    case blink::mojom::ResourceType::kCspReport:
      sample_type = FetchKeepAliveRequestMetricType::kReporting;
      break;
    case blink::mojom::ResourceType::kImage:
      sample_type = FetchKeepAliveRequestMetricType::kBackgroundFetchIcon;
      break;
    case blink::mojom::ResourceType::kMainFrame:
    case blink::mojom::ResourceType::kSubFrame:
    case blink::mojom::ResourceType::kStylesheet:
    case blink::mojom::ResourceType::kScript:
    case blink::mojom::ResourceType::kFontResource:
    case blink::mojom::ResourceType::kSubResource:
    case blink::mojom::ResourceType::kObject:
    case blink::mojom::ResourceType::kMedia:
    case blink::mojom::ResourceType::kWorker:
    case blink::mojom::ResourceType::kSharedWorker:
    case blink::mojom::ResourceType::kPrefetch:
    case blink::mojom::ResourceType::kFavicon:
    case blink::mojom::ResourceType::kServiceWorker:
    case blink::mojom::ResourceType::kPluginResource:
    case blink::mojom::ResourceType::kNavigationPreloadMainFrame:
    case blink::mojom::ResourceType::kNavigationPreloadSubFrame:
    case blink::mojom::ResourceType::kJson:
      NOTREACHED();
  }

  CHECK(request_state_name == "Total" || request_state_name == "Started" ||
        request_state_name == "Retried" || request_state_name == "Succeeded" ||
        request_state_name == "Failed");

  const std::string histogram_name = base::StrCat(
      {"FetchKeepAlive.Requests2.", request_state_name, ".Browser"});

  // When under the experiment keep a local cache of resolved histograms to
  // avoid contention on the global lock that is taken by histogram functions.
  if (base::features::IsReducePPMsEnabled()) {
    static base::NoDestructor<
        absl::flat_hash_map<std::string, base::HistogramBase*>>
        histograms;

    // Verify that `histograms` is not read/modified by more than one thread.
    // Since it's static it will be used by any code that calls into the
    // function.
    static WrappedThreadChecker* thread_checker = new WrappedThreadChecker;
    thread_checker->Check();

    auto it = histograms->find(histogram_name);
    if (it != histograms->end()) {
      it->second->Add(static_cast<int32_t>(sample_type));
    } else {
      // TODO(crbug.com/424432184): This is messy and leaks information from
      // LinearHistogram. If the experiment succeeds implement
      // GetUmaHistogramEnumerationFactory before cleaning up the flag.
      int32_t max_value =
          static_cast<int32_t>(FetchKeepAliveRequestMetricType::kMaxValue);
      base::HistogramBase* histo = base::LinearHistogram::FactoryGet(
          histogram_name, /*minimum=*/1,
          /*maximum=*/max_value + 1,
          /*bucket_count=*/max_value + 2,
          base::HistogramBase::kUmaTargetedHistogramFlag);
      histo->Add(static_cast<int32_t>(sample_type));

      (*histograms)[histogram_name] = histo;
    }
  } else {
    base::UmaHistogramEnumeration(histogram_name, sample_type);
  }

  if (bool is_context_detached = !GetInitiator();
      request_state_name == "Started" || request_state_name == "Succeeded") {
    base::UmaHistogramBoolean(
        base::StrCat({"FetchKeepAlive.Requests2.", request_state_name,
                      ".IsContextDetached.Browser"}),
        is_context_detached);
  }
}

}  // namespace content
