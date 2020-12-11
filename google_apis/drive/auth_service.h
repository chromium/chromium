// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_AUTH_SERVICE_H_
#define GOOGLE_APIS_DRIVE_AUTH_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/drive/auth_service_interface.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace google_apis {

class AuthServiceObserver;

// This class provides authentication for Google services.
// It integrates specific service integration with the Identity service
// (IdentityManager) and provides OAuth2 token refresh infrastructure.
// All public functions must be called on UI thread.
class AuthService : public AuthServiceInterface,
                    public signin::IdentityManager::Observer {
 public:
  // |url_loader_factory| is used to perform authentication with
  // SimpleURLLoader.
  //
  // |scopes| specifies OAuth2 scopes.
  AuthService(signin::IdentityManager* identity_manager,
              const CoreAccountId& account_id,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
              const std::vector<std::string>& scopes);
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

  // Overridden from IdentityManager::Observer
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

 private:
  // Called when the state of the refresh token changes.
  void OnHandleRefreshToken(bool has_refresh_token);

  // Called when authentication request from StartAuthentication() is
  // completed.
  void OnAuthCompleted(AuthStatusCallback callback,
                       DriveApiErrorCode error,
                       const std::string& access_token);

  signin::IdentityManager* identity_manager_;
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

  DISALLOW_COPY_AND_ASSIGN(AuthService);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_AUTH_SERVICE_H_
