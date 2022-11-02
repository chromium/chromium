// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/oblivious_http_request_handler.h"

#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/third_party/quiche/src/quiche/binary_http/binary_http_message.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/oblivious_http_request.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

namespace {

constexpr size_t kMaxResponseSize = 10 * 1024;  // Response size limit is 10kB
constexpr base::TimeDelta kRequestTimeout = base::Minutes(1);
constexpr char kObliviousHttpRequestMimeType[] = "message/ohttp-req";

constexpr size_t kMaxMethodSize = 16;
constexpr size_t kMaxRequestBodySize = 10 * 1024;  // Request size limit is 10kB
constexpr size_t kMaxContentTypeSize = 256;        // Per RFC6838

std::string CreateAndSerializeBhttpMessage(
    const GURL& request_url,
    const std::string& method,
    absl::optional<std::string> content_type,
    absl::optional<std::string> content_data) {
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
  if (content_data) {
    DCHECK(content_type);
    bhttp_request.AddHeaderField(
        {net::HttpRequestHeaders::kContentType, std::move(*content_type)});
    bhttp_request.AddHeaderField({net::HttpRequestHeaders::kContentLength,
                                  base::NumberToString(content_data->size())});
    bhttp_request.set_body(std::move(*content_data));
  }

  return bhttp_request.Serialize().value();
}

}  // namespace

class ObliviousHttpRequestHandler::RequestState {
 public:
  std::unique_ptr<SimpleURLLoader> loader;
  // Crypto stuff that needs to last from request to response.
  // TrustToken handler
};

ObliviousHttpRequestHandler::ObliviousHttpRequestHandler(
    mojom::NetworkContext* context)
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

  std::string bhttp_payload = CreateAndSerializeBhttpMessage(
      ohttp_request->resource_url, ohttp_request->method,
      std::move(ohttp_request->request_body->content_type),
      std::move(ohttp_request->request_body->content));

  // TODO(behamilton): handle encryption, padding.
  std::string unencrypted_blob = bhttp_payload;

  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>();
  resource_request->url = ohttp_request->relay_url;
  resource_request->method = net::HttpRequestHeaders::kPostMethod;
  resource_request->redirect_mode = mojom::RedirectMode::kError;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->load_flags |= net::LOAD_DISABLE_CACHE;

  state->loader = SimpleURLLoader::Create(
      std::move(resource_request),
      net::NetworkTrafficAnnotationTag(ohttp_request->traffic_annotation));

  state->loader->AttachStringForUpload(unencrypted_blob,
                                       kObliviousHttpRequestMimeType);
  state->loader->SetTimeoutDuration(kRequestTimeout);
  state->loader->DownloadToString(
      GetURLLoaderFactory(),
      base::BindOnce(&ObliviousHttpRequestHandler::OnRequestComplete,
                     base::Unretained(this), id),
      kMaxResponseSize);
}

void ObliviousHttpRequestHandler::RespondWithError(mojo::RemoteSetElementId id,
                                                   int error_code) {
  mojom::ObliviousHttpClient* client = clients_.Get(id);
  DCHECK(client);
  client->OnCompleted(absl::nullopt, error_code);
  clients_.Remove(id);

  DCHECK_EQ(1u, client_state_.count(id));
  client_state_.erase(id);
}

void ObliviousHttpRequestHandler::OnRequestComplete(
    mojo::RemoteSetElementId id,
    std::unique_ptr<std::string> response) {
  mojom::ObliviousHttpClient* client = clients_.Get(id);
  auto state_iter = client_state_.find(id);
  DCHECK(client);
  DCHECK(state_iter != client_state_.end());

  RequestState* state = state_iter->second.get();
  if (!response) {
    RespondWithError(id, state->loader->NetError());
    return;
  }
  // TODO(behamilton): decrypt payload.
  std::string payload = std::move(*response);

  auto bhttp_response = quiche::BinaryHttpResponse::Create(payload);
  if (!bhttp_response.ok()) {
    RespondWithError(id, net::ERR_FAILED);
    return;
  }

  // Check that the inner request was successful.
  int status_code = bhttp_response->status_code();
  if (status_code / 100 != 2) {
    RespondWithError(id, net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    return;
  }

  client->OnCompleted(std::string(bhttp_response->body()), net::OK);
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
  params->is_corb_enabled = false;
  params->is_trusted = true;
  params->automatically_assign_isolation_info = true;

  url_loader_factory_.reset();
  owner_network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(), std::move(params));

  return url_loader_factory_.get();
}

}  // namespace network