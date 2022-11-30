// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_CONTAINER_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_CONTAINER_H_

#include <set>
#include <string>
#include <vector>

#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace web {
class WebState;
}

// Helper object that holds URL parameters for handling interstitial reloads and
// information about the navigation used to populate the interstitial page.
class HttpsOnlyModeContainer
    : public web::WebStateUserData<HttpsOnlyModeContainer> {
 public:
  // HttpsOnlyModeContainer is move-only.
  HttpsOnlyModeContainer(HttpsOnlyModeContainer&& other);
  HttpsOnlyModeContainer& operator=(HttpsOnlyModeContainer&& other);
  ~HttpsOnlyModeContainer() override;

  struct InterstitialParams {
    // Only a limited amount of information is stored here. This might not be
    // sufficient to construct the original navigation in some edge cases.
    // However, the upgraded navigation will always be a main-frame,
    // non-POST navigation so we are OK with limited information here.
    GURL url;
    std::vector<GURL> redirect_chain;
    web::Referrer referrer;

    InterstitialParams();
    InterstitialParams(const InterstitialParams& other);
    ~InterstitialParams();
  };

  // Stores parameters associated with an HTTPS-Only blocking page. Must be
  // called when an HTTPS-Only blocking page is shown.
  void RecordBlockingPageParams(const GURL& url,
                                const web::Referrer& referrer,
                                const std::vector<GURL>& redirect_chain);

  // Stores the URL to go to when the interstitial is dismissed.
  void SetHttpUrl(const GURL& http_url);
  // Returns the URL to go to when the interstitial is dismissed.
  GURL http_url() const;

  // Returns currently stored parameters associated with a blocking
  // page, transferring ownership to the caller.
  std::unique_ptr<InterstitialParams> ReleaseInterstitialParams();

 private:
  explicit HttpsOnlyModeContainer(web::WebState* web_state);
  friend class web::WebStateUserData<HttpsOnlyModeContainer>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Parameters associated with the currently displayed blocking page. These are
  // cleared immediately on next navigation.
  std::unique_ptr<InterstitialParams> interstitial_params_;

  // URL to navigate to when the HTTPS-Only mode interstitial is dismissed.
  GURL http_url_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_HTTPS_ONLY_MODE_HTTPS_ONLY_MODE_CONTAINER_H_
