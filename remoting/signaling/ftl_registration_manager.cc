// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/signaling/ftl_registration_manager.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/proto/ftl/v1/ftl_messages.pb.h"
#include "remoting/signaling/ftl_device_id_provider.h"
#include "remoting/signaling/ftl_services_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

constexpr remoting::ftl::ChromotingCapability::Feature
    kChromotingCapabilities[] = {
        remoting::ftl::ChromotingCapability_Feature_SERIALIZED_XMPP_SIGNALING};
constexpr size_t kChromotingCapabilityCount =
    sizeof(kChromotingCapabilities) /
    sizeof(ftl::ChromotingCapability::Feature);

constexpr remoting::ftl::FtlCapability::Feature kFtlCapabilities[] = {
    remoting::ftl::FtlCapability_Feature_RECEIVE_CALLS_FROM_GAIA,
    remoting::ftl::FtlCapability_Feature_GAIA_REACHABLE};
constexpr size_t kFtlCapabilityCount =
    sizeof(kFtlCapabilities) / sizeof(ftl::FtlCapability::Feature);

constexpr base::TimeDelta kRefreshBufferTime = base::Hours(1);

constexpr char kSignInGaiaPath[] = "/v1/registration:signingaia";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ftl_registration_manager",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Sign into Google instant messaging service so that we can "
            "exchange signaling messages with the peer (either the Chrome "
            "Remote Desktop host or client)."
          trigger:
            "Initiating a Chrome Remote Desktop connection."
          data: "User credentials for using the instant messaging service."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

}  // namespace

class FtlRegistrationManager::RegistrationClientImpl final
    : public FtlRegistrationManager::RegistrationClient {
 public:
  RegistrationClientImpl(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  RegistrationClientImpl(const RegistrationClientImpl&) = delete;
  RegistrationClientImpl& operator=(const RegistrationClientImpl&) = delete;

  ~RegistrationClientImpl() override;

  // RegistrationClient implementations.
  void SignInGaia(const ftl::SignInGaiaRequest& request,
                  SignInGaiaResponseCallback on_done) override;
  void CancelPendingRequests() override;

 private:
  ProtobufHttpClient http_client_;
};

FtlRegistrationManager::RegistrationClientImpl::RegistrationClientImpl(
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(FtlServicesContext::GetServerEndpoint(),
                   token_getter,
                   url_loader_factory) {}

FtlRegistrationManager::RegistrationClientImpl::~RegistrationClientImpl() =
    default;

void FtlRegistrationManager::RegistrationClientImpl::SignInGaia(
    const ftl::SignInGaiaRequest& request,
    SignInGaiaResponseCallback on_done) {
  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = kSignInGaiaPath;
  request_config->request_message =
      std::make_unique<ftl::SignInGaiaRequest>(request);
  auto http_request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  http_request->SetResponseCallback(std::move(on_done));
  http_client_.ExecuteRequest(std::move(http_request));
}

void FtlRegistrationManager::RegistrationClientImpl::CancelPendingRequests() {
  http_client_.CancelPendingRequests();
}

// End of RegistrationClientImplImpl

FtlRegistrationManager::FtlRegistrationManager(
    OAuthTokenGetter* token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<FtlDeviceIdProvider> device_id_provider)
    : FtlRegistrationManager(
          std::make_unique<RegistrationClientImpl>(token_getter,
                                                   url_loader_factory),
          std::move(device_id_provider)) {}

FtlRegistrationManager::FtlRegistrationManager(
    std::unique_ptr<RegistrationClient> registration_client,
    std::unique_ptr<FtlDeviceIdProvider> device_id_provider)
    : registration_client_(std::move(registration_client)),
      device_id_provider_(std::move(device_id_provider)),
      sign_in_backoff_(&FtlServicesContext::GetBackoffPolicy()) {
  DCHECK(device_id_provider_);
}

FtlRegistrationManager::~FtlRegistrationManager() = default;

void FtlRegistrationManager::SignInGaia(DoneCallback on_done) {
  VLOG(1) << "SignInGaia will be called with backoff: "
          << sign_in_backoff_.GetTimeUntilRelease();
  sign_in_backoff_timer_.Start(
      FROM_HERE, sign_in_backoff_.GetTimeUntilRelease(),
      base::BindOnce(&FtlRegistrationManager::DoSignInGaia,
                     base::Unretained(this), std::move(on_done)));
}

void FtlRegistrationManager::SignOut() {
  if (!IsSignedIn()) {
    return;
  }
  registration_client_->CancelPendingRequests();
  sign_in_refresh_timer_.Stop();
  registration_id_.clear();
  ftl_auth_token_.clear();
}

bool FtlRegistrationManager::IsSignedIn() const {
  return !ftl_auth_token_.empty();
}

std::string FtlRegistrationManager::GetRegistrationId() const {
  return registration_id_;
}
std::string FtlRegistrationManager::GetFtlAuthToken() const {
  return ftl_auth_token_;
}

void FtlRegistrationManager::DoSignInGaia(DoneCallback on_done) {
  ftl::SignInGaiaRequest request;
  *request.mutable_header() = FtlServicesContext::CreateRequestHeader();
  request.set_app(FtlServicesContext::GetChromotingAppIdentifier());
  request.set_mode(ftl::SignInGaiaMode_Value_DEFAULT_CREATE_ACCOUNT);

  *request.mutable_register_data()->mutable_device_id() =
      device_id_provider_->GetDeviceId();

  for (size_t i = 0; i < kChromotingCapabilityCount; i++) {
    request.mutable_register_data()->add_caps(kChromotingCapabilities[i]);
  }

  for (size_t i = 0; i < kFtlCapabilityCount; i++) {
    request.mutable_register_data()->add_caps(kFtlCapabilities[i]);
  }

  registration_client_->SignInGaia(
      request, base::BindOnce(&FtlRegistrationManager::OnSignInGaiaResponse,
                              base::Unretained(this), std::move(on_done)));
}

void FtlRegistrationManager::OnSignInGaiaResponse(
    DoneCallback on_done,
    const ProtobufHttpStatus& status,
    std::unique_ptr<ftl::SignInGaiaResponse> response) {
  registration_id_.clear();

  if (!status.ok()) {
    LOG(ERROR) << "Failed to sign in."
               << " Error code: " << static_cast<int>(status.error_code())
               << ", message: " << status.error_message();
    sign_in_backoff_.InformOfRequest(false);
    std::move(on_done).Run(status);
    return;
  }

  sign_in_backoff_.Reset();
  registration_id_ = response->registration_id();
  if (registration_id_.empty()) {
    std::move(on_done).Run(ProtobufHttpStatus(ProtobufHttpStatus::Code::UNKNOWN,
                                              "registration_id is empty."));
    return;
  }

  // TODO(yuweih): Consider caching auth token.
  ftl_auth_token_ = response->auth_token().payload();
  VLOG(1) << "Auth token set on FtlClient";
  base::TimeDelta refresh_delay =
      base::Microseconds(response->auth_token().expires_in());
  if (refresh_delay > kRefreshBufferTime) {
    refresh_delay -= kRefreshBufferTime;
  } else {
    LOG(WARNING) << "Refresh time is too short. Buffer time is not applied.";
  }
  sign_in_refresh_timer_.Start(
      FROM_HERE, refresh_delay,
      base::BindOnce(&FtlRegistrationManager::SignInGaia,
                     base::Unretained(this), base::DoNothing()));
  VLOG(1) << "Scheduled auth token refresh in: " << refresh_delay;
  std::move(on_done).Run(status);
}

}  // namespace remoting
