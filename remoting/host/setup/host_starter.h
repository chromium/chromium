// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_STARTER
#define REMOTING_HOST_HOST_STARTER

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/service_client.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

// A helper class that registers and starts a host.
class HostStarter : public gaia::GaiaOAuthClient::Delegate,
                    public remoting::ServiceClient::Delegate {
 public:
  enum Result {
    START_COMPLETE,
    NETWORK_ERROR,
    OAUTH_ERROR,
    START_ERROR,
  };

  typedef base::Callback<void(Result)> CompletionCallback;

  ~HostStarter() override;

  // Creates a HostStarter.
  static std::unique_ptr<HostStarter> Create(
      const std::string& remoting_server_endpoint,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Registers a new host with the Chromoting service, and starts it.
  // |auth_code| must be a valid OAuth2 authorization code, typically acquired
  // from a browser. This method uses that code to get an OAuth2 refresh token.
  void StartHost(const std::string& host_name,
                 const std::string& host_pin,
                 bool consent_to_data_collection,
                 const std::string& auth_code,
                 const std::string& redirect_url,
                 CompletionCallback on_done);

  // gaia::GaiaOAuthClient::Delegate
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetUserEmailResponse(const std::string& user_email) override;

  // remoting::ServiceClient::Delegate
  void OnHostRegistered(const std::string& authorization_code) override;
  void OnHostUnregistered() override;

  // TODO(sergeyu): Following methods are members of all three delegate
  // interfaces implemented in this class. Fix ServiceClient and
  // GaiaUserEmailFetcher so that Delegate interfaces do not overlap (ideally
  // they should be changed to use Callback<>).
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  // GetTokensFromAuthCode() is used for getting an access token for the
  // Directory API (to register/unregister a new host). It is also used for
  // getting access+refresh tokens for the new host (for getting the robot
  // email and for writing the new config file).
  enum PendingGetTokensRequest {
    GET_TOKENS_NONE,
    GET_TOKENS_DIRECTORY,
    GET_TOKENS_HOST
  };

  HostStarter(std::unique_ptr<gaia::GaiaOAuthClient> oauth_client,
              std::unique_ptr<remoting::ServiceClient> service_client,
              scoped_refptr<remoting::DaemonController> daemon_controller);

  void StartHostProcess();

  void OnHostStarted(DaemonController::AsyncResult result);

  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<remoting::ServiceClient> service_client_;
  scoped_refptr<remoting::DaemonController> daemon_controller_;
  gaia::OAuthClientInfo oauth_client_info_;
  std::string host_name_;
  std::string host_pin_;
  bool consent_to_data_collection_;
  CompletionCallback on_done_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::string host_refresh_token_;
  std::string host_access_token_;
  std::string directory_access_token_;
  std::string host_owner_;
  std::string xmpp_login_;
  scoped_refptr<remoting::RsaKeyPair> key_pair_;
  std::string host_id_;

  // True if the host was not started and unregistration was requested. If this
  // is set and a network/OAuth error occurs during unregistration, this will
  // be logged, but the error will still be reported as START_ERROR.
  bool unregistering_host_;

  PendingGetTokensRequest pending_get_tokens_ = GET_TOKENS_NONE;

  base::WeakPtr<HostStarter> weak_ptr_;
  base::WeakPtrFactory<HostStarter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HostStarter);
};

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_STARTER
