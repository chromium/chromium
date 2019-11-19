// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_IOS_CHROME_HTTP_USER_AGENT_SETTINGS_H_
#define IOS_CHROME_BROWSER_NET_IOS_CHROME_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/prefs/pref_member.h"
#include "net/base/http_user_agent_settings.h"

class PrefService;

// An implementation of |HttpUserAgentSettings| that provides HTTP header
// Accept-Language value that tracks Pref settings.
class IOSChromeHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  // Must be called on the UI thread.
  explicit IOSChromeHttpUserAgentSettings(PrefService* prefs);
  // Must be called on the IO thread.
  ~IOSChromeHttpUserAgentSettings() override;

  void CleanupOnUIThread();

  // net::HttpUserAgentSettings implementation
  std::string GetAcceptLanguage() const override;
  std::string GetUserAgent() const override;

 private:
  StringPrefMember pref_accept_language_;

  // Avoid re-processing by caching the last value from the preferences and the
  // last result of processing via net::HttpUtil::GenerateAccept*Header().
  mutable std::string last_pref_accept_language_;
  mutable std::string last_http_accept_language_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeHttpUserAgentSettings);
};

#endif  // IOS_CHROME_BROWSER_NET_IOS_CHROME_HTTP_USER_AGENT_SETTINGS_H_
