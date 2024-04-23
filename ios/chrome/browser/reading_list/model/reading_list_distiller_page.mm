// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_distiller_page.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/google/core/common/google_util.h"
#import "ios/chrome/browser/reading_list/model/favicon_web_state_dispatcher_impl.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "net/cert/cert_status_flags.h"
#import "url/url_constants.h"

namespace {
// The delay given to the web page to render after the PageLoaded callback.
constexpr base::TimeDelta kPageLoadDelay = base::Seconds(2);

// This script retrieve the href parameter of the <link rel="amphtml"> element
// of the page if it exists. If it does not exist, it returns the src of the
// first iframe of the page.
const char16_t* kGetIframeURLJavaScript =
    u"(() => {"
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

const char16_t* kWikipediaWorkaround =
    u"(() => {"
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
  CGRect frame = [GetAnyKeyWindow() frame];
  frame.origin.x = -5 * std::max(frame.size.width, frame.size.height);
  frame.origin.y = frame.origin.x;
  DCHECK(![CurrentWebState()->GetView() superview]);
  [CurrentWebState()->GetView() setFrame:frame];
  [GetAnyKeyWindow() insertSubview:CurrentWebState()->GetView() atIndex:0];
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ReadingListDistillerPage::DelayedOnLoadURLDone, weak_this,
                     delayed_task_id_),
      kPageLoadDelay);
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
    // TODO(crbug.com/40485232): remove workaround once DOM distiller handle
    // this case.
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
  web::WebState* web_state = CurrentWebState();
  if (!web_state) {
    return;
  }
  web::WebFrame* web_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_frame) {
    return;
  }
  web_frame->ExecuteJavaScript(
      kGetIframeURLJavaScript,
      base::BindOnce(
          &ReadingListDistillerPage::OnHandleGoogleCachedAMPPageResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void ReadingListDistillerPage::OnHandleGoogleCachedAMPPageResult(
    const base::Value* value,
    NSError* error) {
  if (!error && value->is_string()) {
    GURL new_gurl(value->GetString());
    if (new_gurl.is_valid()) {
      FetchFavicon(new_gurl);
      web::NavigationManager::WebLoadParams params(new_gurl);
      CurrentWebState()->GetNavigationManager()->LoadURLWithParams(params);

      // If there is no error, the navigation completion will
      // trigger a new `OnLoadURLDone` call that will resume
      // the distillation.
      return;
    }
  }

  // If there is an error on navigation, continue
  // normal distillation.
  ContinuePageDistillation();
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
  web::WebState* web_state = CurrentWebState();
  if (!web_state) {
    return;
  }
  web::WebFrame* web_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  if (!web_frame) {
    return;
  }
  web_frame->ExecuteJavaScript(
      kWikipediaWorkaround,
      BindOnce(&ReadingListDistillerPage::OnHandleWikipediaPageResult,
               weak_ptr_factory_.GetWeakPtr()));
}

void ReadingListDistillerPage::OnHandleWikipediaPageResult(
    const base::Value* value) {
  ContinuePageDistillation();
}

}  // namespace reading_list
