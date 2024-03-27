// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/oauth_host_starter.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "google_apis/google_api_keys.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/host_starter_base.h"
#include "remoting/host/setup/service_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// A helper class that registers and starts a host using OAuth.
class OAuthHostStarterImpl : public HostStarterBase,
                             public ServiceClient::Delegate {
 public:
  explicit OAuthHostStarterImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  OAuthHostStarterImpl(const OAuthHostStarterImpl&) = delete;
  OAuthHostStarterImpl& operator=(const OAuthHostStarterImpl&) = delete;

  ~OAuthHostStarterImpl() override;

  // HostStarterBase implementation.
  void RegisterNewHost(const std::string& access_token,
                       const std::string& public_key) override;
  void RemoveOldHostFromDirectory(base::OnceClosure on_host_removed) override;

  // ServiceClient::Delegate
  void OnHostRegistered(const std::string& host_id,
                        const std::string& authorization_code) override;
  void OnHostUnregistered() override;
  void OnOAuthError() override;
  void OnNetworkError(int response_code) override;

 private:
  std::string access_token_;
  base::OnceClosure on_host_removed_;
  std::unique_ptr<ServiceClient> service_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OAuthHostStarterImpl> weak_ptr_factory_{this};
};

OAuthHostStarterImpl::OAuthHostStarterImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : HostStarterBase(url_loader_factory),
      service_client_(std::make_unique<ServiceClient>(url_loader_factory)) {}

OAuthHostStarterImpl::~OAuthHostStarterImpl() = default;

void OAuthHostStarterImpl::RegisterNewHost(const std::string& access_token,
                                           const std::string& public_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!access_token.empty());
  DCHECK(!public_key.empty());

  access_token_ = access_token;
  service_client_->RegisterHost(
      params().id, params().name, public_key,
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING_HOST),
      access_token_, this);
}

void OAuthHostStarterImpl::OnHostRegistered(
    const std::string& host_id,
    const std::string& authorization_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnNewHostRegistered(base::ToLowerASCII(host_id),
                      /*owner_account_email=*/std::string(),
                      /*service_account_email=*/std::string(),
                      authorization_code);
}

void OAuthHostStarterImpl::RemoveOldHostFromDirectory(
    base::OnceClosure on_host_removed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_host_removed_ = std::move(on_host_removed);
  service_client_->UnregisterHost(*existing_host_id(), access_token_, this);
}

void OAuthHostStarterImpl::OnOAuthError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (on_host_removed_) {
    LOG(ERROR) << "OAuth error occurred when unregistering host.";
    std::move(on_host_removed_).Run();
    return;
  }

  HandleError("OAuth error occurred when registering the host.", OAUTH_ERROR);
}

void OAuthHostStarterImpl::OnNetworkError(int response_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_host_removed_) {
    LOG(ERROR) << "Network error occurred when unregistering host.";
    std::move(on_host_removed_).Run();
    return;
  }

  HandleError("Network error occurred when registering the host.",
              NETWORK_ERROR);
}

void OAuthHostStarterImpl::OnHostUnregistered() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(on_host_removed_).Run();
}

}  // namespace

std::unique_ptr<HostStarter> CreateOAuthHostStarter(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<OAuthHostStarterImpl>(url_loader_factory);
}

}  // namespace remoting
