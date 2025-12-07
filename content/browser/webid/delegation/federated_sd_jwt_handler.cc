// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/federated_sd_jwt_handler.h"

#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "content/browser/webid/delegation/jwt_signer.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/mappers.h"
#include "content/browser/webid/request_service.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace content {

using blink::mojom::FederatedAuthRequestResult;

namespace {
std::vector<uint8_t> Sha256(std::string_view data) {
  auto hash = crypto::hash::Sha256(base::as_byte_span(data));
  std::vector<uint8_t> result{hash.begin(), hash.end()};
  return result;
}
}  // namespace

FederatedSdJwtHandler::FederatedSdJwtHandler(
    const blink::mojom::IdentityProviderRequestOptionsPtr& provider,
    RenderFrameHost& render_frame_host,
    webid::RequestService* federated_auth_request_impl)
    : fields_(provider->fields),
      nonce_(provider->nonce),
      config_url_(provider->config->config_url),
      render_frame_host_(&render_frame_host),
      federated_auth_request_impl_(federated_auth_request_impl) {
  // Creates a throw away private key for a one-time use for
  // a single presentation. The public key gets sent to the
  // VC issuance endpoint and gets bound to the issued SD-JWT
  // by the issuer, delegating the presentation to the holder.
  // The browser selectively discloses the fields that were
  // requested and binds the audience and the nonce to the
  // Key Binding JWT before returning to the verifier.
  private_key_ = crypto::keypair::PrivateKey::GenerateEcP256();
}

FederatedSdJwtHandler::~FederatedSdJwtHandler() {}

std::string FederatedSdJwtHandler::ComputeUrlEncodedTokenPostDataForIssuers(
    const std::string& account_id) {
  return base::StrCat(
      {"account_id=", base::EscapeUrlEncodedData(account_id, /*use_plus=*/true),
       "&holder_key=",
       base::EscapeUrlEncodedData(*GetPublicKey().Serialize(),
                                  /*use_plus=*/true),
       "&format=", base::EscapeUrlEncodedData("vc+sd-jwt", /*use_plus=*/true)});
}

void FederatedSdJwtHandler::ProcessSdJwt(const std::string& token) {
  // Checked previously.
  DCHECK(webid::IsDelegationEnabled());

  auto value = sdjwt::SdJwt::Parse(token);
  if (!value) {
    federated_auth_request_impl_->CompleteRequestWithError(
        FederatedAuthRequestResult::kError,
        /*token_status=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  auto sd_jwt = sdjwt::SdJwt::From(*value);
  if (!sd_jwt) {
    federated_auth_request_impl_->CompleteRequestWithError(
        FederatedAuthRequestResult::kError,
        /*token_status=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  // Each of the disclosures is an individual JSON Object.
  // Parse them all and use BarrierCallback to get a callback when all
  // parsing is done.
  auto callback = BarrierClosure(
      sd_jwt->disclosures.size(),
      base::BindOnce(&FederatedSdJwtHandler::OnSdJwtParsed,
                     weak_ptr_factory_.GetWeakPtr(), sd_jwt->jwt));

  for (const auto& json : sd_jwt->disclosures) {
    data_decoder::DataDecoder::ParseJsonIsolated(
        json.value(),
        base::BindOnce(&FederatedSdJwtHandler::OnDisclosureParsed,
                       weak_ptr_factory_.GetWeakPtr(), callback, json.value()));
  }
}

sdjwt::Jwk FederatedSdJwtHandler::GetPublicKey() const {
  return *sdjwt::ExportPublicKey(*private_key_);
}

void FederatedSdJwtHandler::OnDisclosureParsed(
    base::RepeatingClosure cb,
    const std::string& json,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_list()) {
    cb.Run();
    return;
  }

  auto disclosure = sdjwt::Disclosure::From(result->GetList());
  if (!disclosure) {
    // Ignore invalid disclosure structures.
    cb.Run();
    return;
  }

  disclosures_.push_back({disclosure->name, sdjwt::JSONString(json)});
  cb.Run();
}

void FederatedSdJwtHandler::OnSdJwtParsed(const sdjwt::Jwt& jwt) {
  std::vector<std::string> fields = {webid::kDefaultFieldName,
                                     webid::kDefaultFieldEmail,
                                     webid::kDefaultFieldPicture};
  if (fields_.has_value()) {
    fields = fields_.value();
  }

  auto selected = sdjwt::SdJwt::Disclose(disclosures_, fields);

  disclosures_.clear();

  if (!selected) {
    federated_auth_request_impl_->CompleteRequestWithError(
        FederatedAuthRequestResult::kError,
        /*token_status=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  sdjwt::SdJwt result;
  result.jwt = jwt;
  result.disclosures = *selected;

  auto sdjwtkb = sdjwt::SdJwtKb::Create(
      result, render_frame_host_->GetLastCommittedOrigin().Serialize(), nonce_,
      /*iat=*/base::Time::Now(), base::BindRepeating(Sha256),
      sdjwt::CreateJwtSigner(*std::move(private_key_)));

  if (!sdjwtkb) {
    federated_auth_request_impl_->CompleteRequestWithError(
        FederatedAuthRequestResult::kError,
        /*token_status=*/std::nullopt,
        /*should_delay_callback=*/false);
    return;
  }

  auto token = sdjwtkb->Serialize();
  // TODO(crbug.com/380367784): introduce and use a more specific
  // TokenStatus type for SD-JWTs.
  federated_auth_request_impl_->CompleteRequest(
      FederatedAuthRequestResult::kSuccess,
      webid::RequestIdTokenStatus::kSuccessUsingTokenInHttpResponse,
      /*token_error=*/std::nullopt, config_url_, base::Value(token),
      /*should_delay_callback=*/false);
}

}  // namespace content
