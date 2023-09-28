// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader.h"

#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/common/url_utils.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_context.h"
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
// Must remain in sync with FetchKeepAliveBrowserMetricType in
// tools/metrics/histograms/enums.xml.
enum class FetchKeepAliveBrowserMetricType {
  kLoadingSuceeded = 0,
  kLoadingFailed = 1,
  kForwardingCompleted = 2,
  kCancelledAfterTimeLimit = 3,
  kAbortedByInitiator = 4,
  kMaxValue = kAbortedByInitiator,
};

void LogFetchKeepAliveMetric(const FetchKeepAliveBrowserMetricType& type) {
  base::UmaHistogramEnumeration("FetchKeepAlive.Browser.Metrics", type);
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
  kMaxValue = kCancelledAfterTimeLimit,
};

void LogFetchLaterMetric(const FetchLaterBrowserMetricType& type) {
  base::UmaHistogramEnumeration("FetchLater.Browser.Metrics", type);
}

// A convenient holder to aggregate modified header fields for redirect.
struct ModifiedHeaders {
  ModifiedHeaders() = default;
  ~ModifiedHeaders() = default;
  // Not copyable.
  ModifiedHeaders(const ModifiedHeaders&) = delete;
  ModifiedHeaders& operator=(const ModifiedHeaders&) = delete;

  void MergeFrom(const ModifiedHeaders& other) {
    for (auto& other_removed_header : other.removed_headers) {
      if (!base::Contains(removed_headers, other_removed_header)) {
        removed_headers.emplace_back(std::move(other_removed_header));
      }
    }
    modified_headers.MergeFrom(other.modified_headers);
    modified_cors_exempt_headers.MergeFrom(other.modified_cors_exempt_headers);
  }

  std::vector<std::string> removed_headers;
  net::HttpRequestHeaders modified_headers;
  net::HttpRequestHeaders modified_cors_exempt_headers;
};

// A ContentSecurityPolicy context for KeepAliveURLLoader.
class KeepAliveURLLoaderCSPContext final : public network::CSPContext {
 public:
  // network::CSPContext override:
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation_params) final {
    // TODO(crbug.com/1356128): Support reporting violation w/o renderer.
  }
  void SanitizeDataForUseInCspViolation(
      network::mojom::CSPDirectiveName directive,
      GURL* blocked_url,
      network::mojom::SourceLocation* source_location) const final {
    // TODO(crbug.com/1356128): Support reporting violation w/o renderer.
  }
};

// Checks if `url` is allowed by the set of Content-Security-Policy `policies`.
// Violation will not be reported back to renderer, as this function must be
// called after renderer is gone.
// TODO(crbug.com/1431165): Isolated world's CSP is not handled.
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
  return KeepAliveURLLoaderCSPContext().IsAllowedByCsp(
      policies, directive, url, url_before_redirects, has_followed_redirect,
      /*is_response_check=*/false, empty_source_location, disposition,
      /*is_form_submission=*/false);
}

}  // namespace

// A custom `blink::URLLoaderThrottle` delegate that only handles relevant
// actions.
//
// Note that a delegate may be called from a throttle asynchronously in a
// different thread, e.g. `safe_browsing::BrowserURLLoaderThrottle` runs in IO
// thread http://crbug.com/1057253.
//
// Throttles calling these methods must not be destroyed synchronously.
class KeepAliveURLLoader::ThrottleDelegate final
    : public blink::URLLoaderThrottle::Delegate {
 public:
  explicit ThrottleDelegate(base::WeakPtr<KeepAliveURLLoader> loader)
      : loader_(std::move(loader)),
        loader_weak_ptr_factory_(
            std::make_unique<base::WeakPtrFactory<KeepAliveURLLoader>>(
                loader_.get())) {}
  // Not copyable.
  ThrottleDelegate(const ThrottleDelegate&) = delete;
  ThrottleDelegate& operator=(const ThrottleDelegate&) = delete;

  // blink::URLLoaderThrottle::Delegate overrides:
  // Asks `loader_` to abort itself asynchronously.
  void CancelWithError(int error, base::StringPiece custom_reason) override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&KeepAliveURLLoader::OnComplete,
                                    loader_weak_ptr_factory_->GetWeakPtr(),
                                    network::URLLoaderCompletionStatus(error)));
      return;
    }
    if (IsLoaderAliveOnUI()) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&KeepAliveURLLoader::OnComplete, loader_->GetWeakPtr(),
                         network::URLLoaderCompletionStatus(error)));
    }
  }
  void Resume() override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&KeepAliveURLLoader::ResumeReadingBodyFromNet,
                         loader_weak_ptr_factory_->GetWeakPtr()));
      return;
    }
    if (IsLoaderAliveOnUI()) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&KeepAliveURLLoader::ResumeReadingBodyFromNet,
                         loader_->GetWeakPtr()));
    }
  }
  void PauseReadingBodyFromNet() override {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&KeepAliveURLLoader::PauseReadingBodyFromNet,
                         loader_weak_ptr_factory_->GetWeakPtr()));
      return;
    }
    if (IsLoaderAliveOnUI()) {
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&KeepAliveURLLoader::PauseReadingBodyFromNet,
                         loader_->GetWeakPtr()));
    }
  }
  void RestartWithFlags(int additional_load_flags) override { NOTREACHED(); }
  void RestartWithURLResetAndFlags(int additional_load_flags) override {
    NOTREACHED();
  }

 private:
  // `loader_` is alive and ready to take actions triggered from in-browser
  // throttle, i.e. `loader_` is disconnected from renderer. Otherwise, returns
  // false to avoid early termination when a copy of the same throttle will also
  // be executed in renderer.
  // Must be called on UI thread.
  bool IsLoaderAliveOnUI() const {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return loader_ && !loader_->IsRendererConnected();
  }

  base::WeakPtr<KeepAliveURLLoader> loader_;
  // `loader_` lives in UI thread. This factory helps verify if `loader_` is
  // still available from other threads.
  std::unique_ptr<base::WeakPtrFactory<KeepAliveURLLoader>>
      loader_weak_ptr_factory_;
};

