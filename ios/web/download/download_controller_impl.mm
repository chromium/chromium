// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/download_controller_impl.h"

#include "base/strings/sys_string_conversions.h"
#import "ios/web/download/download_session_cookie_storage.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/download/download_controller_delegate.h"
#import "ios/web/public/web_client.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_request_headers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kDownloadControllerKey = 0;
}  // namespace

namespace web {

// static
DownloadController* DownloadController::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);
  if (!browser_state->GetUserData(&kDownloadControllerKey)) {
    browser_state->SetUserData(&kDownloadControllerKey,
                               std::make_unique<DownloadControllerImpl>());
  }
  return static_cast<DownloadControllerImpl*>(
      browser_state->GetUserData(&kDownloadControllerKey));
}

DownloadControllerImpl::DownloadControllerImpl() = default;

DownloadControllerImpl::~DownloadControllerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  for (DownloadTaskImpl* task : alive_tasks_)
    task->ShutDown();

  if (delegate_)
    delegate_->OnDownloadControllerDestroyed(this);

  DCHECK(!delegate_);
}

void DownloadControllerImpl::CreateDownloadTask(
    WebState* web_state,
    NSString* identifier,
    const GURL& original_url,
    NSString* http_method,
    const std::string& content_disposition,
    int64_t total_bytes,
    const std::string& mime_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  if (!delegate_)
    return;

  auto task = std::make_unique<DownloadTaskImpl>(
      web_state, original_url, http_method, content_disposition, total_bytes,
      mime_type, identifier, this);
  alive_tasks_.insert(task.get());
  delegate_->OnDownloadCreated(this, web_state, std::move(task));
}

void DownloadControllerImpl::SetDelegate(DownloadControllerDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  delegate_ = delegate;
}

DownloadControllerDelegate* DownloadControllerImpl::GetDelegate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  return delegate_;
}

void DownloadControllerImpl::OnTaskDestroyed(DownloadTaskImpl* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  auto it = alive_tasks_.find(task);
  DCHECK(it != alive_tasks_.end());
  alive_tasks_.erase(it);
}

NSURLSession* DownloadControllerImpl::CreateSession(
    NSString* identifier,
    NSArray<NSHTTPCookie*>* cookies,
    id<NSURLSessionDataDelegate> delegate,
    NSOperationQueue* delegate_queue) {
  NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration
      backgroundSessionConfigurationWithIdentifier:identifier];
  configuration.HTTPCookieStorage = [[DownloadSessionCookieStorage alloc] init];
  configuration.HTTPCookieStorage.cookieAcceptPolicy =
      [NSHTTPCookieStorage sharedHTTPCookieStorage].cookieAcceptPolicy;
  // Cookies have to be set in session configuration before the session is
  // created. Once the session is created, the configuration object can't be
  // edited and configuration property will return a copy of the originally used
  // configuration.
  for (NSHTTPCookie* cookie in cookies) {
    // Cookies copied from the internal WebSiteDataStore cookie store, so
    // there will be no duplications or invalid cookies.
    [configuration.HTTPCookieStorage setCookie:cookie];
  }
  std::string user_agent = GetWebClient()->GetUserAgent(UserAgentType::MOBILE);
  configuration.HTTPAdditionalHeaders = @{
    base::SysUTF8ToNSString(net::HttpRequestHeaders::kUserAgent) :
        base::SysUTF8ToNSString(user_agent),
  };

  return [NSURLSession sessionWithConfiguration:configuration
                                       delegate:delegate
                                  delegateQueue:delegate_queue];
}

}  // namespace web
