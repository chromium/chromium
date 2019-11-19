// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_DETECTOR_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_DETECTOR_TAB_HELPER_H_

#import "ios/web/public/web_state_user_data.h"

@protocol CaptivePortalDetectorTabHelperDelegate;

namespace captive_portal {
class CaptivePortalDetector;
}

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

namespace web {
class WebState;
}

// Associates a Tab to a CaptivePortalDetector and manages its lifetime.
class CaptivePortalDetectorTabHelper
    : public web::WebStateUserData<CaptivePortalDetectorTabHelper> {
 public:
  ~CaptivePortalDetectorTabHelper() override;

  // Creates a Tab Helper and attaches it to |web_state|. The |delegate| is not
  // retained by the CaptivePortalDetectorTabHelper and must not be nil.
  static void CreateForWebState(
      web::WebState* web_state,
      id<CaptivePortalDetectorTabHelperDelegate> delegate,
      network::mojom::URLLoaderFactory* loader_factory_for_testing = nullptr);

  // Returns the associated captive portal detector.
  captive_portal::CaptivePortalDetector* detector();

  // Displays the Captive Portal Login page at |landing_url|.
  void DisplayCaptivePortalLoginPage(GURL landing_url);

 private:
  friend class web::WebStateUserData<CaptivePortalDetectorTabHelper>;

  CaptivePortalDetectorTabHelper(
      web::WebState* web_state,
      id<CaptivePortalDetectorTabHelperDelegate> delegate,
      network::mojom::URLLoaderFactory* loader_factory_for_testing);

  // The delegate to notify when the user performs an action in response to the
  // captive portal detector state.
  __weak id<CaptivePortalDetectorTabHelperDelegate> delegate_;
  // The underlying CaptivePortalDetector.
  std::unique_ptr<captive_portal::CaptivePortalDetector> detector_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalDetectorTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SSL_CAPTIVE_PORTAL_DETECTOR_TAB_HELPER_H_
