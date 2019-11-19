// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_IOS_CHROME_NETWORK_DELEGATE_H_
#define IOS_CHROME_BROWSER_NET_IOS_CHROME_NETWORK_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "net/base/network_delegate_impl.h"

class PrefService;

template <typename T>
class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

// IOSChromeNetworkDelegate is the central point from within the Chrome code to
// add hooks into the network stack.
class IOSChromeNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  IOSChromeNetworkDelegate();
  ~IOSChromeNetworkDelegate() override;

  // If |cookie_settings| is null or not set, all cookies are enabled,
  // otherwise the settings are enforced on all observed network requests.
  // Not inlined because we assign a scoped_refptr, which requires us to include
  // the header file. Here we just forward-declare it.
  void set_cookie_settings(content_settings::CookieSettings* cookie_settings) {
    cookie_settings_ = cookie_settings;
  }

  void set_enable_do_not_track(BooleanPrefMember* enable_do_not_track) {
    enable_do_not_track_ = enable_do_not_track;
  }

  // Binds the pref members to |pref_service| and moves them to the IO thread.
  // This method should be called on the UI thread.
  static void InitializePrefsOnUIThread(BooleanPrefMember* enable_do_not_track,
                                        PrefService* pref_service);

 private:
  // NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  bool OnCanGetCookies(const net::URLRequest& request,
                       const net::CookieList& cookie_list,
                       bool allowed_from_caller) override;
  bool OnCanSetCookie(const net::URLRequest& request,
                      const net::CanonicalCookie& cookie,
                      net::CookieOptions* options,
                      bool allowed_from_caller) override;
  bool OnForcePrivacyMode(
      const GURL& url,
      const GURL& site_for_cookies,
      const base::Optional<url::Origin>& top_frame_origin) const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  // Weak, owned by our owner.
  BooleanPrefMember* enable_do_not_track_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeNetworkDelegate);
};

#endif  // IOS_CHROME_BROWSER_NET_IOS_CHROME_NETWORK_DELEGATE_H_
