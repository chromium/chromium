// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_HELPER_OBSERVER_H_

#import "base/observer_list_types.h"

class GeminiTabHelper;

namespace web {
class WebState;
}  // namespace web

// Observer interface for GeminiTabHelper.
class GeminiTabHelperObserver : public base::CheckedObserver {
 public:
  // Called when the page context (URL, title, favicon, etc.) has been updated.
  virtual void OnPageContextUpdated(web::WebState* web_state) {}

  // Called when the GeminiTabHelper is about to be destroyed.
  virtual void OnGeminiTabHelperDestroyed(GeminiTabHelper* tab_helper) {}
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_TAB_HELPER_OBSERVER_H_
