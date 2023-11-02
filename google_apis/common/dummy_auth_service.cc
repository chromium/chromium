// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/common/dummy_auth_service.h"

namespace google_apis {

DummyAuthService::DummyAuthService() {
  set_access_token("dummy");
  set_refresh_token("dummy");
}

void DummyAuthService::AddObserver(AuthServiceObserver* observer) {}

void DummyAuthService::RemoveObserver(AuthServiceObserver* observer) {}

void DummyAuthService::StartAuthentication(AuthStatusCallback callback) {}

bool DummyAuthService::HasAccessToken() const {
  return !access_token_.empty();
}

bool DummyAuthService::HasRefreshToken() const {
  return !refresh_token_.empty();
}

const std::string& DummyAuthService::access_token() const {
  return access_token_;
}

void DummyAuthService::ClearAccessToken() {
  access_token_.clear();
}

void DummyAuthService::ClearRefreshToken() {
  refresh_token_.clear();
}

}  // namespace google_apis
