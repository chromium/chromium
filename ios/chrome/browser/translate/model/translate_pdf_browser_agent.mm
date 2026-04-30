// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_pdf_browser_agent.h"

#import "components/translate/core/browser/translate_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_pdf_metric_logger.h"
#import "ios/web/public/web_state.h"

TranslatePDFBrowserAgent::TranslatePDFBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  StartObserving(browser_);
}

TranslatePDFBrowserAgent::~TranslatePDFBrowserAgent() {
  StopObserving();
}

#pragma mark - TranslatePDFDelegate

bool TranslatePDFBrowserAgent::IsOpenerTabTranslatedForWebState(
    web::WebState* web_state) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  int index = web_state_list->GetIndexOfWebState(web_state);
  if (index == WebStateList::kInvalidIndex) {
    return false;
  }

  WebStateOpener opener = web_state_list->GetOpenerOfWebStateAt(index);
  if (!opener.opener) {
    return false;
  }

  ChromeIOSTranslateClient* client =
      ChromeIOSTranslateClient::FromWebState(opener.opener);
  if (!client) {
    return false;
  }

  return client->GetTranslateManager()->GetLanguageState()->IsPageTranslated();
}

#pragma mark - TabsDependencyInstaller

void TranslatePDFBrowserAgent::OnWebStateInserted(web::WebState* web_state) {
  TranslatePDFMetricLogger* logger =
      TranslatePDFMetricLogger::FromWebState(web_state);
  if (logger) {
    logger->SetDelegate(this);
  }
}

void TranslatePDFBrowserAgent::OnWebStateRemoved(web::WebState* web_state) {
  TranslatePDFMetricLogger* logger =
      TranslatePDFMetricLogger::FromWebState(web_state);
  if (logger) {
    logger->SetDelegate(nullptr);
  }
}

void TranslatePDFBrowserAgent::OnWebStateDeleted(web::WebState* web_state) {}

void TranslatePDFBrowserAgent::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {}
