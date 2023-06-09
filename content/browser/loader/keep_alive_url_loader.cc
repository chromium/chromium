// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader.h"

#include <vector>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "content/browser/renderer_host/frame_tree_node.h"
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
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace content {
namespace {

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

KeepAliveURLLoader::KeepAliveURLLoader(
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    scoped_refptr<PolicyContainerHost> policy_container_host,
    BrowserContext* browser_context,
    base::PassKey<KeepAliveURLLoaderService>,
    URLLoaderThrottlesGetter url_loader_throttles_getter_for_testing)
    : request_id_(request_id),
      resource_request_(resource_request),
      forwarding_client_(std::move(forwarding_client)),
      policy_container_host_(std::move(policy_container_host)),
      initial_url_(resource_request.url),
      last_url_(resource_request.url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(network_loader_factory);
  CHECK(policy_container_host_);
  CHECK(!resource_request.trusted_params);
  CHECK(browser_context);
  TRACE_EVENT("loading", "KeepAliveURLLoader::KeepAliveURLLoader", "request_id",
              request_id_, "url", last_url_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("loading", "KeepAliveURLLoader",
                                    request_id_, "url", last_url_);

  // Asks the network service to create a URL loader with passed in params.
  network_loader_factory->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), request_id, options,
      resource_request, loader_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation);
  loader_receiver_.set_disconnect_handler(base::BindOnce(
      &KeepAliveURLLoader::OnNetworkConnectionError, base::Unretained(this)));
  forwarding_client_.set_disconnect_handler(base::BindOnce(
      &KeepAliveURLLoader::OnRendererConnectionError, base::Unretained(this)));

  {
    std::vector<std::unique_ptr<blink::URLLoaderThrottle>> content_throttles =
        url_loader_throttles_getter_for_testing
            ? url_loader_throttles_getter_for_testing.Run()
            : CreateContentBrowserURLLoaderThrottlesForKeepAlive(
                  resource_request_, browser_context,
                  // When `throttles_` need to be run by this loader, the
                  // renderer should have be gone.
                  /*wc_getter=*/base::BindRepeating([]() -> WebContents* {
                    return nullptr;
                  }),
                  FrameTreeNode::kFrameTreeNodeInvalidId);
    for (auto& content_throttle : content_throttles) {
      throttle_entries_.emplace_back(std::make_unique<ThrottleEntry>(
          GetWeakPtr(), std::move(content_throttle)));
    }
  }

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
}

void KeepAliveURLLoader::set_on_delete_callback(
    OnDeleteCallback on_delete_callback) {
  on_delete_callback_ = std::move(on_delete_callback);
}

base::WeakPtr<KeepAliveURLLoader> KeepAliveURLLoader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void KeepAliveURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::FollowRedirect", "request_id",
              request_id_, "url", new_url);

  // Forwards the action to `loader_` in the network service.
  loader_->FollowRedirect(removed_headers, modified_headers,
                          modified_cors_exempt_headers, new_url);
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
  TRACE_EVENT("loading", "KeepAliveURLLoader::FollowRedirect", "request_id",
              request_id_);
  if (IsRendererConnected()) {
    // If the renderer is alive, simply forwards the action to the network
    // service as the checks are already handled in the renderer.
    loader_->PauseReadingBodyFromNet();
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
  if (IsRendererConnected()) {
    // If the renderer is alive, simply forwards the action to the network
    // service as the checks are already handled in the renderer.
    loader_->ResumeReadingBodyFromNet();
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

void KeepAliveURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnReceiveResponse", "request_id",
              request_id_, "url", last_url_);

  has_received_response_ = true;
  // TODO(crbug.com/1424731): The renderer might exit before `OnReceiveRedirect`
  // or `OnReceiveResponse` is called, or during their execution. In such case,
  // `forwarding_client_` can't finish response handling. Figure out a way to
  // negotiate shutdown timing via RenderFrameHostImpl::OnUnloadAck() and
  // invalidate `forwarding_client_`.
  if (IsRendererConnected()) {
    // The renderer is alive, forwards the action.

    // The receiver may fail to finish reading `response`, so response caching
    // is not guaranteed.
    forwarding_client_->OnReceiveResponse(std::move(response), std::move(body),
                                          std::move(cached_metadata));
    // TODO(crbug.com/1422645): Ensure that attributionsrc response handling is
    // migrated to browser process.

    if (observer_for_testing_) {
      CHECK_IS_TEST();
      observer_for_testing_->OnReceiveResponseForwarded(this);
    }
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

void KeepAliveURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnReceiveRedirect", "request_id",
              request_id_);

  // TODO(crbug.com/1424731): The renderer might exit before `OnReceiveRedirect`
  // or `OnReceiveResponse` is called, or during their execution. In such case,
  // `forwarding_client_` can't finish response handling. Figure out a way to
  // negotiate shutdown timing via RenderFrameHostImpl::OnUnloadAck() and
  // invalidate `forwarding_client_`.
  if (IsRendererConnected()) {
    // The renderer is alive, forwards the action.
    // Redirects must be handled by the renderer so that it know what URL the
    // response come from when parsing responses.
    forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));

    if (observer_for_testing_) {
      CHECK_IS_TEST();
      observer_for_testing_->OnReceiveRedirectForwarded(this);
    }
    return;
  }

  // Handles redirect in browser. See also the call sequence from renderer:
  // https://docs.google.com/document/d/1ZzxMMBvpqn8VZBZKnb7Go8TWjnrGcXuLS_USwVVRUvY/edit#heading=h.6uwqtijf7dvd

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
        &redirect_info_copy, *head, &throttle_deferred,
        &throttle_modified.removed_headers, &throttle_modified.modified_headers,
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

  // Follows redirect only after all current throttle UI tasks are executed.
  // Note: there may be throttles running in IO thread, which may send signals
  // in between `FollowRedirect()` and the next `OnReceiveRedirect()` or
  // `OnReceiveResponse()`.
  FollowRedirect(modified.removed_headers, modified.modified_headers,
                 modified.modified_cors_exempt_headers,
                 /*new_url=*/absl::nullopt);
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

  if (IsRendererConnected()) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnComplete(completion_status);

    if (observer_for_testing_) {
      CHECK_IS_TEST();
      observer_for_testing_->OnCompleteForwarded(this, completion_status);
    }

    DeleteSelf();
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

void KeepAliveURLLoader::OnNetworkConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnNetworkConnectionError",
              "request_id", request_id_);

  // The network loader has an error; we should let the client know it's
  // closed by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

void KeepAliveURLLoader::OnRendererConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("loading", "KeepAliveURLLoader::OnRendererConnectionError",
              "request_id", request_id_);

  if (has_received_response_) {
    // No need to wait for `OnComplete()`.
    DeleteSelf();
    // DO NOT touch any members after this line. `this` is already deleted.
    return;
  }
  // Otherwise, let this loader continue to handle responses.
  forwarding_client_.reset();
  // TODO(crbug.com/1424731): When we reach here while the renderer is
  // processing a redirect, we should take over the redirect handling in the
  // browser process. See TODOs in `OnReceiveRedirect()`.
}

void KeepAliveURLLoader::DeleteSelf() {
  CHECK(on_delete_callback_);
  std::move(on_delete_callback_).Run();
}

void KeepAliveURLLoader::SetObserverForTesting(
    scoped_refptr<TestObserver> observer) {
  observer_for_testing_ = observer;
}

}  // namespace content
