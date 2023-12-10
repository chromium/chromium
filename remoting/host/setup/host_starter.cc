// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_starter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/host_config.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/host_stopper.h"
#include "remoting/host/setup/service_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

HostStarter::~HostStarter() = default;

HostStarter::Params::Params() = default;
HostStarter::Params::Params(HostStarter::Params&&) = default;
HostStarter::Params& HostStarter::Params::operator=(HostStarter::Params&&) =
    default;
HostStarter::Params::~Params() = default;

namespace {

constexpr int kMaxGetTokensRetries = 3;

// A helper class that registers and starts a host.
class HostStarterImpl : public HostStarter,
                        public gaia::GaiaOAuthClient::Delegate,
                        public remoting::ServiceClient::Delegate {
 public:
  HostStarterImpl(std::unique_ptr<gaia::GaiaOAuthClient> oauth_client,
                  std::unique_ptr<remoting::ServiceClient> service_client,
                  scoped_refptr<remoting::DaemonController> daemon_controller,
                  std::unique_ptr<remoting::HostStopper> host_stopper);

  HostStarterImpl(const HostStarterImpl&) = delete;
  HostStarterImpl& operator=(const HostStarterImpl&) = delete;

  ~HostStarterImpl() override;

  // HostStarterImpl implementation.
  void StartHost(Params params, CompletionCallback on_done) override;

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

  void StartHostProcess();

  void OnLocalHostStopped();
  void OnHostStarted(DaemonController::AsyncResult result);

  Params start_host_params_;
  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<remoting::ServiceClient> service_client_;
  scoped_refptr<remoting::DaemonController> daemon_controller_;
  std::unique_ptr<remoting::HostStopper> host_stopper_;
  gaia::OAuthClientInfo oauth_client_info_;
  CompletionCallback on_done_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  std::string host_refresh_token_;
  std::string host_access_token_;
  std::string directory_access_token_;
  std::string service_account_email_;
  scoped_refptr<remoting::RsaKeyPair> key_pair_;
  bool auth_code_exchanged_ = false;

  // True if the host was not started and unregistration was requested. If this
  // is set and a network/OAuth error occurs during unregistration, this will
  // be logged, but the error will still be reported as START_ERROR.
  bool unregistering_host_ = false;

  PendingGetTokensRequest pending_get_tokens_ = GET_TOKENS_NONE;

  base::WeakPtr<HostStarterImpl> weak_ptr_;
  base::WeakPtrFactory<HostStarterImpl> weak_ptr_factory_{this};
};

HostStarterImpl::HostStarterImpl(
    std::unique_ptr<gaia::GaiaOAuthClient> oauth_client,
    std::unique_ptr<remoting::ServiceClient> service_client,
    scoped_refptr<remoting::DaemonController> daemon_controller,
    std::unique_ptr<remoting::HostStopper> host_stopper)
    : oauth_client_(std::move(oauth_client)),
      service_client_(std::move(service_client)),
      daemon_controller_(daemon_controller),
      host_stopper_(std::move(host_stopper)) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

HostStarterImpl::~HostStarterImpl() = default;

void HostStarterImpl::StartHost(Params params, CompletionCallback on_done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!on_done_);

  start_host_params_ = std::move(params);
  on_done_ = std::move(on_done);
  oauth_client_info_.client_id =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING);
  oauth_client_info_.client_secret =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING);
  oauth_client_info_.redirect_uri = start_host_params_.redirect_url;
  // Map the authorization code to refresh and access tokens.
  DCHECK_EQ(pending_get_tokens_, GET_TOKENS_NONE);
  pending_get_tokens_ = GET_TOKENS_DIRECTORY;
  oauth_client_->GetTokensFromAuthCode(oauth_client_info_,
                                       start_host_params_.auth_code,
                                       kMaxGetTokensRetries, this);
}

void HostStarterImpl::OnGetTokensResponse(const std::string& refresh_token,
                                          const std::string& access_token,
                                          int expires_in_seconds) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HostStarterImpl::OnGetTokensResponse, weak_ptr_,
                       refresh_token, access_token, expires_in_seconds));
    return;
  }

  if (pending_get_tokens_ == GET_TOKENS_DIRECTORY) {
    directory_access_token_ = access_token;
  } else if (pending_get_tokens_ == GET_TOKENS_HOST) {
    host_refresh_token_ = refresh_token;
    host_access_token_ = access_token;
  } else {
    NOTREACHED();
  }

  pending_get_tokens_ = GET_TOKENS_NONE;

  // Get the email corresponding to the access token.
  oauth_client_->GetUserEmail(access_token, 1, this);
}

void HostStarterImpl::OnRefreshTokenResponse(const std::string& access_token,
                                             int expires_in_seconds) {
  // We never request a refresh token, so this call is not expected.
  NOTREACHED();
}

