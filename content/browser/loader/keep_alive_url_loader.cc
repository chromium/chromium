// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/keep_alive_attribution_request_helper.h"
#include "content/browser/renderer_host/mixed_content_checker.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_request.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

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
    BrowserContext* browser_context,
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
      traffic_annotation_(traffic_annotation),
      network_loader_factory_(std::move(network_loader_factory)),
      stored_url_load_(std::make_unique<StoredURLLoad>()),
      policy_container_host_(std::move(policy_container_host)),
      weak_document_ptr_(std::move(weak_document_ptr)),
      browser_context_(browser_context),
      initial_url_(resource_request.url),
      last_url_(resource_request.url),
      throttles_getter_(throttles_getter),
      attribution_request_helper_(std::move(attribution_request_helper)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(network_loader_factory_);
  CHECK(policy_container_host_);
  CHECK(!resource_request.trusted_params);
  CHECK(browser_context_);
  TRACE_EVENT("loading", "KeepAliveURLLoader::KeepAliveURLLoader", "request_id",
              request_id_, "url", last_url_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("loading", "KeepAliveURLLoader",
                                    request_id_, "url", last_url_);
  LogFetchKeepAliveRequestMetric("Total");
  if (IsFetchLater()) {
    base::UmaHistogramBoolean("FetchLater.Browser.Total", true);
  }
}

void KeepAliveURLLoader::Start() {
  CHECK(!is_started_);
  TRACE_EVENT("loading", "KeepAliveURLLoader::Start", "request_id",
              request_id_);
  is_started_ = true;

  LogFetchKeepAliveRequestMetric("Started");
  if (IsFetchLater()) {
    base::UmaHistogramBoolean("FetchLater.Browser.Total.Started", true);
    // Logs to DevTools only if the initiator is still alive.
    if (auto* rfh = GetInitiator(); rfh) {
      devtools_instrumentation::OnFetchKeepAliveRequestWillBeSent(
          rfh->frame_tree_node(), devtools_request_id_, resource_request_);
    }
  }

  GetContentClient()->browser()->OnKeepaliveRequestStarted(browser_context_);

  // Asks the network service to create a URL loader with passed in params.
  url_loader_ = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      network_loader_factory_, throttles_getter_.Run(), request_id_, options_,
      &resource_request_, forwarding_client_.get(),
      net::NetworkTrafficAnnotationTag(traffic_annotation_),
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
  TRACE_EVENT_NESTABLE_ASYNC_END0("loading", "KeepAliveURLLoader", request_id_);

  disconnected_loader_timer_.Stop();
  if (IsStarted()) {
    GetContentClient()->browser()->OnKeepaliveRequestFinished();
  }
}

void KeepAliveURLLoader::set_on_delete_callback(
    OnDeleteCallback on_delete_callback) {
  on_delete_callback_ = std::move(on_delete_callback);
}

base::WeakPtr<KeepAliveURLLoader> KeepAliveURLLoader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool KeepAliveURLLoader::IsStarted() const {
  return is_started_;
}

RenderFrameHostImpl* KeepAliveURLLoader::GetInitiator() const {
  return static_cast<RenderFrameHostImpl*>(
      weak_document_ptr_.AsRenderFrameHostIfValid());
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

void KeepAliveURLLoader::PauseReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::PauseReadingBodyFromNet",
              "request_id", request_id_);
  if (HasReceivedResponse()) {
    // This may come from a renderer that tries to process a redirect which has
    // been previously handled in this loader.
    return;
  }

  // Let `url_loader_` handles how to forward the action to the network service.
  url_loader_->PauseReadingBodyFromNet();

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->PauseReadingBodyFromNetProcessed(this);
  }
}

// TODO(crbug.com/40236167): Add test coverage.
void KeepAliveURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::ResumeReadingBodyFromNet",
              "request_id", request_id_);
  if (HasReceivedResponse()) {
    // This may come from a renderer that tries to process a redirect which has
    // been previously handled in this loader.
    return;
  }

  // Let `url_loader_` handles how to forward the action to the network service.
  url_loader_->ResumeReadingBodyFromNet();

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->ResumeReadingBodyFromNetProcessed(this);
  }
}

void KeepAliveURLLoader::EndReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::EndReceiveRedirect", "request_id",
              request_id_);

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

  if (attribution_request_helper_) {
    attribution_request_helper_->OnReceiveRedirect(head->headers.get(),
                                                   redirect_info.new_url);
  }

  // Stores the redirect data for later use by renderer.
  stored_url_load_->redirects.emplace(
      std::make_unique<StoredURLLoad::RedirectData>(redirect_info,
                                                    std::move(head)));

  // Runs additional redirect checks before proceeding.
  if (net::Error err = WillFollowRedirect(redirect_info); err != net::OK) {
    OnComplete(network::URLLoaderCompletionStatus(err));
    return;
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

void KeepAliveURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnComplete", "request_id",
              request_id_);

  if (completion_status.error_code != net::OK) {
    // If the request succeeds, it should've been logged in `OnReceiveResponse`.
    LogFetchKeepAliveRequestMetric("Failed");
  }

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnComplete(this, completion_status);
  }

  if (IsFetchLater()) {
    // Logs to DevTools only if the initiator is still alive.
    if (auto* rfh = GetInitiator(); rfh) {
      devtools_instrumentation::OnFetchKeepAliveRequestComplete(
          rfh->frame_tree_node(), devtools_request_id_, completion_status);
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

bool KeepAliveURLLoader::HasReceivedResponse() const {
  return stored_url_load_ && stored_url_load_->response != nullptr;
}

void KeepAliveURLLoader::ForwardURLLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsRendererConnected());
  CHECK(stored_url_load_);

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

void KeepAliveURLLoader::CancelWithStatus(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::CancelWithStatus", "request_id",
              request_id_);
  if (!stored_url_load_->completion_status.has_value()) {
    // Only logs if there is no error logged by `OnComplete()` yet.
    LogFetchKeepAliveRequestMetric("Failed");
  }

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
  if (IsFetchLater()) {
    LogFetchLaterMetric(FetchLaterBrowserMetricType::kCancelledAfterTimeLimit);
  }
  DeleteSelf();
}

void KeepAliveURLLoader::Shutdown() {
  base::UmaHistogramBoolean(
      "FetchKeepAlive.Requests2.Shutdown.IsStarted.Browser", IsStarted());
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
        request_state_name == "Succeeded" || request_state_name == "Failed");

  base::UmaHistogramEnumeration(base::StrCat({"FetchKeepAlive.Requests2.",
                                              request_state_name, ".Browser"}),
                                sample_type);
  if (bool is_context_detached = !GetInitiator();
      request_state_name == "Started" || request_state_name == "Succeeded") {
    base::UmaHistogramBoolean(
        base::StrCat({"FetchKeepAlive.Requests2.", request_state_name,
                      ".IsContextDetached.Browser"}),
        is_context_detached);
  }
}

}  // namespace content
