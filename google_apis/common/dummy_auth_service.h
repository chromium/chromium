// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_DUMMY_AUTH_SERVICE_H_
#define GOOGLE_APIS_COMMON_DUMMY_AUTH_SERVICE_H_

#include "base/compiler_specific.h"
#include "google_apis/common/auth_service_interface.h"

namespace google_apis {

// Dummy implementation of AuthServiceInterface that always return a dummy
// access token.
class DummyAuthService : public AuthServiceInterface {
 public:
  // The constructor presets non-empty tokens. When a test for checking auth
  // failure case (i.e., empty tokens) is needed, explicitly clear them by the
  // Clear{Access, Refresh}Token methods.
  DummyAuthService();

  void set_access_token(const std::string& token) { access_token_ = token; }
  void set_refresh_token(const std::string& token) { refresh_token_ = token; }
  const std::string& refresh_token() const { return refresh_token_; }

  // AuthServiceInterface overrides.
  void AddObserver(AuthServiceObserver* observer) override;
  void RemoveObserver(AuthServiceObserver* observer) override;
  void StartAuthentication(AuthStatusCallback callback) override;
  bool HasAccessToken() const override;
  bool HasRefreshToken() const override;
  const std::string& access_token() const override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;

 private:
  std::string access_token_;
  std::string refresh_token_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_DUMMY_AUTH_SERVICE_H_
