// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/host_starter_base.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/values.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/fqdn.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/host_config.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/host_starter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

HostStarterBase::HostStarterBase(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

HostStarterBase::~HostStarterBase() = default;

void HostStarterBase::StartHost(Params params, CompletionCallback on_done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!on_done_);

  start_host_params_ = std::move(params);
  if (start_host_params_.name.empty()) {
    // Use the FQDN if a name was not provided via the command line.
    start_host_params_.name = GetFqdn();
  }
  // |auth_code| and |redirect_url| must match and either be populated or empty.
  DCHECK(start_host_params_.auth_code.empty() ==
         start_host_params_.redirect_url.empty());

  on_done_ = std::move(on_done);
  key_pair_ = RsaKeyPair::Generate();

  // Check to see if there is an existing host instance on this machine which
  // needs to be cleaned up before we can create and start a new host instance.
  daemon_controller_->GetConfig(
      base::BindOnce(&HostStarterBase::OnExistingConfigLoaded, weak_ptr_));
}

void HostStarterBase::OnExistingConfigLoaded(
    std::optional<base::Value::Dict> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config.has_value()) {
    std::string* host_id = config->FindString(kHostIdConfigPath);
    if (host_id) {
      existing_host_id_.emplace(*host_id);
      // Formatted to make start_host output more readable.
      LOG(INFO) << "\n  Found existing host: `" << *existing_host_id_ << "`.\n"
                << "  This instance will be deleted from the Directory before "
                << "starting the new host instance.";
    }
  }

  if (!start_host_params_.auth_code.empty()) {
    oauth_helper_.emplace(url_loader_factory_)
        .FetchTokens(
            start_host_params_.owner_email, start_host_params_.auth_code,
            {
                .client_id = google_apis::GetOAuth2ClientID(
                    google_apis::CLIENT_REMOTING),
                .client_secret = google_apis::GetOAuth2ClientSecret(
                    google_apis::CLIENT_REMOTING),
                .redirect_uri = start_host_params_.redirect_url,
            },
            base::BindOnce(&HostStarterBase::OnUserTokensRetrieved, weak_ptr_),
            base::BindOnce(&HostStarterBase::HandleError, weak_ptr_));
  } else {
    RegisterNewHost(key_pair_->GetPublicKey(), /*access_token=*/std::nullopt);
  }
}

void HostStarterBase::OnUserTokensRetrieved(const std::string& user_email,
                                            const std::string& access_token,
                                            const std::string& refresh_token,
                                            const std::string& scope_str) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If an owner email was not provided, then use the account which created the
  // authorization code.
  if (start_host_params_.owner_email.empty()) {
    start_host_params_.owner_email = base::ToLowerASCII(user_email);
  }

  // We don't need a `refresh_token` for the user so ignore it even if the
  // authorization_code was created with the offline param.
  RegisterNewHost(key_pair_->GetPublicKey(), access_token);
}

void HostStarterBase::OnNewHostRegistered(
    const std::string& directory_id,
    const std::string& owner_account_email,
    const std::string& service_account_email,
    const std::string& authorization_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (directory_id.empty()) {
    HandleError("No host id returned by the Directory.", REGISTRATION_ERROR);
    return;
  }
  if (!start_host_params_.id.empty() &&
      !base::EqualsCaseInsensitiveASCII(start_host_params_.id, directory_id)) {
    HandleError(base::StringPrintf(
                    "Host id (%s) returned from the service does not match "
                    "the host id provided (%s)",
                    directory_id.c_str(), start_host_params_.id.c_str()),
                REGISTRATION_ERROR);
    return;
  }
  start_host_params_.id = base::ToLowerASCII(directory_id);

  if (!owner_account_email.empty()) {
    start_host_params_.owner_email = base::ToLowerASCII(owner_account_email);
  }

  // For some use cases, the CRD backend will return the service account and for
  // others we will need to use the email associated with the authz code. The
  // first approach is preferred as then we can provide the email address to
  // the OAuth helper to make sure they match so we set |service_account_email_|
  // here if a value was provided. Otherwise we will set this member after
  // retrieving the service account refresh token from the authorization code.
  if (!service_account_email.empty()) {
    service_account_email_ = base::ToLowerASCII(service_account_email);
  }

  if (authorization_code.empty()) {
    HandleError("No authorization code returned by the Directory.",
                REGISTRATION_ERROR);
    return;
  }

  oauth_helper_.emplace(url_loader_factory_)
      .FetchTokens(
          service_account_email, authorization_code,
          {
              .client_id = google_apis::GetOAuth2ClientID(
                  google_apis::CLIENT_REMOTING_HOST),
              .client_secret = google_apis::GetOAuth2ClientSecret(
                  google_apis::CLIENT_REMOTING_HOST),
              // Service account requests do not set |redirect_uri|.
          },
          base::BindOnce(&HostStarterBase::OnServiceAccountTokensRetrieved,
                         weak_ptr_),
          base::BindOnce(&HostStarterBase::HandleError, weak_ptr_));
}

