// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_JAVA_SCRIPT_FEATURE_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_JAVA_SCRIPT_FEATURE_OBSERVER_H_

#include "base/observer_list.h"

@class WebSelectionResponse;

namespace web {
class WebState;
}

class WebSelectionJavaScriptFeatureObserver : public base::CheckedObserver {
 public:
  // Called whenever a JavaScript message with a selection is received.
  virtual void OnSelectionRetrieved(web::WebState* web_state,
                                    WebSelectionResponse* response) {}
};

#endif  // IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_JAVA_SCRIPT_FEATURE_OBSERVER_H_
