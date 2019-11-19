// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_distiller_page.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/reading_list/favicon_web_state_dispatcher_impl.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/cert_status_flags.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The delay given to the web page to render after the PageLoaded callback.
const int64_t kPageLoadDelayInSeconds = 2;

// This script retrieve the href parameter of the <link rel="amphtml"> element
// of the page if it exists. If it does not exist, it returns the src of the
// first iframe of the page.
const char* kGetIframeURLJavaScript =
    "(() => {"
    "  var link = document.evaluate('//link[@rel=\"amphtml\"]',"
    "                               document,"
    "                               null,"
    "                               XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,"
    "                               null ).snapshotItem(0);"
    "  if (link !== null) {"
    "    return link.getAttribute('href');"
    "  }"
    "  return document.getElementsByTagName('iframe')[0].src;"
    "})()";

const char* kWikipediaWorkaround =
    "(() => {"
    "  var s = document.createElement('style');"
    "  s.innerHTML='.client-js .collapsible-block { display: block }';"
    "  document.head.appendChild(s);"
    "})()";
}  // namespace

namespace reading_list {

ReadingListDistillerPageDelegate::ReadingListDistillerPageDelegate() {}
ReadingListDistillerPageDelegate::~ReadingListDistillerPageDelegate() {}

ReadingListDistillerPage::ReadingListDistillerPage(
    const GURL& url,
    web::BrowserState* browser_state,
    FaviconWebStateDispatcher* web_state_dispatcher,
    ReadingListDistillerPageDelegate* delegate)
    : dom_distiller::DistillerPageIOS(browser_state),
      original_url_(url),
      web_state_dispatcher_(web_state_dispatcher),
      delegate_(delegate),
      delayed_task_id_(0),
      weak_ptr_factory_(this) {
  DCHECK(delegate);
}

ReadingListDistillerPage::~ReadingListDistillerPage() {}

void ReadingListDistillerPage::DistillPageImpl(const GURL& url,
                                               const std::string& script) {
  std::unique_ptr<web::WebState> old_web_state = DetachWebState();
  if (old_web_state) {
    web_state_dispatcher_->ReturnWebState(std::move(old_web_state));
  }
  std::unique_ptr<web::WebState> new_web_state =
      web_state_dispatcher_->RequestWebState();
  AttachWebState(std::move(new_web_state));

  delayed_task_id_++;
  distilling_main_page_ = url == original_url_;
  FetchFavicon(url);

  DistillerPageIOS::DistillPageImpl(url, script);

  // WKWebView sets the document.hidden property to true and the
  // document.visibilityState to prerender if the page is not added to a view
  // hierarchy. Some pages may not render their content in these conditions.
  // Add the view and move it out of the screen far in the top left corner of
  // the coordinate space.
  CGRect frame = [[[UIApplication sharedApplication] keyWindow] frame];
  frame.origin.x = -5 * std::max(frame.size.width, frame.size.height);
  frame.origin.y = frame.origin.x;
  DCHECK(![CurrentWebState()->GetView() superview]);
  [CurrentWebState()->GetView() setFrame:frame];
  [[[UIApplication sharedApplication] keyWindow]
      insertSubview:CurrentWebState()->GetView()
            atIndex:0];
}

void ReadingListDistillerPage::FetchFavicon(const GURL& page_url) {
  if (!CurrentWebState() || !page_url.is_valid()) {
    return;
  }
  favicon::WebFaviconDriver* favicon_driver =
      favicon::WebFaviconDriver::FromWebState(CurrentWebState());
  DCHECK(favicon_driver);
  favicon_driver->FetchFavicon(page_url, /*is_same_document=*/false);
}

void ReadingListDistillerPage::OnDistillationDone(const GURL& page_url,
                                                  const base::Value* value) {
  std::unique_ptr<web::WebState> old_web_state = DetachWebState();
  if (old_web_state) {
    [old_web_state->GetView() removeFromSuperview];
    web_state_dispatcher_->ReturnWebState(std::move(old_web_state));
  }
  delayed_task_id_++;
  DistillerPageIOS::OnDistillationDone(page_url, value);
}

bool ReadingListDistillerPage::IsLoadingSuccess(
    web::PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status != web::PageLoadCompletionStatus::SUCCESS) {
    return false;
  }
  if (!CurrentWebState() || !CurrentWebState()->GetNavigationManager() ||
      !CurrentWebState()->GetNavigationManager()->GetLastCommittedItem()) {
    // Only distill fully loaded, committed pages. If the page was not fully
    // loaded, web::PageLoadCompletionStatus::FAILURE should have been passed to
    // OnLoadURLDone. But check that the item exist before using it anyway.
    return false;
  }
  web::NavigationItem* item =
      CurrentWebState()->GetNavigationManager()->GetLastCommittedItem();
  if (!item->GetURL().SchemeIsCryptographic()) {
    // HTTP is allowed.
    return true;
  }

  // On SSL connections, check there was no error.
  const web::SSLStatus& ssl_status = item->GetSSL();
  if (net::IsCertStatusError(ssl_status.cert_status)) {
    return false;
  }
  return true;
}

