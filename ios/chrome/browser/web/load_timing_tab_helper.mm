// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/load_timing_tab_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#import "ios/web/public/web_state.h"

const char LoadTimingTabHelper::kOmnibarToPageLoadedMetric[] =
    "IOS.PageLoadTiming.OmnibarToPageLoaded";

LoadTimingTabHelper::LoadTimingTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

LoadTimingTabHelper::~LoadTimingTabHelper() {
  DCHECK(!web_state_);
}

void LoadTimingTabHelper::DidInitiatePageLoad() {
  load_start_time_ = base::TimeTicks::Now();
}

void LoadTimingTabHelper::DidPromotePrerenderTab() {
  if (web_state_->IsLoading()) {
    DidInitiatePageLoad();
  } else {
    ReportLoadTime(base::TimeDelta());
    ResetTimer();
  }
}

void LoadTimingTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (!load_start_time_.is_null() &&
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    ReportLoadTime(base::TimeTicks::Now() - load_start_time_);
  }
  ResetTimer();
}

void LoadTimingTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void LoadTimingTabHelper::ReportLoadTime(const base::TimeDelta& elapsed) {
  UMA_HISTOGRAM_TIMES(kOmnibarToPageLoadedMetric, elapsed);
}

void LoadTimingTabHelper::ResetTimer() {
  load_start_time_ = base::TimeTicks();
}

WEB_STATE_USER_DATA_KEY_IMPL(LoadTimingTabHelper)
