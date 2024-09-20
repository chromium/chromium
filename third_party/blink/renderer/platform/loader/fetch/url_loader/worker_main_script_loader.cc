// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader.h"

#include "base/containers/span.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader_client.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WorkerMainScriptLoader::WorkerMainScriptLoader() = default;

WorkerMainScriptLoader::~WorkerMainScriptLoader() = default;

void WorkerMainScriptLoader::Start(
    const FetchParameters& fetch_params,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    FetchContext* fetch_context,
    ResourceLoadObserver* resource_load_observer,
    WorkerMainScriptLoaderClient* client) {
  DCHECK(resource_load_observer);
  DCHECK(client);
  request_id_ = worker_main_script_load_params->request_id;
  start_time_ = base::TimeTicks::Now();
  initial_request_ = fetch_params.GetResourceRequest();
  resource_loader_options_ = fetch_params.Options();
  initial_request_url_ = fetch_params.GetResourceRequest().Url();
  last_request_url_ = initial_request_url_;
  resource_load_observer_ = resource_load_observer;
  fetch_context_ = fetch_context;
  client_ = client;
  resource_load_info_notifier_wrapper_ =
      fetch_context->CreateResourceLoadInfoNotifierWrapper();

  // TODO(crbug.com/929370): Support CSP check to post violation reports for
  // worker top-level scripts, if off-the-main-thread fetch is enabled.

  // Currently we don't support ad resource check for the worker scripts.
  resource_load_info_notifier_wrapper_->NotifyResourceLoadInitiated(
      request_id_, GURL(initial_request_url_),
      initial_request_.HttpMethod().Latin1(),
      WebStringToGURL(WebString(initial_request_.ReferrerString())),
      initial_request_.GetRequestDestination(), net::HIGHEST,
      /*is_ad_resource=*/false);

  if (!worker_main_script_load_params->redirect_responses.empty()) {
    HandleRedirections(worker_main_script_load_params->redirect_infos,
                       worker_main_script_load_params->redirect_responses);
  }

  auto response_head = std::move(worker_main_script_load_params->response_head);
  WebURLResponse response =
      WebURLResponse::Create(WebURL(last_request_url_), *response_head,
                             response_head->ssl_info.has_value(), request_id_);
  resource_response_ = response.ToResourceResponse();
  resource_load_info_notifier_wrapper_->NotifyResourceResponseReceived(
      std::move(response_head));

  ResourceRequest resource_request(initial_request_);
  resource_load_observer_->DidReceiveResponse(
      initial_request_.InspectorId(), resource_request, resource_response_,
      /*resource=*/nullptr,
      ResourceLoadObserver::ResponseSource::kNotFromMemoryCache);

  if (resource_response_.IsHTTP() &&
      !network::IsSuccessfulStatus(resource_response_.HttpStatusCode())) {
    client_->OnFailedLoadingWorkerMainScript();
    resource_load_observer_->DidFailLoading(
        initial_request_.Url(), initial_request_.InspectorId(),
        ResourceError(net::ERR_FAILED, last_request_url_, std::nullopt),
        resource_response_.EncodedDataLength(),
        ResourceLoadObserver::IsInternalRequest(
            resource_loader_options_.initiator_info.name ==
            fetch_initiator_type_names::kInternal));
    return;
  }

  script_encoding_ =
      resource_response_.TextEncodingName().empty()
          ? UTF8Encoding()
          : WTF::TextEncoding(resource_response_.TextEncodingName());

  url_loader_remote_.Bind(std::move(
      worker_main_script_load_params->url_loader_client_endpoints->url_loader));
  receiver_.Bind(
      std::move(worker_main_script_load_params->url_loader_client_endpoints
                    ->url_loader_client));
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &WorkerMainScriptLoader::OnConnectionClosed, WrapWeakPersistent(this)));
  data_pipe_ = std::move(worker_main_script_load_params->response_body);

  client_->OnStartLoadingBodyWorkerMainScript(resource_response_);
  StartLoadingBody();
}

void WorkerMainScriptLoader::Cancel() {
  if (has_cancelled_)
    return;
  has_cancelled_ = true;
  if (watcher_ && watcher_->IsWatching())
    watcher_->Cancel();

  receiver_.reset();
  url_loader_remote_.reset();
}

void WorkerMainScriptLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // This has already happened in the browser process.
  NOTREACHED_IN_MIGRATION();
}

void WorkerMainScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle handle,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  // This has already happened in the browser process.
  NOTREACHED_IN_MIGRATION();
}

void WorkerMainScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // This has already happened in the browser process.
  NOTREACHED_IN_MIGRATION();
}

void WorkerMainScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // This has already happened in the browser process.
  NOTREACHED_IN_MIGRATION();
}

void WorkerMainScriptLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kWorkerMainScriptLoader);
}

void WorkerMainScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK)
    has_seen_end_of_data_ = true;

  // Reports resource timing info for the worker main script.
  resource_response_.SetEncodedBodyLength(status.encoded_body_length);
  resource_response_.SetDecodedBodyLength(status.decoded_body_length);
  resource_response_.SetCurrentRequestUrl(last_request_url_);

  // https://fetch.spec.whatwg.org/#fetch-finale
  // Step 3.3.1. If fetchParams's request's URL's scheme is not an HTTP(S)
  // scheme, then return.
  //
  // i.e. call `AddResourceTiming()` only if the URL's scheme is HTTP(S).
  if (initial_request_url_.ProtocolIsInHTTPFamily()) {
    mojom::blink::ResourceTimingInfoPtr timing_info = CreateResourceTimingInfo(
        start_time_, initial_request_url_, &resource_response_);
    timing_info->response_end = status.completion_time;
    fetch_context_->AddResourceTiming(std::move(timing_info),
                                      fetch_initiator_type_names::kOther);
  }

  has_received_completion_ = true;
  status_ = status;
  NotifyCompletionIfAppropriate();
}

CachedMetadataHandler* WorkerMainScriptLoader::CreateCachedMetadataHandler() {
  // Currently we support the metadata caching only for HTTP family.
  if (!initial_request_url_.ProtocolIsInHTTPFamily() ||
      !resource_response_.CurrentRequestUrl().ProtocolIsInHTTPFamily()) {
    return nullptr;
  }

  std::unique_ptr<CachedMetadataSender> cached_metadata_sender =
      CachedMetadataSender::Create(
          resource_response_, mojom::blink::CodeCacheType::kJavascript,
          SecurityOrigin::Create(initial_request_url_));
  return MakeGarbageCollected<ScriptCachedMetadataHandler>(
      script_encoding_, std::move(cached_metadata_sender));
}

void WorkerMainScriptLoader::Trace(Visitor* visitor) const {
  visitor->Trace(fetch_context_);
  visitor->Trace(resource_load_observer_);
  visitor->Trace(client_);
  visitor->Trace(resource_loader_options_);
}

void WorkerMainScriptLoader::StartLoadingBody() {
  // Loading body may be cancelled before starting by calling |Cancel()|.
  if (has_cancelled_)
    return;

  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  MojoResult rv =
      watcher_->Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                      WTF::BindRepeating(&WorkerMainScriptLoader::OnReadable,
                                         WrapWeakPersistent(this)));
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  watcher_->ArmOrNotify();
}

void WorkerMainScriptLoader::OnReadable(MojoResult) {
  // It isn't necessary to handle MojoResult here since BeginReadDataRaw()
  // returns an equivalent error.
  base::span<const uint8_t> buffer;
  MojoResult rv = data_pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  switch (rv) {
    case MOJO_RESULT_BUSY:
    case MOJO_RESULT_INVALID_ARGUMENT:
      NOTREACHED_IN_MIGRATION();
      return;
    case MOJO_RESULT_FAILED_PRECONDITION:
      has_seen_end_of_data_ = true;
      NotifyCompletionIfAppropriate();
      return;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_->ArmOrNotify();
      return;
    case MOJO_RESULT_OK:
      break;
    default:
      OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
  }

  if (!buffer.empty()) {
    base::span<const char> chars = base::as_chars(buffer);
    client_->DidReceiveDataWorkerMainScript(chars);
    resource_load_observer_->DidReceiveData(initial_request_.InspectorId(),
                                            base::SpanOrSize(chars));
  }

  rv = data_pipe_->EndReadData(buffer.size());
  DCHECK_EQ(rv, MOJO_RESULT_OK);
  watcher_->ArmOrNotify();
}

void WorkerMainScriptLoader::NotifyCompletionIfAppropriate() {
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;

  data_pipe_.reset();
  watcher_->Cancel();
  resource_load_info_notifier_wrapper_->NotifyResourceLoadCompleted(status_);

  if (!client_)
    return;
  WorkerMainScriptLoaderClient* client = client_.Get();
  client_.Clear();

  if (status_.error_code == net::OK) {
    client->OnFinishedLoadingWorkerMainScript();
    resource_load_observer_->DidFinishLoading(
        initial_request_.InspectorId(), base::TimeTicks::Now(),
        resource_response_.EncodedDataLength(),
        resource_response_.DecodedBodyLength());
  } else {
    client->OnFailedLoadingWorkerMainScript();
    resource_load_observer_->DidFailLoading(
        last_request_url_, initial_request_.InspectorId(),
        ResourceError(status_.error_code, last_request_url_, std::nullopt),
        resource_response_.EncodedDataLength(),
        ResourceLoadObserver::IsInternalRequest(
            ResourceLoadObserver::IsInternalRequest(
                resource_loader_options_.initiator_info.name ==
                fetch_initiator_type_names::kInternal)));
  }
}

void WorkerMainScriptLoader::OnConnectionClosed() {
  if (!has_received_completion_) {
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    return;
  }
}

void WorkerMainScriptLoader::HandleRedirections(
    std::vector<net::RedirectInfo>& redirect_infos,
    std::vector<network::mojom::URLResponseHeadPtr>& redirect_responses) {
  DCHECK_EQ(redirect_infos.size(), redirect_responses.size());
  for (size_t i = 0; i < redirect_infos.size(); ++i) {
    auto& redirect_info = redirect_infos[i];
    auto& redirect_response = redirect_responses[i];
    last_request_url_ = KURL(redirect_info.new_url);
    resource_load_info_notifier_wrapper_->NotifyResourceRedirectReceived(
        redirect_info, std::move(redirect_response));
  }
}

}  // namespace blink
