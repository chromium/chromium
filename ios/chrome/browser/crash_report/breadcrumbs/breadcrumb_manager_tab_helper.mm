// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#import "base/ios/ns_error_util.h"
#import "base/strings/stringprintf.h"
#import "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/security_style.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns true if navigation URL repesents Chrome's New Tab Page.
bool IsNtpUrl(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() == kChromeUINewTabURL ||
         (url.SchemeIs(url::kAboutScheme) &&
          (url.path() == "//newtab" || url.path() == "//newtab/"));
}

}  // namespace

using LoggingBlock = void (^)(const std::string& event);

// Observes scroll and zoom events and executes LoggingBlock.
@interface BreadcrumbScrollingObserver
    : NSObject <CRWWebViewScrollViewProxyObserver>
- (instancetype)initWithLoggingBlock:(LoggingBlock)loggingBlock;
@end
@implementation BreadcrumbScrollingObserver {
  LoggingBlock _loggingBlock;
}

- (instancetype)initWithLoggingBlock:(LoggingBlock)loggingBlock {
  if (self = [super init]) {
    _loggingBlock = [loggingBlock copy];
  }
  return self;
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  _loggingBlock(breadcrumbs::kBreadcrumbScroll);
}

- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale {
  _loggingBlock(breadcrumbs::kBreadcrumbZoom);
}

@end

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(web::WebState* web_state)
    : breadcrumbs::BreadcrumbManagerTabHelper(
          InfoBarManagerImpl::FromWebState(web_state)),
      web_state_(web_state) {
  web_state_->AddObserver(this);

  scroll_observer_ = [[BreadcrumbScrollingObserver alloc]
      initWithLoggingBlock:^(const std::string& event) {
        if (event == breadcrumbs::kBreadcrumbScroll) {
          sequentially_scrolled_++;
          if (ShouldLogRepeatedEvent(sequentially_scrolled_)) {
            LogEvent(base::StringPrintf("%s %d", breadcrumbs::kBreadcrumbScroll,
                                        sequentially_scrolled_));
          }
        } else {
          LogEvent(event);
        }
      }];
  [[web_state->GetWebViewProxy() scrollViewProxy] addObserver:scroll_observer_];
}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

void BreadcrumbManagerTabHelper::PlatformLogEvent(const std::string& event) {
  const bool is_scroll_event =
      event.find(breadcrumbs::kBreadcrumbScroll) != std::string::npos;
  if (!is_scroll_event) {
    // `sequentially_scrolled_` is incremented for each scroll event and reset
    // here when non-scrolling event is logged. The user can scroll multiple
    // times and `sequentially_scrolled_` will allow to throttle the logs to
    // avoid polluting breadcrumbs.
    sequentially_scrolled_ = 0;
  }

  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
      web_state_->GetBrowserState())
      ->AddEvent(event);
}

void BreadcrumbManagerTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  LogDidStartNavigation(navigation_context->GetNavigationId(),
                        navigation_context->GetUrl(),
                        IsNtpUrl(navigation_context->GetUrl()),
                        navigation_context->IsRendererInitiated(),
                        navigation_context->HasUserGesture(),
                        navigation_context->GetPageTransition());
}

void BreadcrumbManagerTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  NSError* error = navigation_context->GetError();
  int error_code = 0;
  if (error) {
    error_code = net::ERR_FAILED;
    NSError* final_error = base::ios::GetFinalUnderlyingErrorFromError(error);
    // Only errors with net::kNSErrorDomain have correct net error code.
    if (final_error && [final_error.domain isEqual:net::kNSErrorDomain]) {
      error_code = final_error.code;
    }
  }
  LogDidFinishNavigation(navigation_context->GetNavigationId(),
                         navigation_context->IsDownload(), error_code);
}

void BreadcrumbManagerTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  LogPageLoaded(
      IsNtpUrl(web_state->GetLastCommittedURL()),
      web_state->GetLastCommittedURL(),
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS,
      web_state->GetContentsMimeType());
}

void BreadcrumbManagerTabHelper::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!visible_item)
    return;
  const web::SSLStatus& ssl = visible_item->GetSSL();

  const bool displayed_mixed_content =
      ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  const bool security_style_authentication_broken =
      ssl.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN;

  LogDidChangeVisibleSecurityState(displayed_mixed_content,
                                   security_style_authentication_broken);
}

void BreadcrumbManagerTabHelper::RenderProcessGone(web::WebState* web_state) {
  LogRenderProcessGone();
}

void BreadcrumbManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);

  [[web_state->GetWebViewProxy() scrollViewProxy]
      removeObserver:scroll_observer_];
  scroll_observer_ = nil;
  web_state_ = nil;
}

WEB_STATE_USER_DATA_KEY_IMPL(BreadcrumbManagerTabHelper)
