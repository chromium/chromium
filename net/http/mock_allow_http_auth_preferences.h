// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_MOCK_ALLOW_HTTP_AUTH_PREFERENCES_H_
#define NET_HTTP_MOCK_ALLOW_HTTP_AUTH_PREFERENCES_H_

#include "base/macros.h"
#include "net/http/http_auth_preferences.h"

namespace net {

// An HttpAuthPreferences class which allows all origins to use default
// credentials and delegate. This should only be used in unit testing.
class MockAllowHttpAuthPreferences : public HttpAuthPreferences {
 public:
  MockAllowHttpAuthPreferences();
  ~MockAllowHttpAuthPreferences() override;

  bool CanUseDefaultCredentials(const GURL& auth_origin) const override;
  HttpAuth::DelegationType GetDelegationType(
      const GURL& auth_origin) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAllowHttpAuthPreferences);
};

}  // namespace net

#endif  // NET_HTTP_MOCK_ALLOW_HTTP_AUTH_PREFERENCES_H_
