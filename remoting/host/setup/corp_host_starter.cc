// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/corp_host_starter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/uuid.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "net/base/network_interfaces.h"
#include "remoting/base/corp_service_client.h"
#include "remoting/base/hostname.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/host_config.h"
#include "remoting/host/pin_hash.h"
#include "remoting/host/setup/buildflags.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/host_starter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(REMOTING_INTERNAL)
#include "remoting/internal/proto/helpers.h"
#else
#include "remoting/proto/internal_stubs.h"  // nogncheck
#endif

namespace remoting {

namespace {

constexpr int kMaxGetTokensRetries = 3;

// A helper class which provisions a corp machine for Chrome Remote Desktop.
class CorpHostStarter : public HostStarter,
                        public gaia::GaiaOAuthClient::Delegate {
 public:
  CorpHostStarter(std::unique_ptr<gaia::GaiaOAuthClient> oauth_client,
                  std::unique_ptr<CorpServiceClient> corp_service_client);

  CorpHostStarter(const CorpHostStarter&) = delete;
  CorpHostStarter& operator=(const CorpHostStarter&) = delete;

  ~CorpHostStarter() override;

  // HostStarter implementation.
  void StartHost(Params params, CompletionCallback on_done) override;

  // gaia::GaiaOAuthClient::Delegate implementation.
  void OnGetTokensResponse(const std::string& refresh_token,
                           const std::string& access_token,
                           int expires_in_seconds) override;
  void OnRefreshTokenResponse(const std::string& access_token,
                              int expires_in_seconds) override;
  void OnGetUserEmailResponse(const std::string& user_email) override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  void StartHostProcess();

  void OnExistingConfigLoaded(std::optional<base::Value::Dict> config);

  void OnProvisionCorpMachineResponse(
      const ProtobufHttpStatus& status,
      std::unique_ptr<internal::ProvisionCorpMachineResponse> response);

  void OnHostStarted(DaemonController::AsyncResult result);
  void OnHostStopped(DaemonController::AsyncResult result);

  void GetOAuthTokens();

  void HandleHttpStatusError(const ProtobufHttpStatus& status);

  void ReportProvisioningError(const std::string& message, Result result);

  Params start_host_params_;
  std::string host_refresh_token_;
  std::string service_account_email_;
  scoped_refptr<remoting::RsaKeyPair> key_pair_;
  bool has_existing_host_instance_ = false;

  std::unique_ptr<gaia::GaiaOAuthClient> oauth_client_;
  std::unique_ptr<CorpServiceClient> corp_service_client_;
  scoped_refptr<remoting::DaemonController> daemon_controller_ =
      remoting::DaemonController::Create();

  gaia::OAuthClientInfo oauth_client_info_;
  std::string authorization_code_;
  CompletionCallback on_done_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  base::WeakPtr<CorpHostStarter> weak_ptr_;
  base::WeakPtrFactory<CorpHostStarter> weak_ptr_factory_{this};
};

CorpHostStarter::CorpHostStarter(
    std::unique_ptr<gaia::GaiaOAuthClient> oauth_client,
    std::unique_ptr<CorpServiceClient> corp_service_client)
    : oauth_client_(std::move(oauth_client)),
      corp_service_client_(std::move(corp_service_client)) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

CorpHostStarter::~CorpHostStarter() = default;

void CorpHostStarter::StartHost(Params params, CompletionCallback on_done) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(!on_done_);

  start_host_params_ = std::move(params);
  if (start_host_params_.name.empty()) {
    // Use the host FQDN if a name was not provided via the command line.
    start_host_params_.name = GetHostname();
  }
  on_done_ = std::move(on_done);
  key_pair_ = RsaKeyPair::Generate();

