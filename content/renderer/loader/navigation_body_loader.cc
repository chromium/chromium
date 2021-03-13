// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/navigation_body_loader.h"

#include "base/bind.h"
#include "base/macros.h"
#include "content/renderer/render_frame_impl.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/web/web_navigation_params.h"

namespace content {

// static
constexpr uint32_t NavigationBodyLoader::kMaxNumConsumedBytesInTask;

// static
void NavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    int request_id,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    RenderFrameImpl* render_frame_impl,
    bool is_main_frame,
    blink::WebNavigationParams* navigation_params) {
  std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
      resource_load_info_notifier_wrapper =
          render_frame_impl
              ? render_frame_impl->CreateResourceLoadInfoNotifierWrapper()
              : std::make_unique<blink::ResourceLoadInfoNotifierWrapper>(
                    /*resource_load_info_notifier=*/nullptr);

  // Use the original navigation url to start with. We'll replay the
  // redirects afterwards and will eventually arrive to the final url.
  const GURL original_url = !commit_params->original_url.is_empty()
                                ? commit_params->original_url
                                : common_params->url;
  GURL url = original_url;
  resource_load_info_notifier_wrapper->NotifyResourceLoadInitiated(
      request_id, url,
      !commit_params->original_method.empty() ? commit_params->original_method
                                              : common_params->method,
      common_params->referrer->url,
      // TODO(kinuko): This should use the same value as in the request that
      // was used in browser process, i.e. what CreateResourceRequest in
      // content/browser/loader/navigation_url_loader_impl.cc gives.
      // (Currently we don't propagate the value from the browser on
      // navigation commit.)
      is_main_frame ? network::mojom::RequestDestination::kDocument
                    : network::mojom::RequestDestination::kIframe,
      is_main_frame ? net::HIGHEST : net::LOWEST);
  size_t redirect_count = commit_params->redirect_response.size();

  if (redirect_count != commit_params->redirects.size()) {
    // We currently incorrectly send empty redirect_response and redirect_infos
    // on frame reloads and some cases involving throttles.
    // TODO(https://crbug.com/1171225): Fix this.
    DCHECK_EQ(0u, redirect_count);
    DCHECK_EQ(0u, commit_params->redirect_infos.size());
    DCHECK_NE(0u, commit_params->redirects.size());
  }
  navigation_params->redirects.reserve(redirect_count);
  navigation_params->redirects.resize(redirect_count);
  for (size_t i = 0; i < redirect_count; ++i) {
    blink::WebNavigationParams::RedirectInfo& redirect =
        navigation_params->redirects[i];
    auto& redirect_info = commit_params->redirect_infos[i];
    auto& redirect_response = commit_params->redirect_response[i];
    blink::WebURLLoader::PopulateURLResponse(
        url, *redirect_response, &redirect.redirect_response,
        response_head->ssl_info.has_value(), request_id);
    resource_load_info_notifier_wrapper->NotifyResourceRedirectReceived(
        redirect_info, std::move(redirect_response));
    if (url.SchemeIs(url::kDataScheme))
      redirect.redirect_response.SetHttpStatusCode(200);
    redirect.new_url = redirect_info.new_url;
    redirect.new_referrer =
        blink::WebString::FromUTF8(redirect_info.new_referrer);
    redirect.new_referrer_policy =
        blink::ReferrerUtils::NetToMojoReferrerPolicy(
            redirect_info.new_referrer_policy);
    redirect.new_http_method =
        blink::WebString::FromLatin1(redirect_info.new_method);
    url = redirect_info.new_url;
  }

  blink::WebURLLoader::PopulateURLResponse(
      url, *response_head, &navigation_params->response,
      response_head->ssl_info.has_value(), request_id);
  if (url.SchemeIs(url::kDataScheme))
    navigation_params->response.SetHttpStatusCode(200);

  if (url_loader_client_endpoints) {
    navigation_params->body_loader.reset(new NavigationBodyLoader(
        original_url, std::move(response_head), std::move(response_body),
        std::move(url_loader_client_endpoints), task_runner,
        std::move(resource_load_info_notifier_wrapper)));
  }
}

