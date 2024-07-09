// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_NETWORK_DELEGATE_H_
#define IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_NETWORK_DELEGATE_H_

#include <stdint.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "net/base/network_delegate_impl.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/first_party_sets/first_party_set_metadata.h"

class PrefService;

template <typename T>
class PrefMember;

typedef PrefMember<bool> BooleanPrefMember;

// IOSChromeNetworkDelegate is the central point from within the Chrome code to
// add hooks into the network stack.
class IOSChromeNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  IOSChromeNetworkDelegate();

  IOSChromeNetworkDelegate(const IOSChromeNetworkDelegate&) = delete;
  IOSChromeNetworkDelegate& operator=(const IOSChromeNetworkDelegate&) = delete;

  ~IOSChromeNetworkDelegate() override;

  // If `cookie_settings` is null or not set, all cookies are enabled,
  // otherwise the settings are enforced on all observed network requests.
  // Not inlined because we assign a scoped_refptr, which requires us to include
  // the header file. Here we just forward-declare it.
  void set_cookie_settings(content_settings::CookieSettings* cookie_settings) {
    cookie_settings_ = cookie_settings;
  }

  void set_enable_do_not_track(BooleanPrefMember* enable_do_not_track) {
    enable_do_not_track_ = enable_do_not_track;
  }

  // Binds the pref members to `pref_service` and moves them to the IO thread.
  // This method should be called on the UI thread.
  static void InitializePrefsOnUIThread(BooleanPrefMember* enable_do_not_track,
                                        PrefService* pref_service);

 private:
  // NetworkDelegate implementation.
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  bool OnAnnotateAndMoveUserBlockedCookies(
      const net::URLRequest& request,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieAccessResultList& maybe_included_cookies,
      net::CookieAccessResultList& excluded_cookies) override;
  bool OnCanSetCookie(
      const net::URLRequest& request,
      const net::CanonicalCookie& cookie,
      net::CookieOptions* options,
      const net::FirstPartySetMetadata& first_party_set_metadata,
      net::CookieInclusionStatus* inclusion_status) override;
  std::optional<net::cookie_util::StorageAccessStatus> OnGetStorageAccessStatus(
      const net::URLRequest& request) const override;
  net::NetworkDelegate::PrivacySetting OnForcePrivacyMode(
      const net::URLRequest& request) const override;
  bool OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      const net::URLRequest& request,
      const GURL& target_url,
      const GURL& referrer_url) const override;

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;

  // Weak, owned by our owner.
  raw_ptr<BooleanPrefMember> enable_do_not_track_;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_NETWORK_DELEGATE_H_