void ReadingListDistillerPage::OnLoadURLDone(
    web::PageLoadCompletionStatus load_completion_status) {
  if (!IsLoadingSuccess(load_completion_status)) {
    DistillerPageIOS::OnLoadURLDone(load_completion_status);
    return;
  }
  if (distilling_main_page_) {
    delegate_->DistilledPageHasMimeType(
        original_url_, CurrentWebState()->GetContentsMimeType());
  }
  if (!CurrentWebState()->ContentIsHTML()) {
    // If content is not HTML, distillation will fail immediately.
    // Call the handler to make sure cleaning methods are called correctly.
    // There is no need to wait for rendering either.
    DistillerPageIOS::OnLoadURLDone(load_completion_status);
    return;
  }
  FetchFavicon(CurrentWebState()->GetVisibleURL());

  // Page is loaded but rendering may not be done yet. Give a delay to the page.
  base::WeakPtr<ReadingListDistillerPage> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ReadingListDistillerPage::DelayedOnLoadURLDone, weak_this,
                     delayed_task_id_),
      base::TimeDelta::FromSeconds(kPageLoadDelayInSeconds));
}

void ReadingListDistillerPage::DelayedOnLoadURLDone(int delayed_task_id) {
  if (!CurrentWebState() || delayed_task_id != delayed_task_id_) {
    // Something interrupted the distillation.
    // Abort here.
    return;
  }
  if (IsGoogleCachedAMPPage()) {
    // Workaround for Google AMP pages.
    HandleGoogleCachedAMPPage();
    return;
  }
  if (IsWikipediaPage()) {
    // Workaround for Wikipedia pages.
    // TODO(crbug.com/647667): remove workaround once DOM distiller handle this
    // case.
    HandleWikipediaPage();
    return;
  }
  ContinuePageDistillation();
}

void ReadingListDistillerPage::ContinuePageDistillation() {
  if (!CurrentWebState()) {
    // Something interrupted the distillation.
    // Abort here.
    return;
  }
  // The page is ready to be distilled.
  // If the visible URL is not the original URL, notify the caller that URL
  // changed.
  GURL redirected_url = CurrentWebState()->GetVisibleURL();
  if (redirected_url != original_url_ && delegate_ && distilling_main_page_) {
    delegate_->DistilledPageRedirectedToURL(original_url_, redirected_url);
  }
  DistillerPageIOS::OnLoadURLDone(web::PageLoadCompletionStatus::SUCCESS);
}

bool ReadingListDistillerPage::IsGoogleCachedAMPPage() {
  // All google AMP pages have URL in the form "https://google_domain/amp/..."
  // and a valid certificate.
  // This method checks that this is strictly the case.
  const GURL& url = CurrentWebState()->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  if (!google_util::IsGoogleDomainUrl(
          url, google_util::DISALLOW_SUBDOMAIN,
          google_util::DISALLOW_NON_STANDARD_PORTS) ||
      !url.path().compare(0, 4, "amp/")) {
    return false;
  }
  const web::SSLStatus& ssl_status = CurrentWebState()
                                         ->GetNavigationManager()
                                         ->GetLastCommittedItem()
                                         ->GetSSL();
  if (!ssl_status.certificate ||
      net::IsCertStatusError(ssl_status.cert_status)) {
    return false;
  }

  return true;
}

void ReadingListDistillerPage::HandleGoogleCachedAMPPage() {
  base::WeakPtr<ReadingListDistillerPage> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [CurrentWebState()->GetJSInjectionReceiver()
      executeJavaScript:@(kGetIframeURLJavaScript)
      completionHandler:^(id result, NSError* error) {
        if (weak_this &&
            !weak_this->HandleGoogleCachedAMPPageJavaScriptResult(result,
                                                                  error)) {
          // If there is an error on navigation, continue normal distillation.
          weak_this->ContinuePageDistillation();
        }
        // If there is no error, the navigation completion will trigger a new
        // |OnLoadURLDone| call that will resume the distillation.
      }];
}

bool ReadingListDistillerPage::HandleGoogleCachedAMPPageJavaScriptResult(
    id result,
    id error) {
  if (error) {
    return false;
  }
  NSString* result_string = base::mac::ObjCCast<NSString>(result);
  NSURL* new_url = [NSURL URLWithString:result_string];
  if (!new_url) {
    return false;
  }

  GURL new_gurl = net::GURLWithNSURL(new_url);
  if (!new_gurl.is_valid()) {
    return false;
  }
  FetchFavicon(new_gurl);
  web::NavigationManager::WebLoadParams params(new_gurl);
  CurrentWebState()->GetNavigationManager()->LoadURLWithParams(params);
  return true;
}

bool ReadingListDistillerPage::IsWikipediaPage() {
  // All wikipedia pages are in the form "https://xxx.m.wikipedia.org/..."
  const GURL& url = CurrentWebState()->GetLastCommittedURL();
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  return (base::EndsWith(url.host(), ".m.wikipedia.org",
                         base::CompareCase::SENSITIVE));
}

void ReadingListDistillerPage::HandleWikipediaPage() {
  base::WeakPtr<ReadingListDistillerPage> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  [CurrentWebState()->GetJSInjectionReceiver()
      executeJavaScript:@(kWikipediaWorkaround)
      completionHandler:^(id result, NSError* error) {
        if (weak_this) {
          weak_this->ContinuePageDistillation();
        }
      }];
}

}  // namespace reading_list
