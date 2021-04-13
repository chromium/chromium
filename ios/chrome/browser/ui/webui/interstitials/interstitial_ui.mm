// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#import <Foundation/Foundation.h>
#include <memory>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "components/grit/dev_ui_components_resources.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_constants.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_util.h"
#import "ios/web/public/security/web_interstitial_delegate.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/webui/url_data_source_ios.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "net/base/url_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Implementation of chrome://interstitials demonstration pages.
class InterstitialHTMLSource : public web::URLDataSourceIOS {
 public:
  explicit InterstitialHTMLSource(ChromeBrowserState* browser_state);
  ~InterstitialHTMLSource() override;
  InterstitialHTMLSource(InterstitialHTMLSource&& other) = default;
  InterstitialHTMLSource& operator=(InterstitialHTMLSource&& other) = default;

 private:
  // web::URLDataSourceIOS:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      web::URLDataSourceIOS::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) const override;

  // The ChromeBrowserState passed on initialization.  Used to construct
  // WebStates that are passed to WebInterstitialDelegates.
  ChromeBrowserState* browser_state_ = nullptr;
};

}  //  namespace

#pragma mark - InterstitialHTMLSource

InterstitialHTMLSource::InterstitialHTMLSource(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

InterstitialHTMLSource::~InterstitialHTMLSource() = default;

std::string InterstitialHTMLSource::GetMimeType(
    const std::string& mime_type) const {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() const {
  return kChromeUIIntersitialsHost;
}

void InterstitialHTMLSource::StartDataRequest(
    const std::string& path,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(web::WebState::CreateParams(browser_state_));
  std::unique_ptr<web::WebInterstitialDelegate> interstitial_delegate;
  std::string html;
  // Using this form of the path so we can do exact matching, while ignoring the
  // query (everything after the ? character).
  GURL url = GURL(kChromeUIIntersitialsURL).GetWithEmptyPath().Resolve(path);
  std::string path_without_query = url.path();
  if (path_without_query == kChromeInterstitialSslPath) {
    interstitial_delegate = CreateSslBlockingPageDelegate(web_state.get(), url);
  } else if (path_without_query == kChromeInterstitialCaptivePortalPath) {
    interstitial_delegate =
        CreateCaptivePortalBlockingPageDelegate(web_state.get());
  } else if (path_without_query == kChromeInterstitialSafeBrowsingPath) {
    interstitial_delegate =
        CreateSafeBrowsingBlockingPageDelegate(web_state.get(), url);
  }
  // TODO(crbug.com/1064805): Update the page HTML when a link for an
  // unsupported interstitial type is tapped.

  // Use the HTML generated from the interstitial delegate if created
  // successfully.  Otherwise, return the default chrome://interstitials HTML.
  if (interstitial_delegate) {
    html = interstitial_delegate->GetHtmlContents();
  } else {
    html = ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_SECURITY_INTERSTITIAL_UI_HTML);
  }

  std::move(callback).Run(base::RefCountedString::TakeString(&html));
}

#pragma mark - InterstitialUI

InterstitialUI::InterstitialUI(web::WebUIIOS* web_ui, const std::string& host)
    : WebUIIOSController(web_ui, host) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  web::URLDataSourceIOS::Add(browser_state,
                             new InterstitialHTMLSource(browser_state));
}

InterstitialUI::~InterstitialUI() = default;