// Maintains a `blink::URLLoaderThrottle` and its delegate's lifetime.
class KeepAliveURLLoader::ThrottleEntry {
 public:
  ThrottleEntry(base::WeakPtr<KeepAliveURLLoader> loader,
                std::unique_ptr<blink::URLLoaderThrottle> loader_throttle)
      : delegate_(std::make_unique<ThrottleDelegate>(std::move(loader))),
        throttle_(std::move(loader_throttle)) {
    CHECK(delegate_);
    CHECK(throttle_);
    throttle_->set_delegate(delegate_.get());
  }
  ~ThrottleEntry() {
    // Both `delegate_` and `throttle_` are about to be destroyed, but
    // `throttle_` may refer to `delegate_` in its dtor. Hence, clear the
    // pointer from `throttle_` to avoid any UAF.
    throttle_->set_delegate(nullptr);
  }
  // Not copyable.
  ThrottleEntry(const ThrottleEntry&) = delete;
  ThrottleEntry& operator=(const ThrottleEntry&) = delete;

  blink::URLLoaderThrottle& throttle() { return *throttle_; }

 private:
  // `delegate_` must live longer than `throttle_`.
  std::unique_ptr<ThrottleDelegate> delegate_;
  std::unique_ptr<blink::URLLoaderThrottle> throttle_;
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
                 absl::optional<mojo_base::BigBuffer> cached_metadata)
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
    absl::optional<mojo_base::BigBuffer> metadata;
  };

  // Stores all intermediate redirect data received from `OnReceiveRedirect()`.
  std::queue<std::unique_ptr<RedirectData>> redirects;
  // Stores the response data received from `OnReceiveResponse()` for later use
  // in renderer.
  std::unique_ptr<ResponseData> response = nullptr;
  // Stores the completion status received from `OnComplete()` for later use in
  // renderer.
  absl::optional<network::URLLoaderCompletionStatus> completion_status =
      absl::nullopt;
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
    BrowserContext* browser_context,
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles,
    base::PassKey<KeepAliveURLLoaderService>)
    : request_id_(request_id),
      options_(options),
      resource_request_(resource_request),
      forwarding_client_(std::move(forwarding_client)),
      traffic_annotation_(traffic_annotation),
      network_loader_factory_(std::move(network_loader_factory)),
      stored_url_load_(std::make_unique<StoredURLLoad>()),
      policy_container_host_(std::move(policy_container_host)),
      browser_context_(browser_context),
      initial_url_(resource_request.url),
      last_url_(resource_request.url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(network_loader_factory_);
  CHECK(policy_container_host_);
  CHECK(!resource_request.trusted_params);
  CHECK(browser_context_);
  TRACE_EVENT("loading", "KeepAliveURLLoader::KeepAliveURLLoader", "request_id",
              request_id_, "url", last_url_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("loading", "KeepAliveURLLoader",
                                    request_id_, "url", last_url_);
  if (IsFetchLater()) {
    base::UmaHistogramBoolean("FetchLater.Browser.Total", true);
  }
  base::UmaHistogramBoolean("FetchKeepAlive.Browser.Total", true);

  // TODO(crbug.com/1356128): Replace custom throttle logic here with blink's.
  for (auto& content_throttle : throttles) {
    throttle_entries_.emplace_back(std::make_unique<ThrottleEntry>(
        GetWeakPtr(), std::move(content_throttle)));
  }
}

void KeepAliveURLLoader::Start() {
  CHECK(!is_started_);
  TRACE_EVENT("loading", "KeepAliveURLLoader::Start", "request_id",
              request_id_);
  is_started_ = true;

  if (IsFetchLater()) {
    base::UmaHistogramBoolean("FetchLater.Browser.Total.Started", true);
  }
  base::UmaHistogramBoolean("FetchKeepAlive.Browser.Total.Started", true);

  // Asks the network service to create a URL loader with passed in params.
  network_loader_factory_->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
      resource_request_, loader_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation_);
  loader_receiver_.set_disconnect_handler(base::BindOnce(
      &KeepAliveURLLoader::OnNetworkConnectionError, base::Unretained(this)));
  forwarding_client_.set_disconnect_handler(
      base::BindOnce(&KeepAliveURLLoader::OnForwardingClientDisconnected,
                     base::Unretained(this)));

  // These throttles are also run by `blink::ThrottlingURLLoader`. However, they
  // have to be re-run here in case of handling in-browser redirects.
  // There is already a similar use case that also runs throttles in browser in
  // `SearchPrefetchRequest::StartPrefetchRequest()`. The review discussion in
  // https://crrev.com/c/2552723/3 suggests that running them again in browser
  // is fine.
  for (auto& throttle_entry : throttle_entries_) {
    TRACE_EVENT("loading",
                "KeepAliveURLLoader::KeepAliveURLLoader.WillStartRequest");
    bool throttle_deferred = false;
    auto weak_ptr = GetWeakPtr();
    // Marks delegate to ignore abort requests if this is connected to renderer.
    throttle_entry->throttle().WillStartRequest(&resource_request_,
                                                &throttle_deferred);
    if (!weak_ptr) {
      // `this` is already destroyed by throttle.
      return;
    }
    if (!IsRendererConnected() && throttle_deferred) {
      // Only processes a throttle result if this loader is already disconnected
      // from renderer. We treat deferring as canceling the request.
      // See also `ThrottleDelegate` which may cancel request asynchronously.
      OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
      return;
    }
  }
}

