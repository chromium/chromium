// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_
#define IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "components/captive_portal/core/captive_portal_types.h"
#include "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/web_state_user_data.h"
#include "url/gurl.h"

namespace net {
class SSLInfo;
}  // namespace net

namespace web {
class WebState;
}  // namespace web

// Default delay in seconds before displaying the SSL interstitial.
// - If a "captive portal detected" result arrives during this time,
//   a captive portal interstitial is displayed.
// - Otherwise, an SSL interstitial is displayed.
extern const int64_t kSSLInterstitialDelayInSeconds;

// This class is responsible for deciding what type of interstitial to show for
// an SSL validation error.
class IOSSSLErrorHandler : public web::WebStateUserData<IOSSSLErrorHandler> {
 public:
  // Entry point for the class.
  static void HandleSSLError(
      web::WebState* web_state,
      int cert_error,
      const net::SSLInfo& info,
      const GURL& request_url,
      bool overridable,
      int64_t navigation_id,
      base::OnceCallback<void(bool)> callback,
      base::OnceCallback<void(NSString*)> blocking_page_callback);
  ~IOSSSLErrorHandler() override;

 private:
  friend class web::WebStateUserData<IOSSSLErrorHandler>;

  // Creates an error handler for the given |web_state| and |request_url|.
  // The |cert_error| and SSL |info| represent the SSL error detected which
  // triggered the display of the SSL interstitial for a particular
  // |navigation_id|, which is retrieved from
  // NavigationContext::GetNavigationId(). If |overridable| is true, the
  // interstitial will allow the error to be ignored in order to proceed to
  // |request_url|. |callback| will be called after the user is done interacting
  // with this interstitial. |blocking_page_callback| will be called for
  // committed interstitials to fetch the HTML of the interstitial page.
  IOSSSLErrorHandler(
      web::WebState* web_state,
      int cert_error,
      const net::SSLInfo& info,
      const GURL& request_url,
      bool overridable,
      int64_t navigation_id,
      base::OnceCallback<void(bool)> callback,
      base::OnceCallback<void(NSString*)> blocking_page_callback);

  // Begins captive portal detection to determine which interstitial should be
  // displayed.
  void StartHandlingError();
  // Presents the appropriate interstitial based on the |results| of the captive
  // portal detection.
  void HandleCaptivePortalDetectionResult(
      const captive_portal::CaptivePortalDetector::Results& results);
  // Displays an SSL error page interstitial, or if committed interstitials are
  // enabled, returns HTML content for the error page by running
  // |blocking_page_callback_|.
  void ShowSSLInterstitial();
  // Displays a Captive Portal interstitial, or if committed interstitials are
  // enabled, returns the HTML content for the error page by running
  // |blocking_page_callback_|. The |landing_url| is the web page which allows
  // the user to complete their connection to the network.
  void ShowCaptivePortalInterstitial(const GURL& landing_url);
  // Records a metric to classify if SSL errors are due to a Captive Portal
  // state.
  static void LogCaptivePortalResult(
      captive_portal::CaptivePortalResult result);

  // The WebState associated with this error handler.
  web::WebState* const web_state_ = nullptr;
  // The ssl certificate error.
  const int cert_error_ = 0;
  // The ssl certificate details.
  const net::SSLInfo ssl_info_;
  // The request the user was loading which triggered the certificate error.
  const GURL request_url_;
  // Whether or not the user can ignore this error in order to continue loading
  // |request_url_|.
  const bool overridable_ = false;
  // The id of the navigation.
  const int64_t navigation_id_ = 0;
  // The callback to run after the user is done interacting with this
  // interstitial. |proceed| will be true if the user wants to procced with the
  // page load of |request_url_|, false otherwise.
  base::OnceCallback<void(bool proceed)> callback_;
  // The callback to run for showing a committed interstitial.
  base::OnceCallback<void(NSString*)> blocking_page_callback_;
  // A timer to display the SSL interstitial if the captive portal detection
  // takes too long.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<IOSSSLErrorHandler> weak_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(IOSSSLErrorHandler);
};

#endif  // IOS_CHROME_BROWSER_SSL_IOS_SSL_ERROR_HANDLER_H_
