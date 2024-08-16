// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"

#include "base/unguessable_token.h"
#include "net/base/request_priority.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_names.h"

namespace {

::blink::ResourceLoadPriority ConvertRequestPriorityToResourceLoadPriority(
    net::RequestPriority priority) {
  switch (priority) {
    case net::RequestPriority::THROTTLED:
      break;
    case net::RequestPriority::IDLE:
      return ::blink::ResourceLoadPriority::kVeryLow;
    case net::RequestPriority::LOWEST:
      return ::blink::ResourceLoadPriority::kLow;
    case net::RequestPriority::LOW:
      return ::blink::ResourceLoadPriority::kMedium;
    case net::RequestPriority::MEDIUM:
      return ::blink::ResourceLoadPriority::kHigh;
    case net::RequestPriority::HIGHEST:
      return ::blink::ResourceLoadPriority::kVeryHigh;
  }

  NOTREACHED_IN_MIGRATION() << priority;
  return blink::ResourceLoadPriority::kUnresolved;
}

}  // namespace

namespace blink {

namespace {

bool IsExcludedHeaderForServiceWorkerFetchEvent(const String& header_name) {
  // Excluding Sec-Fetch-... headers as suggested in
  // https://crbug.com/949997#c4.
  if (header_name.StartsWithIgnoringASCIICase("sec-fetch-")) {
    return true;
  }

  return false;
}

void SignalError(Persistent<DataPipeBytesConsumer::CompletionNotifier> notifier,
                 uint32_t reason,
                 const std::string& description) {
  notifier->SignalError(BytesConsumer::Error());
}

void SignalSize(
    std::unique_ptr<mojo::Remote<network::mojom::blink::ChunkedDataPipeGetter>>,
    Persistent<DataPipeBytesConsumer::CompletionNotifier> notifier,
    int32_t status,
    uint64_t size) {
  if (status != 0) {
    // error case
    notifier->SignalError(BytesConsumer::Error());
    return;
  }
  notifier->SignalSize(size);
}

}  // namespace

FetchRequestData* FetchRequestData::Create(
    ScriptState* script_state,
    mojom::blink::FetchAPIRequestPtr fetch_api_request,
    ForServiceWorkerFetchEvent for_service_worker_fetch_event) {
  DCHECK(fetch_api_request);
  FetchRequestData* request = MakeGarbageCollected<FetchRequestData>(
      script_state ? ExecutionContext::From(script_state) : nullptr);
  request->url_ = fetch_api_request->url;
  request->method_ = AtomicString(fetch_api_request->method);
  for (const auto& pair : fetch_api_request->headers) {
    // TODO(leonhsl): Check sources of |fetch_api_request.headers| to make clear
    // whether we really need this filter.
    if (EqualIgnoringASCIICase(pair.key, "referer"))
      continue;
    if (for_service_worker_fetch_event == ForServiceWorkerFetchEvent::kTrue &&
        IsExcludedHeaderForServiceWorkerFetchEvent(pair.key)) {
      continue;
    }
    request->header_list_->Append(pair.key, pair.value);
  }

  if (fetch_api_request->blob) {
    DCHECK(fetch_api_request->body.IsEmpty());
    request->SetBuffer(
        BodyStreamBuffer::Create(
            script_state,
            MakeGarbageCollected<BlobBytesConsumer>(
                ExecutionContext::From(script_state), fetch_api_request->blob),
            nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr),
        fetch_api_request->blob->size());
  } else if (fetch_api_request->body.FormBody()) {
    request->SetBuffer(
        BodyStreamBuffer::Create(script_state,
                                 MakeGarbageCollected<FormDataBytesConsumer>(
                                     ExecutionContext::From(script_state),
                                     fetch_api_request->body.FormBody()),
                                 nullptr /* AbortSignal */,
                                 /*cached_metadata_handler=*/nullptr),
        fetch_api_request->body.FormBody()->SizeInBytes());
  } else if (fetch_api_request->body.StreamBody()) {
    mojo::ScopedDataPipeConsumerHandle readable;
    mojo::ScopedDataPipeProducerHandle writable;
    MojoCreateDataPipeOptions options{sizeof(MojoCreateDataPipeOptions),
                                      MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1, 0};
    const MojoResult result =
        mojo::CreateDataPipe(&options, writable, readable);
    if (result == MOJO_RESULT_OK) {
      DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
      // Explicitly creating a ReadableStream here in order to remember
      // that the request is created from a ReadableStream.
      auto* stream =
          BodyStreamBuffer::Create(
              script_state,
              MakeGarbageCollected<DataPipeBytesConsumer>(
                  ExecutionContext::From(script_state)
                      ->GetTaskRunner(TaskType::kNetworking),
                  std::move(readable), &completion_notifier),
              /*AbortSignal=*/nullptr, /*cached_metadata_handler=*/nullptr)
              ->Stream();
      request->SetBuffer(
          MakeGarbageCollected<BodyStreamBuffer>(script_state, stream,
                                                 /*AbortSignal=*/nullptr));

      auto body_remote = std::make_unique<
          mojo::Remote<network::mojom::blink::ChunkedDataPipeGetter>>(
          fetch_api_request->body.TakeStreamBody());
      body_remote->set_disconnect_with_reason_handler(
          WTF::BindOnce(SignalError, WrapPersistent(completion_notifier)));
      auto* body_remote_raw = body_remote.get();
      (*body_remote_raw)
          ->GetSize(WTF::BindOnce(SignalSize, std::move(body_remote),
                                  WrapPersistent(completion_notifier)));
      (*body_remote_raw)->StartReading(std::move(writable));
    } else {
      request->SetBuffer(BodyStreamBuffer::Create(
          script_state, BytesConsumer::CreateErrored(BytesConsumer::Error()),
          nullptr /* AbortSignal */, /*cached_metadata_handler=*/nullptr));
    }
  }

  // Context is always set to FETCH later, so we don't copy it
  // from fetch_api_request here.
  // TODO(crbug.com/1045925): Remove this comment too when
  // we deprecate SetContext.

  request->SetDestination(fetch_api_request->destination);
  if (fetch_api_request->request_initiator)
    request->SetOrigin(fetch_api_request->request_initiator);
  request->SetNavigationRedirectChain(
      fetch_api_request->navigation_redirect_chain);
  request->SetReferrerString(AtomicString(Referrer::NoReferrer()));
  if (fetch_api_request->referrer) {
    if (!fetch_api_request->referrer->url.IsEmpty()) {
      request->SetReferrerString(
          AtomicString(fetch_api_request->referrer->url));
    }
    request->SetReferrerPolicy(fetch_api_request->referrer->policy);
  }
  request->SetMode(fetch_api_request->mode);
  request->SetTargetAddressSpace(fetch_api_request->target_address_space);
  request->SetCredentials(fetch_api_request->credentials_mode);
  request->SetCacheMode(fetch_api_request->cache_mode);
  request->SetRedirect(fetch_api_request->redirect_mode);
  request->SetMimeType(request->header_list_->ExtractMIMEType());
  request->SetIntegrity(fetch_api_request->integrity);
  request->SetKeepalive(fetch_api_request->keepalive);
  request->SetIsHistoryNavigation(fetch_api_request->is_history_navigation);
  request->SetPriority(ConvertRequestPriorityToResourceLoadPriority(
      fetch_api_request->priority));
  if (fetch_api_request->fetch_window_id)
    request->SetWindowId(fetch_api_request->fetch_window_id.value());

  if (fetch_api_request->trust_token_params) {
    if (script_state) {
      // script state might be null for some tests
      DCHECK(RuntimeEnabledFeatures::PrivateStateTokensEnabled(
          ExecutionContext::From(script_state)));
    }
    std::optional<network::mojom::blink::TrustTokenParams> trust_token_params =
        std::move(*(fetch_api_request->trust_token_params->Clone().get()));
    request->SetTrustTokenParams(trust_token_params);
  }

  request->SetAttributionReportingEligibility(
      fetch_api_request->attribution_reporting_eligibility);
  request->SetAttributionReportingSupport(
      fetch_api_request->attribution_reporting_support);

  if (fetch_api_request->service_worker_race_network_request_token) {
    request->SetServiceWorkerRaceNetworkRequestToken(
        fetch_api_request->service_worker_race_network_request_token.value());
  }

  return request;
}

FetchRequestData* FetchRequestData::CloneExceptBody() {
  auto* request = MakeGarbageCollected<FetchRequestData>(execution_context_);
  request->url_ = url_;
  request->method_ = method_;
  request->header_list_ = header_list_->Clone();
  request->origin_ = origin_;
  request->navigation_redirect_chain_ = navigation_redirect_chain_;
  request->isolated_world_origin_ = isolated_world_origin_;
  request->destination_ = destination_;
  request->referrer_string_ = referrer_string_;
  request->referrer_policy_ = referrer_policy_;
  request->mode_ = mode_;
  request->target_address_space_ = target_address_space_;
  request->credentials_ = credentials_;
  request->cache_mode_ = cache_mode_;
  request->redirect_ = redirect_;
  request->mime_type_ = mime_type_;
  request->integrity_ = integrity_;
  request->priority_ = priority_;
  request->fetch_priority_hint_ = fetch_priority_hint_;
  request->original_destination_ = original_destination_;
  request->keepalive_ = keepalive_;
  request->browsing_topics_ = browsing_topics_;
  request->ad_auction_headers_ = ad_auction_headers_;
  request->shared_storage_writable_ = shared_storage_writable_;
  request->is_history_navigation_ = is_history_navigation_;
  request->window_id_ = window_id_;
  request->trust_token_params_ = trust_token_params_;
  request->attribution_reporting_eligibility_ =
      attribution_reporting_eligibility_;
  request->attribution_reporting_support_ = attribution_reporting_support_;
  request->service_worker_race_network_request_token_ =
      service_worker_race_network_request_token_;
  return request;
}

FetchRequestData* FetchRequestData::Clone(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  FetchRequestData* request = FetchRequestData::CloneExceptBody();
  if (request->service_worker_race_network_request_token_) {
    request->service_worker_race_network_request_token_ =
        base::UnguessableToken::Null();
  }
  if (buffer_) {
    BodyStreamBuffer* new1 = nullptr;
    BodyStreamBuffer* new2 = nullptr;
    buffer_->Tee(&new1, &new2, exception_state);
    if (exception_state.HadException())
      return nullptr;
    buffer_ = new1;
    request->buffer_ = new2;
    request->buffer_byte_length_ = buffer_byte_length_;
  }
  if (url_loader_factory_.is_bound()) {
    url_loader_factory_->Clone(
        request->url_loader_factory_.BindNewPipeAndPassReceiver(
            ExecutionContext::From(script_state)
                ->GetTaskRunner(TaskType::kNetworking)));
  }
  return request;
}

FetchRequestData* FetchRequestData::Pass(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  FetchRequestData* request = FetchRequestData::CloneExceptBody();
  if (buffer_) {
    request->buffer_ = buffer_;
    request->buffer_byte_length_ = buffer_byte_length_;
    buffer_ = BodyStreamBuffer::Create(
        script_state, BytesConsumer::CreateClosed(), nullptr /* AbortSignal */,
        /*cached_metadata_handler=*/nullptr);
    buffer_->CloseAndLockAndDisturb(exception_state);
    buffer_byte_length_ = 0;
  }
  request->url_loader_factory_ = std::move(url_loader_factory_);
  return request;
}

FetchRequestData::~FetchRequestData() {}

FetchRequestData::FetchRequestData(ExecutionContext* execution_context)
    : referrer_string_(Referrer::ClientReferrerString()),
      url_loader_factory_(execution_context),
      execution_context_(execution_context) {}

void FetchRequestData::Trace(Visitor* visitor) const {
  visitor->Trace(buffer_);
  visitor->Trace(header_list_);
  visitor->Trace(url_loader_factory_);
  visitor->Trace(execution_context_);
}

}  // namespace blink
