// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/navigation_body_loader.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_url_loader.h"
#include "third_party/blink/public/web/web_navigation_params.h"

namespace blink {

NavigationBodyLoader::NavigationBodyLoader(
    const KURL& original_url,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    bool is_main_frame)
    : response_head_(std::move(response_head)),
      response_body_(std::move(response_body)),
      endpoints_(std::move(endpoints)),
      task_runner_(std::move(task_runner)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      task_runner_),
      resource_load_info_notifier_wrapper_(
          std::move(resource_load_info_notifier_wrapper)),
      original_url_(original_url),
      is_main_frame_(is_main_frame) {}

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
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body) {
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
  base::UmaHistogramBoolean(
      base::StrCat({"V8.InlineCodeCache.",
                    is_main_frame_ ? "MainFrame" : "Subframe",
                    ".CacheTimesMatch"}),
      true);
  client_->BodyCodeCacheReceived(std::move(data));
}

void NavigationBodyLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  resource_load_info_notifier_wrapper_->NotifyResourceTransferSizeUpdated(
      transfer_size_diff);
}

void NavigationBodyLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle handle) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::OnStartLoadingResponseBody",
               "url", original_url_.GetString().Utf8());
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

void NavigationBodyLoader::SetDefersLoading(WebLoaderFreezeMode mode) {
  if (freeze_mode_ == mode)
    return;
  freeze_mode_ = mode;
  if (handle_.is_valid())
    OnReadable(MOJO_RESULT_OK);
}

void NavigationBodyLoader::StartLoadingBody(
    WebNavigationBodyLoader::Client* client,
    CodeCacheHost* code_cache_host) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::StartLoadingBody", "url",
               original_url_.GetString().Utf8());
  client_ = client;

  base::Time response_head_response_time = response_head_->response_time;
  resource_load_info_notifier_wrapper_->NotifyResourceResponseReceived(
      std::move(response_head_));

  if (code_cache_host) {
    if (code_cache_data_) {
      ContinueWithCodeCache(base::TimeTicks::Now(),
                            response_head_response_time);
      return;
    }

    // Save these for when the code cache is ready.
    code_cache_wait_start_time_ = base::TimeTicks::Now();
    response_head_response_time_ = response_head_response_time;

    // If the code cache loader hasn't been created yet the request hasn't
    // started, so start it now.
    if (!code_cache_loader_)
      StartLoadingCodeCache(code_cache_host);

    // TODO(crbug.com/1274867): See if this can be enabled for subframes too.
    if (base::FeatureList::IsEnabled(features::kEarlyBodyLoad) &&
        is_main_frame_) {
      // Start loading the body in parallel with the code cache.
      BindURLLoaderAndStartLoadingResponseBodyIfPossible();
    }
    return;
  }

  code_cache_data_ = mojo_base::BigBuffer();
  ContinueWithCodeCache(base::TimeTicks::Now(), response_head_response_time);
}

void NavigationBodyLoader::StartLoadingCodeCache(
    CodeCacheHost* code_cache_host) {
  code_cache_loader_ = WebCodeCacheLoader::Create(code_cache_host);
  code_cache_loader_->FetchFromCodeCache(
      mojom::CodeCacheType::kJavascript, original_url_,
      base::BindOnce(&NavigationBodyLoader::CodeCacheReceived,
                     weak_factory_.GetWeakPtr()));
}

void NavigationBodyLoader::CodeCacheReceived(base::Time response_time,
                                             mojo_base::BigBuffer data) {
  code_cache_data_ = std::move(data);
  code_cache_response_time_ = response_time;
  if (!code_cache_wait_start_time_.is_null()) {
    ContinueWithCodeCache(code_cache_wait_start_time_,
                          response_head_response_time_);
  }
}

