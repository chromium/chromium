// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/dwa_web_state_observer.h"

#import "base/check_op.h"
#import "components/metrics/dwa/dwa_recorder.h"
#import "ios/web/public/web_state.h"

DwaWebStateObserver::DwaWebStateObserver(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

DwaWebStateObserver::~DwaWebStateObserver() {
  DCHECK(!web_state_);
}

void DwaWebStateObserver::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK_EQ(web_state_, web_state);
  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    metrics::dwa::DwaRecorder::Get()->OnPageLoad();
  }
}

void DwaWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}
