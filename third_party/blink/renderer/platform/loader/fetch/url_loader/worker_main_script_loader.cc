// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader.h"

#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader_client.h"

namespace blink {

void WorkerMainScriptLoader::Start(
    FetchParameters& fetch_params,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    FetchContext* fetch_context,
    ResourceLoadObserver* resource_loade_observer,
    CrossVariantMojoRemote<mojom::ResourceLoadInfoNotifierInterfaceBase>
        resource_load_info_notifier,
    WorkerMainScriptLoaderClient* client) {
  DCHECK(resource_loade_observer);
  DCHECK(client);
  initial_request_.CopyFrom(fetch_params.GetResourceRequest());
  resource_loader_options_ = fetch_params.Options();
  initial_request_url_ = fetch_params.GetResourceRequest().Url();
  last_request_url_ = initial_request_url_;
  resource_load_observer_ = resource_loade_observer;
  fetch_context_ = fetch_context;
  client_ = client;
  resource_load_info_ = blink::mojom::ResourceLoadInfo::New();

  // |resource_load_info_notifier| is valid when PlzDedicatedWorker.
  if (resource_load_info_notifier) {
    DCHECK(base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
    resource_loader_info_notifier_.Bind(std::move(resource_load_info_notifier));
  }

  // TODO(crbug.com/929370): Support CSP check to post violation reports for
  // worker top-level scripts, if off-the-main-thread fetch is enabled.

  resource_load_observer_->WillSendRequest(
      initial_request_.InspectorId(), initial_request_,
      /*redirect_response=*/ResourceResponse(), ResourceType::kScript,
      resource_loader_options_.initiator_info);

  if (!worker_main_script_load_params->redirect_responses.empty()) {
    HandleRedirections(worker_main_script_load_params->redirect_infos,
                       worker_main_script_load_params->redirect_responses);
  }

  WebURLResponse response;
  auto response_head = std::move(worker_main_script_load_params->response_head);
  Platform::Current()->PopulateURLResponse(
      WebURL(last_request_url_), *response_head, &response,
      response_head->ssl_info.has_value(), /*request_id=*/-1);
  resource_response_ = response.ToResourceResponse();
  NotifyResponseReceived(std::move(response_head));

  if (resource_response_.IsHTTP() &&
      !cors::IsOkStatus(resource_response_.HttpStatusCode())) {
    client_->OnFailedLoadingWorkerMainScript();
    resource_load_observer_->DidFailLoading(
        initial_request_.Url(), initial_request_.InspectorId(),
        ResourceError(net::ERR_FAILED, last_request_url_, base::nullopt),
        resource_response_.EncodedDataLength(),
        ResourceLoadObserver::IsInternalRequest(
            resource_loader_options_.initiator_info.name ==
            fetch_initiator_type_names::kInternal));
    return;
  }

  resource_load_observer_->DidReceiveResponse(
      initial_request_.InspectorId(), initial_request_, resource_response_,
      /*resource=*/nullptr,
      ResourceLoadObserver::ResponseSource::kNotFromMemoryCache);
  script_encoding_ =
      resource_response_.TextEncodingName().IsEmpty()
          ? UTF8Encoding()
          : WTF::TextEncoding(resource_response_.TextEncodingName());

  url_loader_remote_.Bind(std::move(
      worker_main_script_load_params->url_loader_client_endpoints->url_loader));
  receiver_.Bind(
      std::move(worker_main_script_load_params->url_loader_client_endpoints
                    ->url_loader_client));
  receiver_.set_disconnect_handler(base::BindOnce(
      &WorkerMainScriptLoader::OnConnectionClosed, base::Unretained(this)));
  data_pipe_ = std::move(worker_main_script_load_params->response_body);

  client_->OnStartLoadingBody(resource_response_);
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

void WorkerMainScriptLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void WorkerMainScriptLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void WorkerMainScriptLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void WorkerMainScriptLoader::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {}

void WorkerMainScriptLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
}

void WorkerMainScriptLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle handle) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void WorkerMainScriptLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK)
    has_seen_end_of_data_ = true;

  // Reports resource timing info for the worker main script.
  scoped_refptr<ResourceTimingInfo> timing_info =
      ResourceTimingInfo::Create(g_empty_atom, base::TimeTicks::Now(),
                                 initial_request_.GetRequestContext(),
                                 initial_request_.GetRequestDestination());
  const int64_t encoded_data_length = resource_response_.EncodedDataLength();
  timing_info->SetInitialURL(initial_request_url_);
  timing_info->SetFinalResponse(resource_response_);
  timing_info->SetLoadResponseEnd(status.completion_time);
  timing_info->AddFinalTransferSize(
      encoded_data_length == -1 ? 0 : encoded_data_length);
  fetch_context_->AddResourceTiming(*timing_info);

  has_received_completion_ = true;
  status_ = status;
  NotifyCompletionIfAppropriate();
}

