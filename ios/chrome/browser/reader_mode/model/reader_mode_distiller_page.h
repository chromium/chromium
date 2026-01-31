// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DISTILLER_PAGE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DISTILLER_PAGE_H_

#import <string>

#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/values.h"
#import "components/dom_distiller/core/distiller_page.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "url/gurl.h"

// Implementation of page distillation customized for the Reader Mode feature.
class ReaderModeDistillerPage : public dom_distiller::DistillerPage,
                                public web::WebStateObserver {
 public:
  ReaderModeDistillerPage(web::WebState* web_state);
  ~ReaderModeDistillerPage() override;

  // dom_distiller::DistillerPage implementation.
  void DistillPageImpl(const GURL& url, const std::string& script) override;
  bool ShouldFetchOfflineData() override;
  dom_distiller::DistillerType GetDistillerType() override;

 private:
  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  void HandleJavaScriptResult(const GURL& url, const base::Value* result);

  raw_ptr<web::WebState> web_state_;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  base::WeakPtrFactory<ReaderModeDistillerPage> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_DISTILLER_PAGE_H_