KeepAliveURLLoader::~KeepAliveURLLoader() {
  TRACE_EVENT("loading", "KeepAliveURLLoader::~KeepAliveURLLoader",
              "request_id", request_id_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("loading", "KeepAliveURLLoader", request_id_);

  disconnected_loader_timer_.Stop();
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

void KeepAliveURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::FollowRedirect", "request_id",
              request_id_, "url", new_url);

  if (new_url != absl::nullopt) {
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

  // Forwards the action to `loader_` in the network service.
  loader_->SetPriority(priority, intra_priority_value);
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

  if (paused_reading_body_from_net_count_ == 0) {
    // Only sends the action to `loader_` in the network service once before
    // resuming.
    loader_->PauseReadingBodyFromNet();
  }
  paused_reading_body_from_net_count_++;

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->PauseReadingBodyFromNetProcessed(this);
  }
}

// TODO(crbug.com/1356128): Add test coverage.
void KeepAliveURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::ResumeReadingBodyFromNet",
              "request_id", request_id_);
  if (HasReceivedResponse()) {
    // This may come from a renderer that tries to process a redirect which has
    // been previously handled in this loader.
    return;
  }

  if (paused_reading_body_from_net_count_ == 1) {
    // Sends the action to `loader_` in the network service.
    loader_->ResumeReadingBodyFromNet();
  }
  paused_reading_body_from_net_count_--;

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->ResumeReadingBodyFromNetProcessed(this);
  }
}

void KeepAliveURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnReceiveEarlyHints",
              "request_id", request_id_);

  if (IsRendererConnected()) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
    return;
  }

  // TODO(crbug.com/1356128): Handle in browser process.
}

void KeepAliveURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnReceiveRedirect", "request_id",
              request_id_);
  base::UmaHistogramBoolean("FetchKeepAlive.Browser.Total.Redirected", true);

  // Stores the redirect data for later use by renderer.
  stored_url_load_->redirects.emplace(
      std::make_unique<StoredURLLoad::RedirectData>(redirect_info,
                                                    std::move(head)));

  // Handles all redirects in browser first.
  // See also the call sequence from renderer:
  // https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit?pli=1#heading=h.d006i46pmq9

  // Runs throttles from content embedder.
  ModifiedHeaders modified;
  for (auto& throttle_entry : throttle_entries_) {
    TRACE_EVENT("loading",
                "KeepAliveURLLoader::OnReceiveRedirect.WillRedirectRequest");
    bool throttle_deferred = false;
    ModifiedHeaders throttle_modified;
    net::RedirectInfo redirect_info_copy = redirect_info;
    auto weak_ptr = GetWeakPtr();
    throttle_entry->throttle().WillRedirectRequest(
        &redirect_info_copy, *(stored_url_load_->redirects.back()->head),
        &throttle_deferred, &throttle_modified.removed_headers,
        &throttle_modified.modified_headers,
        &throttle_modified.modified_cors_exempt_headers);
    if (!weak_ptr) {
      // `this` is already destroyed by throttle.
      return;
    }
    CHECK_EQ(redirect_info_copy.new_url, redirect_info.new_url)
        << "KeepAliveURLLoader doesn't support throttles changing the URL.";

    if (throttle_deferred) {
      // We treat deferring as canceling the request.
      // See also `ThrottleDelegate` which may cancel request asynchronously.
      OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
      return;
    }
    modified.MergeFrom(throttle_modified);
  }

  if (net::Error err = WillFollowRedirect(redirect_info); err != net::OK) {
    OnComplete(network::URLLoaderCompletionStatus(err));
    return;
  }

  // TODO(crbug.com/1356128): Replicate critical logic from the followings:
  //   `ResourceRequestSender::OnReceivedRedirect()`.
  //   `URLLoader::Context::OnReceivedRedirect().
  // TODO(crbug.com/1356128): Figure out how to deal with lost ResourceFetcher's
  // counter & dev console logging (renderer is dead).

  resource_request_.url = redirect_info.new_url;
  resource_request_.site_for_cookies = redirect_info.new_site_for_cookies;
  resource_request_.referrer = GURL(redirect_info.new_referrer);
  resource_request_.referrer_policy = redirect_info.new_referrer_policy;
  // Ask the network service to follow the redirect.
  last_url_ = GURL(redirect_info.new_url);
  // TODO(crbug.com/1393520): Remove Authorization header upon cross-origin
  // redirect.
  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnReceiveRedirectProcessed(this);
  }

  // Directly forwards the action to `loader_` in the network service.
  //
  // Follows redirect only after all current throttle UI tasks are executed.
  // Note: there may be throttles running in IO thread, which may send signals
  // in between `FollowRedirect()` and the next `OnReceiveRedirect()` or
  // `OnReceiveResponse()`.
  loader_->FollowRedirect(modified.removed_headers, modified.modified_headers,
                          modified.modified_cors_exempt_headers,
                          /*new_url=*/absl::nullopt);
}

void KeepAliveURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnReceiveResponse", "request_id",
              request_id_, "url", last_url_);
  base::UmaHistogramBoolean("FetchKeepAlive.Browser.Total.ReceivedResponse",
                            true);

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnReceiveResponse(this);
  }

  // In case the renderer is alive, the stored response data will be forwarded
  // at the end of `ForwardURLLoad()`.
  stored_url_load_->response = std::make_unique<StoredURLLoad::ResponseData>(
      std::move(response), std::move(body), std::move(cached_metadata));

  // TODO(crbug.com/1422645): Ensure that attributionsrc response handling is
  // migrated to browser process here so that it works even when renderer is
  // disconnected.
  // For now, it happens in the renderer after response is forwarded.

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

  // No need to wait for `OnComplete()`.
  // This loader should be deleted immediately to avoid hanged requests taking
  // up resources.
  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

void KeepAliveURLLoader::OnUploadProgress(int64_t current_position,
                                          int64_t total_size,
                                          base::OnceCallback<void()> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnUploadProgress", "request_id",
              request_id_);

  if (IsRendererConnected()) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnUploadProgress(current_position, total_size,
                                         std::move(callback));
    return;
  }

  // TODO(crbug.com/1356128): Handle in the browser process.
}

void KeepAliveURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnTransferSizeUpdated",
              "request_id", request_id_);

  if (IsRendererConnected()) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
    return;
  }

  // TODO(crbug.com/1356128): Handle in the browser process.
}

void KeepAliveURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnComplete", "request_id",
              request_id_);

  if (observer_for_testing_) {
    CHECK_IS_TEST();
    observer_for_testing_->OnComplete(this, completion_status);
  }

  LogFetchKeepAliveMetric(
      completion_status.error_code == net::OK
          ? FetchKeepAliveBrowserMetricType::kLoadingSuceeded
          : FetchKeepAliveBrowserMetricType::kLoadingFailed);

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

  // TODO(crbug.com/1356128): Handle in the browser process.
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
    LogFetchKeepAliveMetric(
        FetchKeepAliveBrowserMetricType::kForwardingCompleted);

    DeleteSelf();
    // DO NOT touch any members after this line. `this` is already deleted.
  }
}

