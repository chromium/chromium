// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_COMMON_AUTH_SERVICE_OBSERVER_H_
#define GOOGLE_APIS_COMMON_AUTH_SERVICE_OBSERVER_H_

namespace google_apis {

// Interface for classes that need to observe events from AuthService.
// All events are notified on UI thread.
class AuthServiceObserver {
 public:
  // Triggered when a new OAuth2 refresh token is received from AuthService.
  virtual void OnOAuth2RefreshTokenChanged() = 0;

 protected:
  virtual ~AuthServiceObserver() = default;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_COMMON_AUTH_SERVICE_OBSERVER_H_