void NavigationBodyLoader::ContinueWithCodeCache(
    base::TimeTicks start_time,
    base::Time response_head_response_time) {
  if (code_cache_loader_) {
    base::UmaHistogramTimes(
        base::StrCat({"Navigation.CodeCacheTime.",
                      is_main_frame_ ? "MainFrame" : "Subframe"}),
        base::TimeTicks::Now() - start_time);
  }

  // Check that the times match to ensure that the code cache data is for this
  // response. See https://crbug.com/1099587.
  const bool is_cache_usable =
      (response_head_response_time == code_cache_response_time_);
  base::UmaHistogramBoolean(
      base::StrCat({"V8.InlineCodeCache.",
                    is_main_frame_ ? "MainFrame" : "Subframe",
                    ".CacheTimesMatch"}),
      is_cache_usable);
  if (!is_cache_usable)
    code_cache_data_ = mojo_base::BigBuffer();

  auto weak_self = weak_factory_.GetWeakPtr();
  if (client_) {
    client_->BodyCodeCacheReceived(std::move(*code_cache_data_));
    if (!weak_self)
      return;
  }
  code_cache_loader_.reset();
  NotifyCompletionIfAppropriate();
  if (!weak_self)
    return;

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
               original_url_.GetString().Utf8());
  if (has_seen_end_of_data_ || freeze_mode_ != WebLoaderFreezeMode::kNone ||
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
               original_url_.GetString().Utf8());
  uint32_t num_bytes_consumed = 0;
  while (freeze_mode_ == WebLoaderFreezeMode::kNone) {
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
    const uint32_t chunk_size = network::features::GetLoaderChunkSize();
    DCHECK_LE(num_bytes_consumed, chunk_size);
    available = std::min(available, chunk_size - num_bytes_consumed);
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
  if (!has_received_completion_ || !has_seen_end_of_data_ || code_cache_loader_)
    return;

  handle_watcher_.Cancel();

  absl::optional<WebURLError> error;
  if (status_.error_code != net::OK) {
    error = WebURLLoader::PopulateURLError(status_, original_url_);
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
  if (!response_body_) {
    DCHECK(base::FeatureList::IsEnabled(features::kEarlyBodyLoad));
    return;
  }
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

// static
void WebNavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    int request_id,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    bool is_main_frame,
    WebNavigationParams* navigation_params) {
  // Use the original navigation url to start with. We'll replay the
  // redirects afterwards and will eventually arrive to the final url.
  const KURL original_url = !commit_params->original_url.is_empty()
                                ? KURL(commit_params->original_url)
                                : KURL(common_params->url);
  KURL url = original_url;
  resource_load_info_notifier_wrapper->NotifyResourceLoadInitiated(
      request_id, url,
      !commit_params->original_method.empty() ? commit_params->original_method
                                              : common_params->method,
      common_params->referrer->url, common_params->request_destination,
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
    WebNavigationParams::RedirectInfo& redirect =
        navigation_params->redirects[i];
    auto& redirect_info = commit_params->redirect_infos[i];
    auto& redirect_response = commit_params->redirect_response[i];
    WebURLLoader::PopulateURLResponse(
        url, *redirect_response, &redirect.redirect_response,
        response_head->ssl_info.has_value(), request_id);
    resource_load_info_notifier_wrapper->NotifyResourceRedirectReceived(
        redirect_info, std::move(redirect_response));
    if (url.ProtocolIsData())
      redirect.redirect_response.SetHttpStatusCode(200);
    redirect.new_url = KURL(redirect_info.new_url);
    // WebString treats default and empty strings differently while std::string
    // does not. A default value is expected for new_referrer rather than empty.
    if (!redirect_info.new_referrer.empty())
      redirect.new_referrer = WebString::FromUTF8(redirect_info.new_referrer);
    redirect.new_referrer_policy = ReferrerUtils::NetToMojoReferrerPolicy(
        redirect_info.new_referrer_policy);
    redirect.new_http_method = WebString::FromLatin1(redirect_info.new_method);
    url = KURL(redirect_info.new_url);
  }

  WebURLLoader::PopulateURLResponse(
      url, *response_head, &navigation_params->response,
      response_head->ssl_info.has_value(), request_id);
  if (url.ProtocolIsData())
    navigation_params->response.SetHttpStatusCode(200);

  if (url_loader_client_endpoints) {
    navigation_params->body_loader.reset(new NavigationBodyLoader(
        original_url, std::move(response_head), std::move(response_body),
        std::move(url_loader_client_endpoints), task_runner,
        std::move(resource_load_info_notifier_wrapper), is_main_frame));
  }
}
}  // namespace blink
