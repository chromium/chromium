// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/navigation/url_schemes.h"

#import <algorithm>
#import <vector>

#import "ios/web/public/web_client.h"
#import "url/url_util.h"

namespace web {

void RegisterWebSchemes() {
  web::WebClient::Schemes schemes;
  GetWebClient()->AddAdditionalSchemes(&schemes);
  for (const auto& scheme : schemes.standard_schemes)
    url::AddStandardScheme(scheme.c_str(), url::SCHEME_WITH_HOST);

  for (const auto& scheme : schemes.secure_schemes)
    url::AddSecureScheme(scheme.c_str());

  // Prevent future modification of the schemes lists. This is to prevent
  // accidental creation of data races in the program. Add*Scheme aren't
  // threadsafe so must be called when GURL isn't used on any other thread. This
  // is really easy to mess up, so we say that all calls to Add*Scheme in Chrome
  // must be inside this function.
  url::LockSchemeRegistries();
}

}  // namespace web
