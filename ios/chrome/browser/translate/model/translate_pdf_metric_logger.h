// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_METRIC_LOGGER_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_METRIC_LOGGER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class TranslatePDFDelegate;

// A tab helper that logs translation-related metrics for PDF documents It
// specifically tracks the relationship between the PDF load and the translation
// state of the referring page.
class TranslatePDFMetricLogger
    : public web::WebStateObserver,
      public web::WebStateUserData<TranslatePDFMetricLogger> {
 public:
  TranslatePDFMetricLogger(const TranslatePDFMetricLogger&) = delete;
  TranslatePDFMetricLogger& operator=(const TranslatePDFMetricLogger&) = delete;

  ~TranslatePDFMetricLogger() override;

  // Sets the delegate to handle browser-level translation queries.
  void SetDelegate(TranslatePDFDelegate* delegate);

 private:
  friend class web::WebStateUserData<TranslatePDFMetricLogger>;

  explicit TranslatePDFMetricLogger(web::WebState* web_state);

  // web::WebStateObserver implementation
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  raw_ptr<web::WebState> web_state_ = nullptr;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  raw_ptr<TranslatePDFDelegate> delegate_ = nullptr;
  bool was_translated_at_navigation_start_ = false;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_PDF_METRIC_LOGGER_H_
