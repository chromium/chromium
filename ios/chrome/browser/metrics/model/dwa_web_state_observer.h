// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_METRICS_MODEL_DWA_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_METRICS_MODEL_DWA_WEB_STATE_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

class DwaWebStateObserver : public web::WebStateObserver,
                            public web::WebStateUserData<DwaWebStateObserver> {
 public:
  explicit DwaWebStateObserver(web::WebState* web_state);

  DwaWebStateObserver(const DwaWebStateObserver&) = delete;
  DwaWebStateObserver& operator=(const DwaWebStateObserver&) = delete;

  ~DwaWebStateObserver() override;

  // web::WebStateObserver overrides:
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<DwaWebStateObserver>;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_METRICS_MODEL_DWA_WEB_STATE_OBSERVER_H_