// This function is called twice: once with the host owner credentials, and once
// with the service account credentials.
void HostStarterImpl::OnGetUserEmailResponse(const std::string& user_email) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HostStarterImpl::OnGetUserEmailResponse,
                                  weak_ptr_, user_email));
    return;
  }

  if (!auth_code_exchanged_) {
    // This is the first callback, with the host owner credentials.
    auth_code_exchanged_ = true;

    // If a host owner was not provided via the command line, then we just use
    // the user_email for the account which generated the auth_code.
    // Otherwise we want to verify user_email matches the host_owner provided.
    // Note that the auth_code has been exchanged at this point so the user
    // can't just re-run the command with the same nonce and a different
    // host_owner to get the command to succeed.
    if (start_host_params_.owner_email.empty()) {
      start_host_params_.owner_email = user_email;
    } else if (!base::EqualsCaseInsensitiveASCII(start_host_params_.owner_email,
                                                 user_email)) {
      LOG(ERROR) << "User email from auth_code (" << user_email << ") does not "
                 << "match the host owner provided ("
                 << start_host_params_.owner_email << ")";
      std::move(on_done_).Run(OAUTH_ERROR);
      return;
    }
    // If the host is already running, stop it; then register a new host with
    // with the directory.
    host_stopper_->StopLocalHost(
        directory_access_token_,
        base::BindOnce(&HostStarterImpl::OnLocalHostStopped,
                       base::Unretained(this)));
  } else {
    // This is the second callback, with the service account credentials.
    // This email is the service account's email.
    service_account_email_ = user_email;
    StartHostProcess();
  }
}

void HostStarterImpl::OnHostRegistered(const std::string& authorization_code) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HostStarterImpl::OnHostRegistered, weak_ptr_,
                                  authorization_code));
    return;
  }

  if (authorization_code.empty()) {
    NOTREACHED() << "No authorization code returned by the Directory.";
    std::move(on_done_).Run(START_ERROR);
    return;
  }

  // Received a service account authorization code, update oauth_client_info_
  // to use the service account client keys, and get service account tokens.
  DCHECK_EQ(pending_get_tokens_, GET_TOKENS_NONE);
  pending_get_tokens_ = GET_TOKENS_HOST;

  oauth_client_info_.client_id =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  oauth_client_info_.client_secret =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);
  // Clear the redirect_uri field since it's not needed for robot auth codes.
  // See b/231442487 for more details.
  oauth_client_info_.redirect_uri.clear();
  oauth_client_->GetTokensFromAuthCode(oauth_client_info_, authorization_code,
                                       kMaxGetTokensRetries, this);
}

void HostStarterImpl::StartHostProcess() {
  // Start the host.
  std::string host_secret_hash =
      remoting::MakeHostPinHash(start_host_params_.id, start_host_params_.pin);
  base::Value::Dict config;
  config.Set(kHostOwnerConfigPath, start_host_params_.owner_email);
  // Write `service_account_email_` twice for backwards compatibility reasons.
  // If the host config only contains `service_account` and the package is down-
  // graded, the host will go offline because `xmpp_login` will not be present.
  // TODO(joedow): Remove the dual-write logic once M120 is rollback-safe.
  config.Set(kServiceAccountConfigPath, service_account_email_);
  config.Set(kDeprecatedXmppLoginConfigPath, service_account_email_);
  config.Set(kOAuthRefreshTokenConfigPath, host_refresh_token_);
  config.Set(kHostIdConfigPath, start_host_params_.id);
  config.Set(kHostNameConfigPath, start_host_params_.name);
  config.Set(kPrivateKeyConfigPath, key_pair_->ToString());
  config.Set(kHostSecretHashConfigPath, host_secret_hash);

  daemon_controller_->SetConfigAndStart(
      std::move(config), start_host_params_.enable_crash_reporting,
      base::BindOnce(&HostStarterImpl::OnHostStarted, base::Unretained(this)));
}

void HostStarterImpl::OnLocalHostStopped() {
  if (start_host_params_.id.empty()) {
    start_host_params_.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  }
  key_pair_ = RsaKeyPair::Generate();

  std::string host_client_id =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);

  service_client_->RegisterHost(start_host_params_.id, start_host_params_.name,
                                key_pair_->GetPublicKey(), host_client_id,
                                directory_access_token_, this);
}

void HostStarterImpl::OnHostStarted(DaemonController::AsyncResult result) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HostStarterImpl::OnHostStarted, weak_ptr_, result));
    return;
  }
  if (result != DaemonController::RESULT_OK) {
    unregistering_host_ = true;
    service_client_->UnregisterHost(start_host_params_.id,
                                    directory_access_token_, this);
    // `on_done_` will be run after the UnregisterHost() call returns.
    return;
  }
  std::move(on_done_).Run(START_COMPLETE);
}

void HostStarterImpl::OnOAuthError() {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HostStarterImpl::OnOAuthError, weak_ptr_));
    return;
  }

  pending_get_tokens_ = GET_TOKENS_NONE;
  if (unregistering_host_) {
    LOG(ERROR) << "OAuth error occurred when unregistering host.";
  }

  std::move(on_done_).Run(unregistering_host_ ? START_ERROR : OAUTH_ERROR);
}

void HostStarterImpl::OnNetworkError(int response_code) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&HostStarterImpl::OnNetworkError, weak_ptr_,
                                  response_code));
    return;
  }

  pending_get_tokens_ = GET_TOKENS_NONE;
  if (unregistering_host_) {
    LOG(ERROR) << "Network error occurred when unregistering host.";
  }

  std::move(on_done_).Run(unregistering_host_ ? START_ERROR : NETWORK_ERROR);
}

void HostStarterImpl::OnHostUnregistered() {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&HostStarterImpl::OnHostUnregistered, weak_ptr_));
    return;
  }
  std::move(on_done_).Run(START_ERROR);
}

}  // namespace

std::unique_ptr<HostStarter> HostStarter::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  auto controller = remoting::DaemonController::Create();
  return std::make_unique<HostStarterImpl>(
      std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory),
      std::make_unique<remoting::ServiceClient>(url_loader_factory), controller,
      std::make_unique<remoting::HostStopper>(
          std::make_unique<remoting::ServiceClient>(url_loader_factory),
          controller));
}

}  // namespace remoting
