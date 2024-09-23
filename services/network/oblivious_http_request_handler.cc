// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/oblivious_http_request_handler.h"

#include <algorithm>
#include <array>

#include "base/i18n/time_formatting.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/load_flags.h"
#include "net/http/http_log_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_values.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/binary_http/binary_http_message.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/common/oblivious_http_header_key_config.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "services/network/cookie_manager.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_request_helper_factory.h"

namespace network {

namespace {

constexpr size_t kMaxResponseSize =
    5 * 1024 * 1024;  // Response size limit is 5MB
constexpr base::TimeDelta kDefaultRequestTimeout = base::Minutes(1);
constexpr char kObliviousHttpRequestMimeType[] = "message/ohttp-req";

constexpr size_t kMaxMethodSize = 16;
constexpr size_t kMaxRequestBodySize =
    5 * 1024 * 1024;                               // Request size limit is 5MB
constexpr size_t kMaxContentTypeSize = 256;        // Per RFC6838

// Class wrapping quiche::ObliviousHttpClient. Should be created once for each
// request/response pair.
class StatefulObliviousHttpClient {
 public:
  static std::optional<StatefulObliviousHttpClient> CreateFromKeyConfig(
      std::string key_config_str) {
    auto key_configs =
        quiche::ObliviousHttpKeyConfigs::ParseConcatenatedKeys(key_config_str);
    if (!key_configs.ok()) {
      return std::nullopt;
    }
    quiche::ObliviousHttpHeaderKeyConfig key_config =
        key_configs->PreferredConfig();
    auto ohttp_client = quiche::ObliviousHttpClient::Create(
        key_configs->GetPublicKeyForId(key_config.GetKeyId()).value(),
        key_config);
    if (!ohttp_client.ok()) {
      return std::nullopt;
    }
    return StatefulObliviousHttpClient(std::move(*ohttp_client));
  }

  // Movable
  StatefulObliviousHttpClient(StatefulObliviousHttpClient&& other) = default;
  StatefulObliviousHttpClient& operator=(StatefulObliviousHttpClient&& other) =
      default;

  std::optional<std::string> EncryptRequest(std::string plain_text) {
    auto maybe_request = quiche_client_.CreateObliviousHttpRequest(plain_text);
    if (!maybe_request.ok()) {
      return std::nullopt;
    }
    std::string cipher_text = maybe_request->EncapsulateAndSerialize();
    context_ = std::move(*maybe_request).ReleaseContext();
    return cipher_text;
  }

  std::optional<std::string> DecryptResponse(std::string cipher_text) {
    DCHECK(context_) << "Decrypt called before Encrypt";
    auto maybe_response = quiche_client_.DecryptObliviousHttpResponse(
        cipher_text, context_.value());
    if (!maybe_response.ok()) {
      return std::nullopt;
    }
    return std::string(maybe_response->GetPlaintextData());
  }

 private:
  explicit StatefulObliviousHttpClient(
      quiche::ObliviousHttpClient quiche_client)
      : quiche_client_(std::move(quiche_client)) {}

