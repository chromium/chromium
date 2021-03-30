// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}

namespace content {

class RenderFrameHost;

// Manages network requests and maintains relevant state for interaction with
// the Identity Provider across a WebID transaction. Owned by
// FederatedAuthRequestImpl and has a lifetime limited to a single identity
// transaction between an RP and an IDP.
//
// Diagram of the permission-based data flows between the browser and the IDP:
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//      |                                 |
//      |     GET /.well-known/webid      |
//      |-------------------------------->|
//      |                                 |
//      |        JSON{idp_url}            |
//      |<--------------------------------|
//      |                                 |
//      | POST /idp_url with OIDC request |
//      |-------------------------------->|
//      |                                 |
//      |      id_token or signin_url     |
//      |<--------------------------------|
//  .-------.                           .---.
//  |Browser|                           |IDP|
//  '-------'                           '---'
//
// If the IDP returns an id_token, the sequence finishes. If it returns a
// signin_url, that URL is loaded as a rendered Document into a new window
// for the user to interact with the IDP.
class CONTENT_EXPORT IdpNetworkRequestManager {
 public:
  enum class FetchStatus {
    kSuccess,
    kWebIdNotSupported,
    kFetchError,
    kInvalidResponseError,
  };

  enum class SigninResponse {
    kLoadIdp,
    kTokenGranted,
    kSigninError,
    kInvalidResponseError,
  };

  static constexpr char kWellKnownFilePath[] = ".well-known/webid";

  using FetchWellKnownCallback =
      base::OnceCallback<void(FetchStatus, const std::string&)>;
  using SigninRequestCallback =
      base::OnceCallback<void(SigninResponse, const std::string&)>;

  static std::unique_ptr<IdpNetworkRequestManager> Create(
      const GURL& provider,
      RenderFrameHost* host);

  IdpNetworkRequestManager(const GURL& provider, RenderFrameHost* host);

  virtual ~IdpNetworkRequestManager();

  IdpNetworkRequestManager(const IdpNetworkRequestManager&) = delete;
  IdpNetworkRequestManager& operator=(const IdpNetworkRequestManager&) = delete;

  // Attempt to fetch the IDP's WebID parameters from the its .well-known file.
  virtual void FetchIdpWellKnown(FetchWellKnownCallback);

  // Transmit the OAuth request to the IDP.
  virtual void SendSigninRequest(const GURL& signin_url,
                                 const std::string& request,
                                 SigninRequestCallback);

 private:
  void OnWellKnownLoaded(std::unique_ptr<std::string> response_body);
  void OnWellKnownParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnSigninRequestResponse(std::unique_ptr<std::string> response_body);
  void OnSigninRequestParsed(data_decoder::DataDecoder::ValueOrError result);

  // URL of the Identity Provider.
  GURL provider_;

  RenderFrameHost* render_frame_host_;

  FetchWellKnownCallback idp_well_known_callback_;
  SigninRequestCallback signin_request_callback_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<IdpNetworkRequestManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_IDP_NETWORK_REQUEST_MANAGER_H_
