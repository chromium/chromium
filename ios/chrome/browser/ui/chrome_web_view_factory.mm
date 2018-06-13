// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/chrome_web_view_factory.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/web/net/request_group_util.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/public/web_thread.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kExternalUserAgent = @"UIWebViewForExternalContent";

namespace ChromeWebView {
// Shared desktop user agent used to mimic Safari on a mac.
NSString* const kDesktopUserAgent =
    @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_13_4) "
    @"AppleWebKit/605.1.15 (KHTML, like Gecko) "
    @"Version/11.1 "
    @"Safari/605.1.15";

NSString* const kExternalRequestGroupID = @"kExternalRequestGroupID";

// Returns an absolute path where sharing service data is stored (such as
// cookies and server-bound certificates).
const base::FilePath GetExternalServicePath(
    ios::ChromeBrowserState* browser_state,
    IOSWebViewFactoryExternalService service) {
  const base::FilePath path =
      browser_state->GetOriginalChromeBrowserState()->GetStatePath();
  switch (service) {
    case SSO_AUTHENTICATION:
      return path.Append("SSOAuthentication");
    case NUM_SHARING_SERVICES:
      NOTREACHED();
      return path.Append("ExternalUnknownService");
  }
}
}  // namespace ChromeWebView

namespace {
ios::ChromeBrowserState* g_external_browser_state = nullptr;
scoped_refptr<web::RequestTrackerImpl> g_request_tracker;

// Clears the cookies.
void ClearCookiesOnIOThread(
    net::URLRequestContextGetter* context_getter,
    const net::CookieDeletionInfo::TimeRange& creation_range) {
  DCHECK(context_getter);
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedInTimeRangeAsync(creation_range,
                                                 base::DoNothing());
}

// Registers |user_agent| as the user agent string to be used by the UIWebView
// instances that are created from now on.
void RegisterUserAgentForUIWebView(NSString* user_agent) {
  [[NSUserDefaults standardUserDefaults] registerDefaults:@{
    @"UserAgent" : user_agent,
  }];
}

}  // namespace

@implementation ChromeWebViewFactory

+ (void)setBrowserStateToUseForExternal:(ios::ChromeBrowserState*)browserState {
  g_external_browser_state = browserState;
}

+ (void)externalSessionFinished {
  g_external_browser_state = nullptr;
  if (g_request_tracker.get())
    g_request_tracker->Close();
  g_request_tracker = nullptr;
}

+ (net::URLRequestContextGetter*)requestContextForExternalService:
    (IOSWebViewFactoryExternalService)externalService {
  DCHECK(g_external_browser_state);
  const base::FilePath servicePath =
      [ChromeWebViewFactory partitionPathForExternalService:externalService];
  return g_external_browser_state->CreateIsolatedRequestContext(servicePath);
}

+ (base::FilePath)partitionPathForExternalService:
    (IOSWebViewFactoryExternalService)externalService {
  return ChromeWebView::GetExternalServicePath(g_external_browser_state,
                                               externalService);
}

+ (ChromeBrowserStateIOData*)ioDataForExternalWebViews {
  DCHECK(g_external_browser_state);
  return g_external_browser_state->GetIOData();
}

+ (void)clearExternalCookies:(IOSWebViewFactoryExternalService)externalService
                browserState:(ios::ChromeBrowserState*)browserState
                    fromTime:(base::Time)deleteBegin
                      toTime:(base::Time)deleteEnd {
  const base::FilePath servicePath =
      ChromeWebView::GetExternalServicePath(browserState, externalService);
  scoped_refptr<net::URLRequestContextGetter> contextGetter =
      browserState->CreateIsolatedRequestContext(servicePath);
  [self clearCookiesForContextGetter:contextGetter
                            fromTime:deleteBegin
                              toTime:deleteEnd];
}

+ (void)clearCookiesForContextGetter:
            (scoped_refptr<net::URLRequestContextGetter>)contextGetter
                            fromTime:(base::Time)deleteBegin
                              toTime:(base::Time)deleteEnd {
  DCHECK(contextGetter.get());
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&ClearCookiesOnIOThread, base::RetainedRef(contextGetter),
                 net::CookieDeletionInfo::TimeRange(deleteBegin, deleteEnd)));
}

+ (void)clearExternalCookies:(ios::ChromeBrowserState*)browserState
                    fromTime:(base::Time)deleteBegin
                      toTime:(base::Time)deleteEnd {
  for (unsigned int i = 0; i < NUM_SHARING_SERVICES; ++i) {
    [ChromeWebViewFactory
        clearExternalCookies:static_cast<IOSWebViewFactoryExternalService>(i)
                browserState:browserState
                    fromTime:deleteBegin
                      toTime:deleteEnd];
  }
}

#pragma mark -
#pragma mark ChromeWebViewFactory
+ (UIWebView*)newExternalWebView:
    (IOSWebViewFactoryExternalService)externalService {
  // All UIWebView's created for sharing share the same user agent, as there is
  // no need for them to be differentiated and this choice ensures that only
  // one request tracker is created on a per-sharing-session basis.
  NSString* userAgent = web::AddRequestGroupIDToUserAgent(
      kExternalUserAgent, ChromeWebView::kExternalRequestGroupID);
  RegisterUserAgentForUIWebView(userAgent);
  if (!g_request_tracker.get()) {
    DCHECK(g_external_browser_state);
    g_request_tracker = web::RequestTrackerImpl::CreateTrackerForRequestGroupID(
        ChromeWebView::kExternalRequestGroupID, g_external_browser_state,
        [self requestContextForExternalService:externalService]);
  }
  return [[UIWebView alloc] initWithFrame:CGRectZero];
}

@end
