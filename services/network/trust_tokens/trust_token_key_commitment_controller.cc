// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace internal {

std::unique_ptr<ResourceRequest> CreateTrustTokenKeyCommitmentRequest(
    const net::URLRequest& request,
    const url::Origin& top_level_origin) {
  auto key_commitment_request = std::make_unique<ResourceRequest>();

  key_commitment_request->url =
      request.url().Resolve(kTrustTokenKeyCommitmentWellKnownPath);

  key_commitment_request->method = net::HttpRequestHeaders::kGetMethod;
  key_commitment_request->priority = request.priority();
  key_commitment_request->credentials_mode = mojom::CredentialsMode::kOmit;
  key_commitment_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  key_commitment_request->request_initiator = request.initiator();

  key_commitment_request->headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                            top_level_origin.Serialize());

  return key_commitment_request;
}

}  // namespace internal

TrustTokenKeyCommitmentController::TrustTokenKeyCommitmentController(
    base::OnceCallback<void(Status status,
                            mojom::TrustTokenKeyCommitmentResultPtr result)>
        completion_callback,
    const net::URLRequest& request,
    const url::Origin& top_level_origin,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* loader_factory,
    std::unique_ptr<Parser> parser)
    : parser_(std::move(parser)),
      completion_callback_(std::move(completion_callback)) {
  url_loader_ = SimpleURLLoader::Create(
      internal::CreateTrustTokenKeyCommitmentRequest(request, top_level_origin),
      traffic_annotation);

  StartRequest(loader_factory);
}

TrustTokenKeyCommitmentController::~TrustTokenKeyCommitmentController() =
    default;

void TrustTokenKeyCommitmentController::StartRequest(
    mojom::URLLoaderFactory* loader_factory) {
  DCHECK(url_loader_);

  // It's safe to use base::Unretained here because this class
  // owns the URLLoader: when |this| is destroyed, |url_loader_| will be, too,
  // so the callbacks won't be called.

  url_loader_->SetOnRedirectCallback(
      base::BindRepeating(&TrustTokenKeyCommitmentController::HandleRedirect,
                          base::Unretained(this)));

  static_assert(
      kTrustTokenKeyCommitmentRegistryMaxSizeBytes <
          SimpleURLLoader::kMaxBoundedStringDownloadSize,
      "If the key commitment record maxium size exceeds ~5MB, the key "
      "commitment fetching implementation should be changed to no longer use "
      "SimpleURLLoader::DownloadToString (see the method's function comment).");
  url_loader_->DownloadToString(
      loader_factory,
      base::BindOnce(&TrustTokenKeyCommitmentController::HandleResponseBody,
                     base::Unretained(this)),
      /*max_body_size=*/kTrustTokenKeyCommitmentRegistryMaxSizeBytes);
}

void TrustTokenKeyCommitmentController::HandleRedirect(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  // At least preliminarily, we don't allow redirects in key commitment fetches
  // because of a lack of compelling reason to do so (and the attendant
  // performance downsides of being redirected en route to getting the
  // commitments).

  // Free |url_loader_| now to abort handling the request, in case there's a
  // delay before the client deletes this object.
  url_loader_.reset();
  std::move(completion_callback_)
      .Run({.value = Status::Value::kGotRedirected}, /*result=*/nullptr);

  // |this| may be deleted here.
}

void TrustTokenKeyCommitmentController::HandleResponseBody(
    std::unique_ptr<std::string> response_body) {
  DCHECK(parser_);

  int error = url_loader_->NetError();
  if (!response_body) {
    DCHECK_NE(error, net::OK);
    std::move(completion_callback_)
        .Run({Status::Value::kNetworkError, error}, /*result=*/nullptr);
    return;
  }

  mojom::TrustTokenKeyCommitmentResultPtr result =
      parser_->Parse(*response_body);

  if (!result) {
    std::move(completion_callback_)
        .Run({.value = Status::Value::kCouldntParse}, /*result=*/nullptr);
    return;
  }

  std::move(completion_callback_)
      .Run({.value = Status::Value::kOk}, std::move(result));

  // |this| may be deleted here.
}

}  // namespace network
