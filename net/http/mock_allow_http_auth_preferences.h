// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_MOCK_ALLOW_HTTP_AUTH_PREFERENCES_H_
#define NET_HTTP_MOCK_ALLOW_HTTP_AUTH_PREFERENCES_H_

#include "net/http/http_auth_preferences.h"

namespace url {
class SchemeHostPort;
}

namespace net {

// An HttpAuthPreferences class which allows all origins to use default
// credentials and delegate. This should only be used in unit testing.
class MockAllowHttpAuthPreferences : public HttpAuthPreferences {
 public:
  MockAllowHttpAuthPreferences();

  MockAllowHttpAuthPreferences(const MockAllowHttpAuthPreferences&) = delete;
  MockAllowHttpAuthPreferences& operator=(const MockAllowHttpAuthPreferences&) =
      delete;

  ~MockAllowHttpAuthPreferences() override;

  bool CanUseDefaultCredentials(
      const url::SchemeHostPort& auth_scheme_host_port) const override;
  HttpAuth::DelegationType GetDelegationType(
      const url::SchemeHostPort& auth_scheme_host_port) const override;
};

}  // namespace net

#endif  // NET_HTTP_MOCK_ALLOW_HTTP_AUTH_PREFERENCES_H_