NavigationBodyLoader::NavigationBodyLoader(
    const GURL& original_url,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper)
    : response_head_(std::move(response_head)),
      response_body_(std::move(response_body)),
      endpoints_(std::move(endpoints)),
      task_runner_(std::move(task_runner)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      task_runner_),
      resource_load_info_notifier_wrapper_(
          std::move(resource_load_info_notifier_wrapper)),
      original_url_(original_url) {}

NavigationBodyLoader::~NavigationBodyLoader() {
  if (!has_received_completion_ || !has_seen_end_of_data_) {
    resource_load_info_notifier_wrapper_->NotifyResourceLoadCanceled(
        net::ERR_ABORTED);
  }
}

void NavigationBodyLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnUploadProgress(int64_t current_position,
                                            int64_t total_size,
                                            OnUploadProgressCallback callback) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  // Even if IsolatedCodeCaching is landed, this code is still used by
  // ServiceWorker.
  // TODO(horo, kinuko): Make a test to cover this function.
  // TODO(https://crbug.com/930000): Add support for inline script code caching
  // with the service worker service.
  client_->BodyCodeCacheReceived(std::move(data));
}

void NavigationBodyLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  resource_load_info_notifier_wrapper_->NotifyResourceTransferSizeUpdated(
      transfer_size_diff);
}

void NavigationBodyLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle handle) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::OnStartLoadingResponseBody",
               "url", original_url_.possibly_invalid_spec());
  DCHECK(!has_received_body_handle_);
  DCHECK(!has_received_completion_);
  has_received_body_handle_ = true;
  has_seen_end_of_data_ = false;
  handle_ = std::move(handle);
  DCHECK(handle_.is_valid());
  handle_watcher_.Watch(handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        base::BindRepeating(&NavigationBodyLoader::OnReadable,
                                            base::Unretained(this)));
  OnReadable(MOJO_RESULT_OK);
}

void NavigationBodyLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Except for errors, there must always be a response's body.
  DCHECK(has_received_body_handle_ || status.error_code != net::OK);
  has_received_completion_ = true;
  status_ = status;
  NotifyCompletionIfAppropriate();
}

void NavigationBodyLoader::SetDefersLoading(
    blink::WebURLLoader::DeferType defers) {
  if (defer_type_ == defers)
    return;
  defer_type_ = defers;
  if (handle_.is_valid())
    OnReadable(MOJO_RESULT_OK);
}

void NavigationBodyLoader::StartLoadingBody(
    WebNavigationBodyLoader::Client* client,
    bool use_isolated_code_cache) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::StartLoadingBody", "url",
               original_url_.possibly_invalid_spec());
  client_ = client;

  base::Time response_head_response_time = response_head_->response_time;
  resource_load_info_notifier_wrapper_->NotifyResourceResponseReceived(
      std::move(response_head_), blink::PreviewsTypes::PREVIEWS_OFF);

  if (use_isolated_code_cache) {
    code_cache_loader_ = blink::WebCodeCacheLoader::Create();
    code_cache_loader_->FetchFromCodeCache(
        blink::mojom::CodeCacheType::kJavascript, original_url_,
        base::BindOnce(&NavigationBodyLoader::CodeCacheReceived,
                       weak_factory_.GetWeakPtr(),
                       response_head_response_time));
    return;
  }

  BindURLLoaderAndStartLoadingResponseBodyIfPossible();
}

void NavigationBodyLoader::CodeCacheReceived(
    base::Time response_head_response_time,
    base::Time response_time,
    mojo_base::BigBuffer data) {
  // Check that the times match to ensure that the code cache data is for this
  // response. See https://crbug.com/1099587.
  if (response_head_response_time == response_time && client_) {
    base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
    client_->BodyCodeCacheReceived(std::move(data));
    if (!weak_self)
      return;
  }
  code_cache_loader_.reset();

  // TODO(dgozman): we should explore retrieveing code cache in parallel with
  // receiving response or reading the first data chunk.
  BindURLLoaderAndStartLoadingResponseBodyIfPossible();
}