SingleCachedMetadataHandler*
WorkerMainScriptLoader::CreateCachedMetadataHandler() {
  // Currently we support the metadata caching only for HTTP family.
  if (!initial_request_url_.ProtocolIsInHTTPFamily() ||
      !resource_response_.CurrentRequestUrl().ProtocolIsInHTTPFamily()) {
    return nullptr;
  }

  std::unique_ptr<CachedMetadataSender> cached_metadata_sender =
      CachedMetadataSender::Create(
          resource_response_, blink::mojom::CodeCacheType::kJavascript,
          SecurityOrigin::Create(initial_request_url_));
  return MakeGarbageCollected<ScriptCachedMetadataHandler>(
      script_encoding_, std::move(cached_metadata_sender));
}

void WorkerMainScriptLoader::Trace(Visitor* visitor) const {
  visitor->Trace(fetch_context_);
  visitor->Trace(resource_load_observer_);
  visitor->Trace(client_);
}

void WorkerMainScriptLoader::StartLoadingBody() {
  // Loading body may be cancelled before starting by calling |Cancel()|.
  if (has_cancelled_)
    return;

  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
  MojoResult rv =
      watcher_->Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                      base::BindRepeating(&WorkerMainScriptLoader::OnReadable,
                                          base::Unretained(this)));
  DCHECK_EQ(MOJO_RESULT_OK, rv);
  watcher_->ArmOrNotify();
}