  // Check to see if there is an existing host instance on this machine which
  // needs to be cleaned up before we can create and start a new host instance.
  daemon_controller_->GetConfig(
      base::BindOnce(&CorpHostStarter::OnExistingConfigLoaded, weak_ptr_));
}

void CorpHostStarter::OnExistingConfigLoaded(
    std::optional<base::Value::Dict> config) {
  std::optional<std::string> existing_host_id;
  if (config.has_value()) {
    std::string* host_id = config->FindString("host_id");
    if (host_id) {
      has_existing_host_instance_ = true;
      existing_host_id.emplace(*host_id);
      // Formatted to make start_host output more readable.
      LOG(INFO) << "\n  Found existing host: `" << *existing_host_id << "`.\n"
                << "  This instance will be deleted from the Directory before "
                << "creating the new host instance.";
    }
  }

  corp_service_client_->ProvisionCorpMachine(
      start_host_params_.owner_email, start_host_params_.name,
      key_pair_->GetPublicKey(), std::move(existing_host_id),
      base::BindOnce(&CorpHostStarter::OnProvisionCorpMachineResponse,
                     weak_ptr_));
}

void CorpHostStarter::OnGetTokensResponse(const std::string& refresh_token,
                                          const std::string& access_token,
                                          int expires_in_seconds) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CorpHostStarter::OnGetTokensResponse, weak_ptr_,
                       refresh_token, access_token, expires_in_seconds));
    return;
  }

  // Store the refresh token since we will eventually write it into the config.
  host_refresh_token_ = refresh_token;

  // Get the email corresponding to the access token, since we don't need the
  // access token for anything else, we don't store it.
  oauth_client_->GetUserEmail(access_token, 1, this);
}

void CorpHostStarter::OnRefreshTokenResponse(const std::string& access_token,
                                             int expires_in_seconds) {
  // We never request a new access token, so this call is not expected.
  NOTREACHED();
}

void CorpHostStarter::OnGetUserEmailResponse(const std::string& user_email) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CorpHostStarter::OnGetUserEmailResponse,
                                  weak_ptr_, user_email));
    return;
  }

  if (service_account_email_.compare(base::ToLowerASCII(user_email)) != 0) {
    ReportProvisioningError(
        base::StringPrintf(
            "authorization_code was created for `%s` which does not "
            "match the service account created for the host: `%s`",
            user_email.c_str(), service_account_email_.c_str()),
        OAUTH_ERROR);
    return;
  }

  StartHostProcess();
}

void CorpHostStarter::OnProvisionCorpMachineResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<internal::ProvisionCorpMachineResponse> response) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CorpHostStarter::OnProvisionCorpMachineResponse,
                       weak_ptr_, status, std::move(response)));
    return;
  }

  if (!status.ok()) {
    HandleHttpStatusError(status);
    return;
  }

  service_account_email_ =
      base::ToLowerASCII(internal::GetServiceAccount(*response));
  start_host_params_.id = internal::GetHostId(*response);

  // Update the owner_email to reflect the account returned by the Directory.
  // The corp-user arg (copied to the owner_email start host param struct) can
  // contain two types of values:
  //   1. The email address of the user to provision the machine for
  //   2. A user permission, defined by the service, which is used to select the
  //      account (e.g. the account which the machine is associated with)
  //
  // The value returned by the Directory should match for scenario #1 and needs
  // to be stored for scenario #2. We don't need to compare since the server
  // will return an error for scenario #1 if the user doesn't have permission.
  start_host_params_.owner_email =
      base::ToLowerASCII(internal::GetOwnerEmail(*response));

  authorization_code_ = internal::GetAuthorizationCode(*response);
  if (authorization_code_.empty()) {
    ReportProvisioningError("No authorization code returned by the Directory.",
                            REGISTRATION_ERROR);
    return;
  }

  if (has_existing_host_instance_) {
    daemon_controller_->Stop(
        base::BindOnce(&CorpHostStarter::OnHostStopped, weak_ptr_));
  } else {
    GetOAuthTokens();
  }
}

void CorpHostStarter::OnHostStopped(DaemonController::AsyncResult result) {
  bool stopped = false;
  for (auto i = 0; !stopped && i < 10; i++) {
    LOG(INFO) << "Attempting to stop the existing host instance...";
    stopped =
        (daemon_controller_->GetState() == DaemonController::STATE_STOPPED);
    if (!stopped) {
      base::PlatformThread::Sleep(base::Seconds(1));
    }
  }
  if (!stopped) {
    LOG(WARNING) << "Unable to stop existing host process. Setup will "
                 << "continue, but you may need to restart the host to "
                 << "complete it.";
  } else {
    LOG(INFO) << "Existing host instance stopped.";
  }

  GetOAuthTokens();
}

