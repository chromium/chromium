// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_PAGE_H_
#define IOS_CHROME_BROWSER_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_PAGE_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ios/web/public/security/web_interstitial_delegate.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace web {
class WebInterstitial;
class WebState;
}

class IOSSecurityInterstitialPage : public web::WebInterstitialDelegate {
 public:
  IOSSecurityInterstitialPage(web::WebState* web_state,
                              const GURL& request_url);
  ~IOSSecurityInterstitialPage() override;

  // Creates an interstitial and shows it.
  void Show();

  // web::WebInterstitialDelegate implementation.
  std::string GetHtmlContents() const override;

 protected:
  // Returns true if the interstitial should create a new navigation item.
  virtual bool ShouldCreateNewNavigation() const = 0;

  // Populates the strings used to generate the HTML from the template.
  virtual void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) const = 0;

  // Gives an opportunity for child classes to react to Show() having run. The
  // |web_interstitial_| will now have a value.
  virtual void AfterShow() = 0;

  // Returns the formatted host name for the request url.
  base::string16 GetFormattedHostName() const;

  // Returns the boolean value of the given |pref_name| from the PrefService of
  // the ChromeBrowserState associated with |web_state_|.
  bool IsPrefEnabled(const char* pref_name) const;

  web::WebState* web_state() const { return web_state_; }
  const GURL& request_url() const { return request_url_; }
  web::WebInterstitial* web_interstitial() const { return web_interstitial_; }

 private:
  // The WebState with which this interstitial page is associated. Not
  // available in the destructor since the it can be destroyed before this
  // class is destroyed.
  web::WebState* web_state_;
  const GURL request_url_;

  // Once non-null, the |web_interstitial_| takes ownership of this
  // IOSSecurityInterstitialPage instance.
  web::WebInterstitial* web_interstitial_;

  DISALLOW_COPY_AND_ASSIGN(IOSSecurityInterstitialPage);
};

#endif  // IOS_CHROME_BROWSER_INTERSTITIALS_IOS_SECURITY_INTERSTITIAL_PAGE_H_
