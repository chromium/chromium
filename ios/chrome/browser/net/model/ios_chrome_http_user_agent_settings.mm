// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/model/ios_chrome_http_user_agent_settings.h"

#import "base/check.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"

IOSChromeHttpUserAgentSettings::IOSChromeHttpUserAgentSettings(
    scoped_refptr<AcceptLanguagePrefWatcher::Handle> accept_language_handle)
    : accept_language_handle_(accept_language_handle) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  DCHECK(accept_language_handle_);
}

IOSChromeHttpUserAgentSettings::~IOSChromeHttpUserAgentSettings() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
}

std::string IOSChromeHttpUserAgentSettings::GetAcceptLanguage() const {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return accept_language_handle_->GetAcceptLanguageHeader();
}

std::string IOSChromeHttpUserAgentSettings::GetUserAgent() const {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE);
}
