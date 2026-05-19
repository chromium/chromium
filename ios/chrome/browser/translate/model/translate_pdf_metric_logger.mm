// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/model/translate_pdf_metric_logger.h"

#import "base/metrics/histogram_functions.h"
#import "components/translate/core/browser/translate_manager.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_pdf_delegate.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

namespace {
const char kPDFMimeType[] = "application/pdf";
}

TranslatePDFMetricLogger::TranslatePDFMetricLogger(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

TranslatePDFMetricLogger::~TranslatePDFMetricLogger() = default;

void TranslatePDFMetricLogger::SetDelegate(TranslatePDFDelegate* delegate) {
  delegate_ = delegate;
}

#pragma mark - WebStateObserver

void TranslatePDFMetricLogger::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument()) {
    return;
  }

  bool is_translated = false;
  ChromeIOSTranslateClient* client =
      ChromeIOSTranslateClient::FromWebState(web_state_);
  if (client) {
    is_translated =
        client->GetTranslateManager()->GetLanguageState()->IsPageTranslated();
  }

  was_translated_at_navigation_start_ = is_translated;
}

void TranslatePDFMetricLogger::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument()) {
    bool is_first_navigation = is_first_navigation_;
    is_first_navigation_ = false;

    if (web_state_->GetContentsMimeType() != kPDFMimeType) {
      return;
    }
    bool was_source_translated = false;
    if (was_translated_at_navigation_start_) {
      was_source_translated = true;
    } else if (delegate_) {
      ui::PageTransition transition = navigation_context->GetPageTransition();
      bool is_reload =
          ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD);
      bool is_back_forward =
          (transition & ui::PAGE_TRANSITION_FORWARD_BACK) != 0;
      if (is_first_navigation && !is_reload && !is_back_forward) {
        was_source_translated =
            delegate_->IsOpenerTabTranslatedForWebState(web_state_);
      }
    }
    base::UmaHistogramBoolean("Translate.IOS.PDF.OpenedFromTranslatedPage",
                              was_source_translated);
  }
  was_translated_at_navigation_start_ = false;
}

void TranslatePDFMetricLogger::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
  web_state_ = nullptr;
  delegate_ = nullptr;
}