void CorpHostStarter::GetOAuthTokens() {
  LOG(INFO) << "Requesting OAuth tokens for the robot account.";
  // Now retrieve the access and refresh tokens for the service account.
  oauth_client_info_.client_id =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST);
  oauth_client_info_.client_secret =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING_HOST);
  oauth_client_->GetTokensFromAuthCode(oauth_client_info_, authorization_code_,
                                       kMaxGetTokensRetries, this);
}

void CorpHostStarter::StartHostProcess() {
  LOG(INFO) << "Starting new host instance.";
  // Start the host.
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

  // Note: Hosts configured using this class do not have a PIN and therefore we
  // do not need to store a hash value in the config for it.

  daemon_controller_->SetConfigAndStart(
      std::move(config), start_host_params_.enable_crash_reporting,
      base::BindOnce(&CorpHostStarter::OnHostStarted, base::Unretained(this)));
}

void CorpHostStarter::OnHostStarted(DaemonController::AsyncResult result) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CorpHostStarter::OnHostStarted, weak_ptr_, result));
    return;
  }
  if (result != DaemonController::RESULT_OK) {
    ReportProvisioningError(base::StringPrintf("Failed to start host: %d",
                                               static_cast<int>(result)),
                            START_ERROR);
    return;
  }
  std::move(on_done_).Run(START_COMPLETE);
}

void CorpHostStarter::OnOAuthError() {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CorpHostStarter::OnOAuthError, weak_ptr_));
    return;
  }

  std::move(on_done_).Run(OAUTH_ERROR);
}

void CorpHostStarter::OnNetworkError(int response_code) {
  if (!main_task_runner_->BelongsToCurrentThread()) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CorpHostStarter::OnNetworkError, weak_ptr_,
                                  response_code));
    return;
  }

  std::move(on_done_).Run(NETWORK_ERROR);
}

void CorpHostStarter::HandleHttpStatusError(const ProtobufHttpStatus& status) {
  ProtobufHttpStatus::Code error_code = status.error_code();
  std::string error_message = status.error_message();
  LOG(ERROR) << "\n  Received error code: " << static_cast<int>(error_code)
             << ", message: " << error_message;

  if (!status.response_body().empty()) {
    size_t pos = status.response_body().rfind("Caused by: ");
    if (pos != std::string::npos) {
      error_message = status.response_body().substr(pos);
      LOG(ERROR) << "\n  Extended error information: \n" << error_message;
      VLOG(1) << "\n  Full error information: \n" << status.response_body();
    } else {
      error_message = status.response_body();
      LOG(ERROR) << "\n  Failed to find extended error information, showing "
                 << "full output:\n"
                 << error_message;
    }
  }

  auto result = NETWORK_ERROR;
  if (error_code == ProtobufHttpStatus::Code::PERMISSION_DENIED) {
    result = PERMISSION_DENIED;
  } else if (error_code == ProtobufHttpStatus::Code::UNAUTHENTICATED) {
    result = OAUTH_ERROR;
  }

  ReportProvisioningError(error_message, result);
}

void CorpHostStarter::ReportProvisioningError(const std::string& message,
                                              Result result) {
  const std::string& host_id = start_host_params_.id;
  LOG(ERROR) << "Reporting provisioning error for host id `" << host_id
             << "`: " << message;
  corp_service_client_->ReportProvisioningError(
      host_id, message,
      base::BindOnce(
          [](CompletionCallback on_done, Result result,
             const ProtobufHttpStatus& status, std::unique_ptr<Empty>) {
            if (!status.ok()) {
              LOG(ERROR) << "Failed to report provisioning error: "
                         << static_cast<int>(status.error_code());
            }
            std::move(on_done).Run(result);
          },
          std::move(on_done_), result));
}

}  // namespace

std::unique_ptr<HostStarter> ProvisionCorpMachine(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<CorpHostStarter>(
      std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory),
      std::make_unique<CorpServiceClient>(url_loader_factory));
}

}  // namespace remoting
