// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_
#define REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "remoting/signaling/registration_manager.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

namespace ftl {

class SignInGaiaRequest;
class SignInGaiaResponse;

}  // namespace ftl

class FtlDeviceIdProvider;
class OAuthTokenGetter;

// Class for registering the user with FTL service.
class FtlRegistrationManager final : public RegistrationManager {
 public:
  // |token_getter| must outlive |this|.
  FtlRegistrationManager(
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<FtlDeviceIdProvider> device_id_provider);

  FtlRegistrationManager(const FtlRegistrationManager&) = delete;
  FtlRegistrationManager& operator=(const FtlRegistrationManager&) = delete;

  ~FtlRegistrationManager() override;

  // RegistrationManager implementations.
  void SignInGaia(DoneCallback on_done) override;
  void SignOut() override;
  bool IsSignedIn() const override;
  std::string GetRegistrationId() const override;
  std::string GetFtlAuthToken() const override;

 private:
  using SignInGaiaResponseCallback =
      base::OnceCallback<void(const ProtobufHttpStatus&,
                              std::unique_ptr<ftl::SignInGaiaResponse>)>;

  friend class FtlRegistrationManagerTest;

  class RegistrationClient {
   public:
    virtual ~RegistrationClient() = default;
    virtual void SignInGaia(const ftl::SignInGaiaRequest& request,
                            SignInGaiaResponseCallback on_done) = 0;
    virtual void CancelPendingRequests() = 0;
  };
  class RegistrationClientImpl;

  FtlRegistrationManager(
      std::unique_ptr<RegistrationClient> registration_client,
      std::unique_ptr<FtlDeviceIdProvider> device_id_provider);

  void DoSignInGaia(DoneCallback on_done);
  void OnSignInGaiaResponse(DoneCallback on_done,
                            const ProtobufHttpStatus& status,
                            std::unique_ptr<ftl::SignInGaiaResponse> response);

  std::unique_ptr<RegistrationClient> registration_client_;
  std::unique_ptr<FtlDeviceIdProvider> device_id_provider_;
  base::OneShotTimer sign_in_backoff_timer_;
  base::OneShotTimer sign_in_refresh_timer_;
  std::string registration_id_;
  std::string ftl_auth_token_;
  net::BackoffEntry sign_in_backoff_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FTL_REGISTRATION_MANAGER_H_