bool KeepAliveURLLoader::IsForwardURLLoadStarted() const {
  return stored_url_load_ && stored_url_load_->forwarding;
}

bool KeepAliveURLLoader::IsRendererConnected() const {
  return !!forwarding_client_;
}

net::Error KeepAliveURLLoader::WillFollowRedirect(
    const net::RedirectInfo& redirect_info) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/1356128): Add logic to handle redirecting to extensions from
  // `ChromeContentRendererClient::IsSafeRedirectTarget()`.
  if (!IsSafeRedirectTarget(last_url_, redirect_info.new_url)) {
    return net::ERR_UNSAFE_REDIRECT;
  }

  if (resource_request_.redirect_mode == network::mojom::RedirectMode::kError) {
    return net::ERR_FAILED;
  }

  if (resource_request_.redirect_mode !=
      network::mojom::RedirectMode::kManual) {
    // Checks if redirecting to `url` is allowed by ContentSecurityPolicy from
    // the request initiator document.
    if (!IsRedirectAllowedByCSP(
            policy_container_host_->policies().content_security_policies,
            redirect_info.new_url, initial_url_, last_url_ != initial_url_)) {
      return net::ERR_BLOCKED_BY_CSP;
    }

    // TODO(crbug.com/1356128): Refactor logic from
    // `blink::MixedContentChecker::ShouldBlockFetch()` to support checking
    // without a frame.
  }

  return net::OK;
}

// Browser <- Network connection.
void KeepAliveURLLoader::OnNetworkConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnNetworkConnectionError",
              "request_id", request_id_);

  // The network loader either has an error or gets disconnected after response
  // handling is completed.
  if (IsRendererConnected()) {
    if (!IsForwardURLLoadStarted()) {
      // The network service disconnects before this loader forwards anything to
      // renderer.
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
void KeepAliveURLLoader::OnForwardingClientDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnForwardingClientDisconnected",
              "request_id", request_id_);

  // Dropping the client as renderer is gone.
  forwarding_client_.reset();

  if (!IsForwardURLLoadStarted() && !HasReceivedResponse()) {
    // The renderer disconnects before this loader forwards anything to it.
    // But the in-browser request processing may not complete yet.

    // TODO(crbug.com/1422645): Ensure that attributionsrc response handling is
    // taken over by browser.
    return;
  }

  // Renderer disconnects in-between forwarding, no need to call
  // `ForwardURLLoad()` anymore.
  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

// Browser <- Renderer connection.
void KeepAliveURLLoader::OnURLLoaderDisconnected() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnURLLoaderDisconnected",
              "request_id", request_id_);
  CHECK(!disconnected_loader_timer_.IsRunning());

  if (!IsStarted()) {
    // May be the last chance to start a deferred loader.
    LogFetchLaterMetric(
        FetchLaterBrowserMetricType::kStartedAfterInitiatorDisconnected);
    Start();
  }
  // For other types of keepalive requests, this loader does not care about
  // whether messages can be received from renderer or not.

  // Prevents this loader from staying alive indefinitely.
  disconnected_loader_timer_.Start(
      FROM_HERE, GetDisconnectedKeepAliveURLLoaderTimeout(),
      base::BindOnce(&KeepAliveURLLoader::OnDisconnectedLoaderTimerFired,
                     // `this` owns `disconnected_loader_timer_`.
                     base::Unretained(this)));
}

void KeepAliveURLLoader::OnDisconnectedLoaderTimerFired() {
  if (IsFetchLater()) {
    LogFetchLaterMetric(FetchLaterBrowserMetricType::kCancelledAfterTimeLimit);
  }
  LogFetchKeepAliveMetric(
      FetchKeepAliveBrowserMetricType::kCancelledAfterTimeLimit);
  DeleteSelf();
}

bool KeepAliveURLLoader::IsFetchLater() const {
  return base::FeatureList::IsEnabled(blink::features::kFetchLaterAPI) &&
         resource_request_.is_fetch_later_api;
}

void KeepAliveURLLoader::DeleteSelf() {
  CHECK(on_delete_callback_);
  base::UmaHistogramBoolean("FetchKeepAlive.Browser.Total.Finished", true);
  std::move(on_delete_callback_).Run();
}

void KeepAliveURLLoader::SetObserverForTesting(
    scoped_refptr<TestObserver> observer) {
  observer_for_testing_ = observer;
}

}  // namespace content
