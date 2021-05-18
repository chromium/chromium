// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_tab_helper.h"

#import "base/ios/ns_error_util.h"
#include "base/strings/stringprintf.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/net/protocol_handler_util.h"
#include "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/security_style.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns true if navigation URL repesents Chrome's New Tab Page.
bool IsNptUrl(const GURL& url) {
  return url.GetOrigin() == kChromeUINewTabURL ||
         (url.SchemeIs(url::kAboutScheme) &&
          (url.path() == "//newtab" || url.path() == "//newtab/"));
}
// Returns true if navigation URL host is google.com or www.google.com.
bool IsGoogleUrl(const GURL& url) {
  return url.host() == "google.com" || url.host() == "www.google.com";
}

// Returns true if event that was sequentially emitted |count| times should be
// logged. Some events (f.e. infobars replacements or scrolling) are emitted
// sequentially multiple times. Logging each event will pollute breadcrumbs, so
// this throttling function decides if event should be logged.
bool ShouldLogRepeatedEvent(int count) {
  return count == 1 || count == 2 || count == 5 || count == 20 ||
         count == 100 || count == 200;
}
}  // namespace

const char kBreadcrumbDidStartNavigation[] = "StartNav";
const char kBreadcrumbDidFinishNavigation[] = "FinishNav";
const char kBreadcrumbPageLoaded[] = "PageLoad";
const char kBreadcrumbDidChangeVisibleSecurityState[] = "SecurityChange";

const char kBreadcrumbInfobarAdded[] = "AddInfobar";
const char kBreadcrumbInfobarRemoved[] = "RemoveInfobar";
const char kBreadcrumbInfobarReplaced[] = "ReplaceInfobar";

const char kBreadcrumbScroll[] = "Scroll";
const char kBreadcrumbZoom[] = "Zoom";

const char kBreadcrumbAuthenticationBroken[] = "#broken";
const char kBreadcrumbDownload[] = "#download";
const char kBreadcrumbMixedContent[] = "#mixed";
const char kBreadcrumbInfobarNotAnimated[] = "#not-animated";
const char kBreadcrumbNtpNavigation[] = "#ntp";
const char kBreadcrumbGoogleNavigation[] = "#google";
const char kBreadcrumbPdfLoad[] = "#pdf";
const char kBreadcrumbPageLoadFailure[] = "#failure";
const char kBreadcrumbRendererInitiatedByUser[] = "#renderer-user";
const char kBreadcrumbRendererInitiatedByScript[] = "#renderer-script";

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
  _loggingBlock(kBreadcrumbScroll);
}

- (void)webViewScrollViewDidEndZooming:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                               atScale:(CGFloat)scale {
  _loggingBlock(kBreadcrumbZoom);
}

@end

BreadcrumbManagerTabHelper::BreadcrumbManagerTabHelper(web::WebState* web_state)
    : web_state_(web_state),
      infobar_manager_(InfoBarManagerImpl::FromWebState(web_state)) {
  web_state_->AddObserver(this);

  static int next_unique_id = 1;
  unique_id_ = next_unique_id++;

  infobar_observation_.Observe(infobar_manager_);

  scroll_observer_ = [[BreadcrumbScrollingObserver alloc]
      initWithLoggingBlock:^(const std::string& event) {
        if (event == kBreadcrumbScroll) {
          sequentially_scrolled_++;
          if (ShouldLogRepeatedEvent(sequentially_scrolled_)) {
            LogEvent(base::StringPrintf("%s %d", kBreadcrumbScroll,
                                        sequentially_scrolled_));
          }
        } else {
          LogEvent(event);
        }
      }];
  [[web_state->GetWebViewProxy() scrollViewProxy] addObserver:scroll_observer_];
}

BreadcrumbManagerTabHelper::~BreadcrumbManagerTabHelper() = default;

void BreadcrumbManagerTabHelper::LogEvent(const std::string& event) {
  bool is_scroll_event = event.find(kBreadcrumbScroll) != std::string::npos;
  if (!is_scroll_event) {
    // |sequentially_scrolled_| is incremented for each scroll event and reset
    // here when non-scrolling event is logged. The user can scroll multiple
    // times and |sequentially_scrolled_| will allow to throttle the logs to
    // avoid polluting breadcrumbs.
    sequentially_scrolled_ = 0;
  }

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());
  std::string event_log =
      base::StringPrintf("Tab%d %s", unique_id_, event.c_str());
  BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(chrome_browser_state)
      ->AddEvent(event_log);
}

void BreadcrumbManagerTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  std::vector<std::string> event = {
      base::StringPrintf("%s%lld", kBreadcrumbDidStartNavigation,
                         navigation_context->GetNavigationId()),
  };

  if (IsNptUrl(navigation_context->GetUrl())) {
    event.push_back(kBreadcrumbNtpNavigation);
  } else if (IsGoogleUrl(navigation_context->GetUrl())) {
    event.push_back(kBreadcrumbGoogleNavigation);
  }

  if (navigation_context->IsRendererInitiated()) {
    if (navigation_context->HasUserGesture()) {
      event.push_back(kBreadcrumbRendererInitiatedByUser);
    } else {
      event.push_back(kBreadcrumbRendererInitiatedByScript);
    }
  }

  event.push_back(
      base::StringPrintf("#%s", ui::PageTransitionGetCoreTransitionString(
                                    navigation_context->GetPageTransition())));

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  std::vector<std::string> event = {
      base::StringPrintf("%s%lld", kBreadcrumbDidFinishNavigation,
                         navigation_context->GetNavigationId()),
  };

  if (navigation_context->IsDownload()) {
    event.push_back(kBreadcrumbDownload);
  }

  NSError* error = navigation_context->GetError();
  if (error) {
    int code = net::ERR_FAILED;
    NSError* final_error = base::ios::GetFinalUnderlyingErrorFromError(error);
    // Only errors with net::kNSErrorDomain have correct net error code.
    if (final_error && [final_error.domain isEqual:net::kNSErrorDomain]) {
      code = final_error.code;
    }
    event.push_back(net::ErrorToShortString(code));
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  std::vector<std::string> event = {kBreadcrumbPageLoaded};

  if (IsNptUrl(web_state->GetLastCommittedURL())) {
    // NTP load can't fail, so there is no need to report success/failure.
    event.push_back(kBreadcrumbNtpNavigation);
  } else {
    if (IsGoogleUrl(web_state->GetLastCommittedURL())) {
      event.push_back(kBreadcrumbGoogleNavigation);
    }

    switch (load_completion_status) {
      case web::PageLoadCompletionStatus::SUCCESS:
        if (web_state->GetContentsMimeType() == "application/pdf") {
          event.push_back(kBreadcrumbPdfLoad);
        }
        break;
      case web::PageLoadCompletionStatus::FAILURE:
        event.push_back(kBreadcrumbPageLoadFailure);
        break;
    }
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::DidChangeVisibleSecurityState(
    web::WebState* web_state) {
  web::NavigationItem* visible_item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!visible_item) {
    return;
  }

  std::vector<std::string> event;
  const web::SSLStatus& ssl = visible_item->GetSSL();
  if (ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
    event.push_back(kBreadcrumbMixedContent);
  }

  if (ssl.security_style == web::SECURITY_STYLE_AUTHENTICATION_BROKEN) {
    event.push_back(kBreadcrumbAuthenticationBroken);
  }

  if (!event.empty()) {
    event.insert(event.begin(), kBreadcrumbDidChangeVisibleSecurityState);
    LogEvent(base::JoinString(event, " "));
  }
}

void BreadcrumbManagerTabHelper::RenderProcessGone(web::WebState* web_state) {
  LogEvent("RenderProcessGone");
}

void BreadcrumbManagerTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);

  [[web_state->GetWebViewProxy() scrollViewProxy]
      removeObserver:scroll_observer_];
  scroll_observer_ = nil;
}

void BreadcrumbManagerTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  sequentially_replaced_infobars_ = 0;

  LogEvent(base::StringPrintf("%s%d", kBreadcrumbInfobarAdded,
                              infobar->delegate()->GetIdentifier()));
}

void BreadcrumbManagerTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                  bool animate) {
  sequentially_replaced_infobars_ = 0;

  std::vector<std::string> event = {
      base::StringPrintf("%s%d", kBreadcrumbInfobarRemoved,
                         infobar->delegate()->GetIdentifier()),
  };

  if (!animate) {
    event.push_back(kBreadcrumbInfobarNotAnimated);
  }

  LogEvent(base::JoinString(event, " "));
}

void BreadcrumbManagerTabHelper::OnInfoBarReplaced(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  sequentially_replaced_infobars_++;

  if (ShouldLogRepeatedEvent(sequentially_replaced_infobars_)) {
    LogEvent(base::StringPrintf("%s%d %d", kBreadcrumbInfobarReplaced,
                                new_infobar->delegate()->GetIdentifier(),
                                sequentially_replaced_infobars_));
  }
}

void BreadcrumbManagerTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK_EQ(infobar_manager_, manager);
  DCHECK(infobar_observation_.IsObservingSource(manager));
  infobar_observation_.Reset();
  infobar_manager_ = nullptr;
  sequentially_replaced_infobars_ = 0;
}

WEB_STATE_USER_DATA_KEY_IMPL(BreadcrumbManagerTabHelper)