void HostStarterBase::OnServiceAccountTokensRetrieved(
    const std::string& service_account_email,
    const std::string& access_token,
    const std::string& refresh_token,
    const std::string& scopes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (service_account_email_.empty()) {
    service_account_email_ = base::ToLowerASCII(service_account_email);
  }
  service_account_refresh_token_ = refresh_token;

  RemoveOldHostFromDirectory(
      base::BindOnce(&HostStarterBase::StopOldHost, weak_ptr_));
}

void HostStarterBase::StopOldHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (existing_host_id_.has_value()) {
    daemon_controller_->Stop(
        base::BindOnce(&HostStarterBase::OnOldHostStopped, weak_ptr_));
  } else {
    GenerateConfigFile();
  }
}

void HostStarterBase::OnOldHostStopped(DaemonController::AsyncResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool stopped = false;
  for (auto i = 0; !stopped && i < 10; i++) {
    stopped =
        (daemon_controller_->GetState() == DaemonController::STATE_STOPPED);
    if (!stopped) {
      base::PlatformThread::Sleep(base::Seconds(1));
    }
  }
  if (!stopped) {
    LOG(WARNING) << "Unable to stop existing host process. Setup will "
                 << "continue, but you may need to reboot to complete it.";
  }

  GenerateConfigFile();
}

void HostStarterBase::GenerateConfigFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value::Dict config;
  // These 5 values are required for the host to start up properly. Other values
  // are optional depending on the use case.
  config.Set(kHostOwnerConfigPath, start_host_params_.owner_email);
  config.Set(kServiceAccountConfigPath, service_account_email_);
  config.Set(kOAuthRefreshTokenConfigPath, service_account_refresh_token_);
  config.Set(kHostIdConfigPath, start_host_params_.id);
  config.Set(kPrivateKeyConfigPath, key_pair_->ToString());

  if (!start_host_params_.name.empty()) {
    config.Set(kHostNameConfigPath, start_host_params_.name);
  }

  ApplyConfigValues(config);

  config.Set(kUsageStatsConsentConfigPath,
             start_host_params_.enable_crash_reporting);

  daemon_controller_->SetConfigAndStart(
      std::move(config), start_host_params_.enable_crash_reporting,
      base::BindOnce(&HostStarterBase::OnNewHostStarted,
                     base::Unretained(this)));
}

void HostStarterBase::OnNewHostStarted(DaemonController::AsyncResult result) {
  if (result != DaemonController::RESULT_OK) {
    HandleError(base::StringPrintf("Failed to start host: %d",
                                   static_cast<int>(result)),
                START_ERROR);
  } else {
    std::move(on_done_).Run(START_COMPLETE);
  }
}

void HostStarterBase::HandleError(const std::string& error_message,
                                  Result error_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReportError(error_message, base::BindOnce(std::move(on_done_), error_result));
}

void HostStarterBase::HandleHttpStatusError(const ProtobufHttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ProtobufHttpStatus::Code error_code = status.error_code();
  std::string error_message = status.error_message();
  LOG(ERROR) << "\n  Received error code: " << static_cast<int>(error_code)
             << ", message: " << error_message;

  if (!status.response_body().empty()) {
    std::string extended_error_info;
    size_t pos = status.response_body().rfind("Caused by: ");
    if (pos != std::string::npos) {
      extended_error_info = status.response_body().substr(pos);
    } else {
      extended_error_info = status.response_body();
    }
    VLOG(1) << "Full error information: \n" << status.response_body();
    // Convert the string contents if it is not valid UTF-8. Otherwise it can
    // cause additional errors when reporting the original error to our backend.
    if (!base::IsStringUTF8(extended_error_info)) {
      std::string converted_string;
      if (base::ConvertToUtf8AndNormalize(
              extended_error_info, base::kCodepageLatin1, &converted_string)) {
        extended_error_info = std::move(converted_string);
      }
    }
    error_message =
        base::StringPrintf("%s\nExtended error information: %s\n",
                           error_message.c_str(), extended_error_info.c_str());
  }

  auto result = NETWORK_ERROR;
  if (error_code == ProtobufHttpStatus::Code::PERMISSION_DENIED) {
    result = PERMISSION_DENIED;
  } else if (error_code == ProtobufHttpStatus::Code::UNAUTHENTICATED) {
    result = OAUTH_ERROR;
  }

  HandleError(error_message, result);
}

void HostStarterBase::ReportError(const std::string& message,
                                  base::OnceClosure on_error_reported) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(ERROR) << message;
  std::move(on_error_reported).Run();
}

void HostStarterBase::SetDaemonControllerForTest(
    scoped_refptr<remoting::DaemonController> daemon_controller) {
  daemon_controller_ = daemon_controller;
}

}  // namespace remoting
