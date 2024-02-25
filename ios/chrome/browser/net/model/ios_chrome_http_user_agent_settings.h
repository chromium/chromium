// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_HTTP_USER_AGENT_SETTINGS_H_
#define IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_HTTP_USER_AGENT_SETTINGS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "ios/chrome/browser/net/model/accept_language_pref_watcher.h"
#include "net/base/http_user_agent_settings.h"

// An implementation of `HttpUserAgentSettings` that provides HTTP header
// Accept-Language value that tracks Pref settings.
class IOSChromeHttpUserAgentSettings : public net::HttpUserAgentSettings {
 public:
  // Must be called on the UI thread.
  explicit IOSChromeHttpUserAgentSettings(
      scoped_refptr<AcceptLanguagePrefWatcher::Handle> accept_language_handle);

  IOSChromeHttpUserAgentSettings(const IOSChromeHttpUserAgentSettings&) =
      delete;
  IOSChromeHttpUserAgentSettings& operator=(
      const IOSChromeHttpUserAgentSettings&) = delete;

  // Must be called on the IO thread.
  ~IOSChromeHttpUserAgentSettings() override;

  // net::HttpUserAgentSettings implementation
  std::string GetAcceptLanguage() const override;
  std::string GetUserAgent() const override;

 private:
  scoped_refptr<AcceptLanguagePrefWatcher::Handle> accept_language_handle_;
};

#endif  // IOS_CHROME_BROWSER_NET_MODEL_IOS_CHROME_HTTP_USER_AGENT_SETTINGS_H_
