// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_AUTH_SERVICE_H_
#define GOOGLE_APIS_COMMON_AUTH_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "google_apis/common/auth_service_interface.h"
#include "google_apis/gaia/core_account_id.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace google_apis {

class AuthServiceObserver;

// This class provides authentication for Google services.
// It integrates specific service integration with the Identity service
// (IdentityManager) and provides OAuth2 token refresh infrastructure.
// All public functions must be called on UI thread.
class AuthService : public AuthServiceInterface {
 public:
  // |url_loader_factory| is used to perform authentication with
  // SimpleURLLoader.
  //
  // |scopes| specifies OAuth2 scopes.
  AuthService(signin::IdentityManager* identity_manager,
              const CoreAccountId& account_id,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const std::vector<std::string>& scopes);
  AuthService(const AuthService&) = delete;
  AuthService& operator=(const AuthService&) = delete;
  ~AuthService() override;

  // Overriden from AuthServiceInterface:
  void AddObserver(AuthServiceObserver* observer) override;
  void RemoveObserver(AuthServiceObserver* observer) override;
  void StartAuthentication(AuthStatusCallback callback) override;
  bool HasAccessToken() const override;
  bool HasRefreshToken() const override;
  const std::string& access_token() const override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;

 private:
  class IdentityManagerObserver;

  // Called when the state of the refresh token changes.
  void OnHandleRefreshToken(const CoreAccountId& account_id,
                            bool has_refresh_token);

  // Called when authentication request from StartAuthentication() is
  // completed.
  void OnAuthCompleted(AuthStatusCallback callback,
                       ApiErrorCode error,
                       const std::string& access_token);

  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<IdentityManagerObserver> identity_manager_observer_;
  CoreAccountId account_id_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  bool has_refresh_token_;
  std::string access_token_;
  std::vector<std::string> scopes_;
  base::ObserverList<AuthServiceObserver>::Unchecked observers_;
  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AuthService> weak_ptr_factory_{this};
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_AUTH_SERVICE_H_
