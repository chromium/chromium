// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_FEDERATED_SD_JWT_HANDLER_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_FEDERATED_SD_JWT_HANDLER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "crypto/keypair.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHost;

namespace webid {
class RequestService;
}

class FederatedSdJwtHandler {
 public:
  explicit FederatedSdJwtHandler(
      const blink::mojom::IdentityProviderRequestOptionsPtr& provider,
      RenderFrameHost& render_frame_host,
      webid::RequestService* federated_auth_request_impl);
  ~FederatedSdJwtHandler();

  std::string ComputeUrlEncodedTokenPostDataForIssuers(
      const std::string& account_id);

  void ProcessSdJwt(const std::string& token);

 private:
  sdjwt::Jwk GetPublicKey() const;
  void OnDisclosureParsed(base::RepeatingClosure cb,
                          const std::string& json,
                          data_decoder::DataDecoder::ValueOrError result);
  void OnSdJwtParsed(const sdjwt::Jwt& jwt);

  // A list of disclosures that were parsed in the token response, when
  // the token's format is "vc+sd-jwt".
  std::vector<std::pair<std::string, content::sdjwt::JSONString>> disclosures_;
  // A private key that is used to bind the token when the token "format" is
  // "vc+sd-jwt".
  std::optional<crypto::keypair::PrivateKey> private_key_;

  std::optional<std::vector<std::string>> fields_;
  std::string nonce_;
  GURL config_url_;

  raw_ptr<RenderFrameHost> render_frame_host_;
  raw_ptr<webid::RequestService> federated_auth_request_impl_;

  base::WeakPtrFactory<FederatedSdJwtHandler> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_FEDERATED_SD_JWT_HANDLER_H_