void NavigationBodyLoader::BindURLLoaderAndContinue() {
  url_loader_.Bind(std::move(endpoints_->url_loader), task_runner_);
  url_loader_client_receiver_.Bind(std::move(endpoints_->url_loader_client),
                                   task_runner_);
  url_loader_client_receiver_.set_disconnect_handler(base::BindOnce(
      &NavigationBodyLoader::OnConnectionClosed, base::Unretained(this)));
}

void NavigationBodyLoader::OnConnectionClosed() {
  // If the connection aborts before the load completes, mark it as failed.
  if (!has_received_completion_)
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

void NavigationBodyLoader::OnReadable(MojoResult unused) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::OnReadable", "url",
               original_url_.possibly_invalid_spec());
  if (has_seen_end_of_data_ ||
      defer_type_ != blink::WebURLLoader::DeferType::kNotDeferred ||
      is_in_on_readable_)
    return;
  // Protect against reentrancy:
  // - when the client calls SetDefersLoading;
  // - when nested message loop starts from BodyDataReceived
  //   and we get notified by the watcher.
  // Note: we cannot use AutoReset here since |this| may be deleted
  // before reset.
  is_in_on_readable_ = true;
  base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
  ReadFromDataPipe();
  if (!weak_self)
    return;
  is_in_on_readable_ = false;
}

void NavigationBodyLoader::ReadFromDataPipe() {
  TRACE_EVENT1("loading", "NavigationBodyLoader::ReadFromDataPipe", "url",
               original_url_.possibly_invalid_spec());
  uint32_t num_bytes_consumed = 0;
  while (defer_type_ == blink::WebURLLoader::DeferType::kNotDeferred) {
    const void* buffer = nullptr;
    uint32_t available = 0;
    MojoResult result =
        handle_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      has_seen_end_of_data_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    if (result != MOJO_RESULT_OK) {
      status_.error_code = net::ERR_FAILED;
      has_seen_end_of_data_ = true;
      has_received_completion_ = true;
      NotifyCompletionIfAppropriate();
      return;
    }
    DCHECK_LE(num_bytes_consumed, kMaxNumConsumedBytesInTask);
    available =
        std::min(available, kMaxNumConsumedBytesInTask - num_bytes_consumed);
    if (available == 0) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      result = handle_->EndReadData(0);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      handle_watcher_.ArmOrNotify();
      return;
    }
    num_bytes_consumed += available;
    base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
    client_->BodyDataReceived(
        base::make_span(static_cast<const char*>(buffer), available));
    if (!weak_self)
      return;
    result = handle_->EndReadData(available);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }
}

void NavigationBodyLoader::NotifyCompletionIfAppropriate() {
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;

  handle_watcher_.Cancel();

  base::Optional<blink::WebURLError> error;
  if (status_.error_code != net::OK) {
    error = blink::WebURLLoader::PopulateURLError(status_, original_url_);
  }

  resource_load_info_notifier_wrapper_->NotifyResourceLoadCompleted(status_);

  if (!client_)
    return;

  // |this| may be deleted after calling into client_, so clear it in advance.
  WebNavigationBodyLoader::Client* client = client_;
  client_ = nullptr;
  client->BodyLoadingFinished(
      status_.completion_time, status_.encoded_data_length,
      status_.encoded_body_length, status_.decoded_body_length,
      status_.should_report_corb_blocking, error);
}

void NavigationBodyLoader::
    BindURLLoaderAndStartLoadingResponseBodyIfPossible() {
  // Bind the mojo::URLLoaderClient interface in advance, because we will start
  // to read from the data pipe immediately which may potentially postpone the
  // method calls from the remote. That causes the flakiness of some layout
  // tests.
  // TODO(minggang): The binding was executed after OnStartLoadingResponseBody
  // originally (prior to passing the response body from the browser process
  // during navigation), we should try to put it back if all the
  // webkit_layout_tests can pass in that way.
  BindURLLoaderAndContinue();

  DCHECK(response_body_.is_valid());
  OnStartLoadingResponseBody(std::move(response_body_));
  // Don't use |this| here as it might have been destroyed.
}

}  // namespace content