  quiche::ObliviousHttpClient quiche_client_;
  std::optional<quiche::ObliviousHttpRequest::Context> context_;
};

std::string CreateAndSerializeBhttpMessage(
    const GURL& request_url,
    const std::string& method,
    mojom::ObliviousHttpRequestBodyPtr request_body,
    net::HttpRequestHeaders::HeaderVector headers) {
  std::string host_port = request_url.host();
  if (request_url.has_port()) {
    host_port += ":" + request_url.port();
  }

  quiche::BinaryHttpRequest bhttp_request(
      {method, request_url.scheme(), host_port, request_url.PathForRequest()});
  bhttp_request.AddHeaderField({net::HttpRequestHeaders::kHost, host_port});
  // Date should be provided by the client to allow for server anti-replay
  // protections (according to the OHTTP spec).
  bhttp_request.AddHeaderField(
      {"Date", base::TimeFormatHTTP(base::Time::Now())});
  if (request_body && !request_body->content.empty()) {
    DCHECK(!request_body->content_type.empty());
    bhttp_request.AddHeaderField({net::HttpRequestHeaders::kContentType,
                                  std::move(request_body->content_type)});
    bhttp_request.AddHeaderField(
        {net::HttpRequestHeaders::kContentLength,
         base::NumberToString(request_body->content.size())});
    bhttp_request.set_body(std::move(request_body->content));
  }
  for (const auto& header : headers) {
    bhttp_request.AddHeaderField({header.key, header.value});
  }
  return bhttp_request.Serialize().value();
}

scoped_refptr<net::HttpResponseHeaders> GetSyntheticBhttpResponseHeader(
    const std::vector<quiche::BinaryHttpRequest::Field>& bhttp_headers) {
  std::string synthetic_headers = "HTTP/1.1 200 Success\r\n";
  for (const auto& header : bhttp_headers) {
    if (!net::HttpUtil::IsValidHeaderName(header.name) ||
        !net::HttpUtil::IsValidHeaderValue(header.value)) {
      return nullptr;
    }
    synthetic_headers += header.name + ": " + header.value + "\r\n";
  }
  return net::HttpResponseHeaders::TryToCreate(synthetic_headers);
}

}  // namespace

class ObliviousHttpRequestHandler::RequestState {
 public:
  mojom::ObliviousHttpRequestPtr request;
  std::unique_ptr<SimpleURLLoader> loader;
  std::unique_ptr<TrustTokenRequestHelperFactory> trust_token_helper_factory;
  std::unique_ptr<TrustTokenRequestHelper> trust_token_helper;
  net::NetLogWithSource net_log;
  std::optional<StatefulObliviousHttpClient> ohttp_client;
};

ObliviousHttpRequestHandler::ObliviousHttpRequestHandler(
    NetworkContext* context)
    : owner_network_context_(context) {
  clients_.set_disconnect_handler(
      base::BindRepeating(&ObliviousHttpRequestHandler::OnClientDisconnect,
                          base::Unretained(this)));
}

ObliviousHttpRequestHandler::~ObliviousHttpRequestHandler() = default;

void ObliviousHttpRequestHandler::StartRequest(
    mojom::ObliviousHttpRequestPtr ohttp_request,
    mojo::PendingRemote<mojom::ObliviousHttpClient> client) {
  DCHECK(client.is_valid());
  DCHECK(ohttp_request);
  if (!ohttp_request->relay_url.is_valid() ||
      !ohttp_request->relay_url.SchemeIs(url::kHttpsScheme)) {
    mojo::ReportBadMessage("Invalid OHTTP Relay URL");
    return;
  }
  if (!ohttp_request->resource_url.is_valid() ||
      !ohttp_request->resource_url.SchemeIs(url::kHttpsScheme)) {
    mojo::ReportBadMessage("Invalid OHTTP Resource URL");
    return;
  }
  if (ohttp_request->method.size() > kMaxMethodSize) {
    mojo::ReportBadMessage("Invalid OHTTP Method");
    return;
  }
  if (!ohttp_request->traffic_annotation.is_valid()) {
    mojo::ReportBadMessage("Invalid OHTTP Traffic Annotation");
    return;
  }
  if (ohttp_request->request_body) {
    if (ohttp_request->request_body->content.size() > kMaxRequestBodySize) {
      mojo::ReportBadMessage("Request body too large");
      return;
    }
    if (ohttp_request->request_body->content_type.size() >
        kMaxContentTypeSize) {
      mojo::ReportBadMessage("Content-Type too large");
      return;
    }
  }

  mojo::RemoteSetElementId id = clients_.Add(std::move(client));
  auto state_pair_iter =
      client_state_.insert({id, std::make_unique<RequestState>()});
  RequestState* state = state_pair_iter.first->second.get();

  state->request = std::move(ohttp_request);

  state->net_log = net::NetLogWithSource::Make(
      net::NetLog::Get(), net::NetLogSourceType::URL_REQUEST);
  state->net_log.BeginEvent(net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST);

  if (state->request->trust_token_params) {
    state->trust_token_helper_factory =
        std::make_unique<TrustTokenRequestHelperFactory>(
            owner_network_context_->trust_token_store(),
            owner_network_context_->network_service()
                ->trust_token_key_commitments(),
            // It's safe to use Unretained because |owner_network_context_| is
            // guaranteed to outlive the ObliviousHttpRequestHandler, which owns
            // state, which owns the TrustTokenRequestHelperFactory.
            base::BindRepeating(&NetworkContext::client,
                                base::Unretained(owner_network_context_)),
            // It's safe to bind to |state| pointer. Callback is owned by the
            // |state->trust_token_helper_factory|.
            base::BindRepeating(
                [](NetworkContext* context, RequestState* state) {
                  // Trust tokens will be blocked if the user has either
                  // disabled the anti-abuse content setting or blocked the
                  // issuer site from storing data (i.e. the cookie content
                  // setting for that site is blocked).
                  bool is_allowed = context->cookie_manager()
                                        ->cookie_settings()
                                        .ArePrivateStateTokensAllowed(
                                            state->request->resource_url);
                  return (is_allowed && !context->are_trust_tokens_blocked());
                },
                base::Unretained(owner_network_context_), state));

    state->trust_token_helper_factory->CreateTrustTokenHelperForRequest(
        url::Origin::Create(state->request->resource_url),
        net::HttpRequestHeaders(), *state->request->trust_token_params,
        state->net_log,
        // It's safe to use Unretained because |this| owns
        // state, which owns the TrustTokenRequestHelperFactory.
        base::BindOnce(
            &ObliviousHttpRequestHandler::OnDoneConstructingTrustTokenHelper,
            base::Unretained(this), id));
    return;
  }
  ContinueHandlingRequest(/*headers=*/std::nullopt, id);
}

void ObliviousHttpRequestHandler::OnDoneConstructingTrustTokenHelper(
    mojo::RemoteSetElementId id,
    TrustTokenStatusOrRequestHelper status_or_helper) {
  if (!status_or_helper.ok()) {
    RespondWithError(id, net::ERR_TRUST_TOKEN_OPERATION_FAILED,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }

  auto state_iter = client_state_.find(id);
  CHECK(state_iter != client_state_.end(), base::NotFatalUntil::M130);

  RequestState* state = state_iter->second.get();
  state->trust_token_helper = status_or_helper.TakeOrCrash();

  state->trust_token_helper->Begin(
      state->request->resource_url,
      base::BindOnce(
          &ObliviousHttpRequestHandler::OnDoneBeginningTrustTokenOperation,
          base::Unretained(this), id));
}

void ObliviousHttpRequestHandler::OnDoneBeginningTrustTokenOperation(
    mojo::RemoteSetElementId id,
    std::optional<net::HttpRequestHeaders> headers,
    mojom::TrustTokenOperationStatus status) {
  if (status != mojom::TrustTokenOperationStatus::kOk) {
    RespondWithError(id, net::ERR_TRUST_TOKEN_OPERATION_FAILED,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }
  ContinueHandlingRequest(std::move(headers), id);
}

void ObliviousHttpRequestHandler::ContinueHandlingRequest(
    std::optional<net::HttpRequestHeaders> headers,
    mojo::RemoteSetElementId id) {
  auto state_iter = client_state_.find(id);
  CHECK(state_iter != client_state_.end(), base::NotFatalUntil::M130);
  RequestState* state = state_iter->second.get();

  std::string bhttp_payload = CreateAndSerializeBhttpMessage(
      state->request->resource_url, state->request->method,
      std::move(state->request->request_body),
      std::move(headers.value_or(net::HttpRequestHeaders()).GetHeaderVector()));

  state->net_log.AddEvent(
      net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST_DATA,
      [&](net::NetLogCaptureMode capture_mode) {
        base::Value::Dict dict;
        dict.Set("byte_count", static_cast<int>(bhttp_payload.size()));
        if (net::NetLogCaptureIncludesSocketBytes(capture_mode)) {
          dict.Set("bytes", net::NetLogBinaryValue(bhttp_payload.data(),
                                                   bhttp_payload.size()));
        }
        return dict;
      });

  // Padding
  size_t padded_length = bhttp_payload.size();
  if (state->request->padding_params) {
    if (state->request->padding_params->add_exponential_pad) {
      double uniform_rand = base::RandDouble();
      // Map the uniform random to exponential distribution.
      // The CDF for exponential is `CDF(x) = 1-exp(-x/mu)`
      // So the inverse CDF is `x = -LN(1-CDF(x))*mu`.
      size_t pad_length =
          std::ceil(-std::log(1 - uniform_rand) *
                    state->request->padding_params->exponential_mean);
      padded_length += pad_length;
    }
    if (state->request->padding_params->pad_to_next_power_of_two) {
      size_t new_size = 1;
      while (new_size < padded_length) {
        new_size <<= 1;
      }
      padded_length = new_size;
    }
  }
  std::string padded_payload;
  if (padded_length > bhttp_payload.size()) {
    // Zero pad payload up to padded_length.
    padded_payload =
        bhttp_payload + std::string(padded_length - bhttp_payload.size(), '\0');
  } else {
    DCHECK_EQ(bhttp_payload.size(), padded_length);
    padded_payload = std::move(bhttp_payload);
  }

  // Encrypt
  auto maybe_client = StatefulObliviousHttpClient::CreateFromKeyConfig(
      std::move(state->request->key_config));
  if (!maybe_client) {
    RespondWithError(id, net::ERR_INVALID_ARGUMENT,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }
  state->ohttp_client = std::move(maybe_client);
  auto maybe_encrypted_blob =
      state->ohttp_client->EncryptRequest(std::move(padded_payload));
  if (!maybe_encrypted_blob) {
    RespondWithError(id, net::ERR_FAILED,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }

  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>();
  resource_request->url = state->request->relay_url;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->redirect_mode = mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags |= net::LOAD_DISABLE_CACHE;
  resource_request->net_log_create_info = state->net_log.source();

  state->loader = SimpleURLLoader::Create(
      std::move(resource_request),
      net::NetworkTrafficAnnotationTag(state->request->traffic_annotation));

  state->loader->AttachStringForUpload(*maybe_encrypted_blob,
                                       kObliviousHttpRequestMimeType);
  state->loader->SetTimeoutDuration(state->request->timeout_duration
                                        ? *state->request->timeout_duration
                                        : kDefaultRequestTimeout);
  state->loader->DownloadToString(
      GetURLLoaderFactory(),
      base::BindOnce(&ObliviousHttpRequestHandler::OnRequestComplete,
                     base::Unretained(this), id),
      kMaxResponseSize);
}

void ObliviousHttpRequestHandler::RespondWithError(
    mojo::RemoteSetElementId id,
    int error_code,
    std::optional<int> outer_response_error_code) {
  mojom::ObliviousHttpClient* client = clients_.Get(id);
  auto state_iter = client_state_.find(id);
  DCHECK(client);
  CHECK(state_iter != client_state_.end(), base::NotFatalUntil::M130);
  RequestState* state = state_iter->second.get();
  state->net_log.EndEvent(net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST, [&] {
    base::Value::Dict params;
    params.Set("net_error", error_code);
    if (outer_response_error_code) {
      params.Set("outer_response_error_code",
                 outer_response_error_code.value());
    }
    return params;
  });

  network::mojom::ObliviousHttpCompletionResultPtr response_result;
  if (outer_response_error_code) {
    DCHECK_NE(outer_response_error_code.value(), net::HTTP_OK);
    response_result = network::mojom::ObliviousHttpCompletionResult::
        NewOuterResponseErrorCode(outer_response_error_code.value());
  } else {
    response_result =
        network::mojom::ObliviousHttpCompletionResult::NewNetError(error_code);
  }

  client->OnCompleted(std::move(response_result));
  clients_.Remove(id);

  DCHECK_EQ(1u, client_state_.count(id));
  client_state_.erase(id);
}

void ObliviousHttpRequestHandler::OnRequestComplete(
    mojo::RemoteSetElementId id,
    std::unique_ptr<std::string> response) {
  auto state_iter = client_state_.find(id);
  CHECK(state_iter != client_state_.end(), base::NotFatalUntil::M130);

  RequestState* state = state_iter->second.get();
  if (!response) {
    std::optional<int> outer_response_error_code;
    if (state->loader->NetError() == net::ERR_HTTP_RESPONSE_CODE_FAILURE &&
        state->loader->ResponseInfo() &&
        state->loader->ResponseInfo()->headers) {
      outer_response_error_code =
          state->loader->ResponseInfo()->headers->response_code();
    }
    RespondWithError(id, state->loader->NetError(), outer_response_error_code);
    return;
  }

  auto maybe_payload =
      state->ohttp_client->DecryptResponse(std::move(*response));
  if (!maybe_payload) {
    RespondWithError(id, net::ERR_INVALID_RESPONSE,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }

  state->net_log.AddEvent(
      net::NetLogEventType::OBLIVIOUS_HTTP_RESPONSE_DATA,
      [&](net::NetLogCaptureMode capture_mode) {
        base::Value::Dict dict;
        dict.Set("byte_count", static_cast<int>(maybe_payload->size()));
        if (net::NetLogCaptureIncludesSocketBytes(capture_mode)) {
          dict.Set("bytes", net::NetLogBinaryValue(maybe_payload->data(),
                                                   maybe_payload->size()));
        }
        return dict;
      });

  // Parse the inner request.
  auto bhttp_response = quiche::BinaryHttpResponse::Create(*maybe_payload);
  if (!bhttp_response.ok()) {
    RespondWithError(id, net::ERR_INVALID_RESPONSE,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }

  int inner_status_code = bhttp_response->status_code();
  scoped_refptr<net::HttpResponseHeaders> headers =
      GetSyntheticBhttpResponseHeader(bhttp_response->GetHeaderFields());
  // Check that the header is valid.
  if (!headers) {
    RespondWithError(id, net::ERR_INVALID_RESPONSE,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }

  if (state->trust_token_helper) {
    state->trust_token_helper->Finalize(
        *headers,
        base::BindOnce(
            &ObliviousHttpRequestHandler::OnDoneFinalizingTrustTokenOperation,
            base::Unretained(this), id, inner_status_code, headers,
            std::string(bhttp_response->body())));
    return;
  }

  NotifyComplete(id, inner_status_code, std::move(headers),
                 std::string(bhttp_response->body()));
}

void ObliviousHttpRequestHandler::OnDoneFinalizingTrustTokenOperation(
    mojo::RemoteSetElementId id,
    int inner_response_code,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::string body,
    mojom::TrustTokenOperationStatus status) {
  if (status != mojom::TrustTokenOperationStatus::kOk) {
    RespondWithError(id, net::ERR_TRUST_TOKEN_OPERATION_FAILED,
                     /*outer_response_error_code=*/std::nullopt);
    return;
  }
  NotifyComplete(id, inner_response_code, std::move(headers), std::move(body));
}

void ObliviousHttpRequestHandler::NotifyComplete(
    mojo::RemoteSetElementId id,
    int inner_response_code,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::string body) {
  mojom::ObliviousHttpClient* client = clients_.Get(id);
  auto state_iter = client_state_.find(id);
  DCHECK(client);
  CHECK(state_iter != client_state_.end(), base::NotFatalUntil::M130);
  RequestState* state = state_iter->second.get();
  net::NetLogResponseHeaders(
      state->net_log, net::NetLogEventType::OBLIVIOUS_HTTP_RESPONSE_HEADERS,
      headers.get());
  state->net_log.EndEvent(net::NetLogEventType::OBLIVIOUS_HTTP_REQUEST, [&] {
    base::Value::Dict params;
    params.Set("net_error", net::OK);
    params.Set("inner_response_code", inner_response_code);
    return params;
  });

  network::mojom::ObliviousHttpResponsePtr response =
      network::mojom::ObliviousHttpResponse::New();
  response->response_code = inner_response_code;
  response->response_body = std::move(body);
  response->headers = std::move(headers);
  network::mojom::ObliviousHttpCompletionResultPtr response_result =
      network::mojom::ObliviousHttpCompletionResult::NewInnerResponse(
          std::move(response));

  client->OnCompleted(std::move(response_result));
  clients_.Remove(id);
  client_state_.erase(id);
}

void ObliviousHttpRequestHandler::OnClientDisconnect(
    mojo::RemoteSetElementId id) {
  DCHECK_GT(client_state_.count(id), 0u);
  client_state_.erase(id);
}

network::mojom::URLLoaderFactory*
ObliviousHttpRequestHandler::GetURLLoaderFactory() {
  // Create the URLLoaderFactory as needed.
  if (url_loader_factory_ && url_loader_factory_.is_connected()) {
    return url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_orb_enabled = false;
  params->is_trusted = true;
  params->automatically_assign_isolation_info = true;

  url_loader_factory_.reset();
  owner_network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));

  return url_loader_factory_.get();
}

}  // namespace network