void WorkerMainScriptLoader::OnReadable(MojoResult) {
  // It isn't necessary to handle MojoResult here since BeginReadDataRaw()
  // returns an equivalent error.
  const char* buffer = nullptr;
  uint32_t bytes_read = 0;
  MojoResult rv =
      data_pipe_->BeginReadData(reinterpret_cast<const void**>(&buffer),
                                &bytes_read, MOJO_READ_DATA_FLAG_NONE);
  switch (rv) {
    case MOJO_RESULT_BUSY:
    case MOJO_RESULT_INVALID_ARGUMENT:
      NOTREACHED();
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

  if (bytes_read > 0) {
    base::span<const char> span = base::make_span(buffer, bytes_read);
    client_->DidReceiveData(span);
    resource_load_observer_->DidReceiveData(initial_request_.InspectorId(),
                                            span);
  }

  rv = data_pipe_->EndReadData(bytes_read);
  DCHECK_EQ(rv, MOJO_RESULT_OK);
  watcher_->ArmOrNotify();
}

void WorkerMainScriptLoader::NotifyCompletionIfAppropriate() {
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;

  data_pipe_.reset();
  watcher_->Cancel();
  NotifyCompleteReceived(status_);

  if (!client_)
    return;
  WorkerMainScriptLoaderClient* client = client_.Get();
  client_.Clear();

  if (status_.error_code == net::OK) {
    client->OnFinishedLoadingWorkerMainScript();
    resource_load_observer_->DidFinishLoading(
        initial_request_.InspectorId(), base::TimeTicks::Now(),
        resource_response_.EncodedDataLength(),
        resource_response_.DecodedBodyLength(),
        /*should_report_corb_blocking=*/false);
  } else {
    client->OnFailedLoadingWorkerMainScript();
    resource_load_observer_->DidFailLoading(
        last_request_url_, initial_request_.InspectorId(),
        ResourceError(status_.error_code, last_request_url_, base::nullopt),
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

    std::unique_ptr<ResourceRequest> new_request =
        initial_request_.CreateRedirectRequest(
            KURL(redirect_info.new_url),
            AtomicString::FromUTF8(redirect_info.new_method.data(),
                                   redirect_info.new_method.length()),
            redirect_info.new_site_for_cookies,
            AtomicString::FromUTF8(redirect_info.new_referrer.data(),
                                   redirect_info.new_referrer.length()),
            ReferrerUtils::NetToMojoReferrerPolicy(
                redirect_info.new_referrer_policy),
            /*skip_service_worker=*/false);
    WebURLResponse response;
    Platform::Current()->PopulateURLResponse(
        WebURL(last_request_url_), *redirect_response, &response,
        redirect_response->ssl_info.has_value(), /*request_id=*/-1);
    resource_load_observer_->WillSendRequest(
        new_request->InspectorId(), *new_request, response.ToResourceResponse(),
        ResourceType::kScript, resource_loader_options_.initiator_info);

    NotifyRedirectionReceived(std::move(redirect_response), redirect_info);
  }
}

void WorkerMainScriptLoader::NotifyResponseReceived(
    network::mojom::URLResponseHeadPtr response_head) {
  if (resource_loader_info_notifier_) {
    resource_load_info_->mime_type = response_head->mime_type;
    resource_load_info_->load_timing_info = response_head->load_timing;
    resource_load_info_->network_info = blink::mojom::CommonNetworkInfo::New();
    resource_load_info_->network_info->network_accessed =
        response_head->network_accessed;
    resource_load_info_->network_info->always_access_network =
        network_utils::AlwaysAccessNetwork(response_head->headers);
    resource_load_info_->network_info->remote_endpoint =
        response_head->remote_endpoint;
    resource_loader_info_notifier_->NotifyResourceResponseReceived(
        resource_load_info_.Clone(), std::move(response_head),
        PreviewsTypes::kPreviewsUnspecified);
  }
}

void WorkerMainScriptLoader::NotifyRedirectionReceived(
    network::mojom::URLResponseHeadPtr redirect_response,
    const net::RedirectInfo& redirect_info) {
  if (resource_loader_info_notifier_) {
    resource_load_info_->final_url = redirect_info.new_url;
    resource_load_info_->method = redirect_info.new_method;
    resource_load_info_->referrer = GURL(redirect_info.new_referrer);
    blink::mojom::RedirectInfoPtr net_redirect_info =
        blink::mojom::RedirectInfo::New();
    net_redirect_info->origin_of_new_url =
        url::Origin::Create(redirect_info.new_url);
    net_redirect_info->network_info = blink::mojom::CommonNetworkInfo::New();
    net_redirect_info->network_info->network_accessed =
        redirect_response->network_accessed;
    net_redirect_info->network_info->always_access_network =
        network_utils::AlwaysAccessNetwork(redirect_response->headers);
    net_redirect_info->network_info->remote_endpoint =
        redirect_response->remote_endpoint;
    resource_load_info_->redirect_info_chain.push_back(
        std::move(net_redirect_info));
  }
}

void WorkerMainScriptLoader::NotifyCompleteReceived(
    const network::URLLoaderCompletionStatus& status) {
  if (resource_loader_info_notifier_) {
    resource_load_info_->network_info = blink::mojom::CommonNetworkInfo::New();
    resource_load_info_->original_url = initial_request_url_;
    resource_load_info_->request_destination =
        initial_request_.GetRequestDestination();
    resource_load_info_->was_cached = status.exists_in_cache;
    resource_load_info_->net_error = status.error_code;
    resource_load_info_->total_received_bytes = status.encoded_data_length;
    resource_load_info_->raw_body_bytes = status.encoded_body_length;

    // |resource_load_info_| is going to be transferred because it's the last
    // notification during loading the script.
    resource_loader_info_notifier_->NotifyResourceLoadCompleted(
        std::move(resource_load_info_), status);
  }
}

}  // namespace blink
