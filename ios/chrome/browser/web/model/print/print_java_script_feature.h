// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_PRINT_PRINT_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_PRINT_PRINT_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#include "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A feature which listens for window.print() commands.
class PrintJavaScriptFeature : public web::JavaScriptFeature {
 private:
  friend class base::NoDestructor<PrintJavaScriptFeature>;
  friend class PrintJavaScriptFeatureTest;

  PrintJavaScriptFeature();
  ~PrintJavaScriptFeature() override;

  PrintJavaScriptFeature(const PrintJavaScriptFeature&) = delete;
  PrintJavaScriptFeature& operator=(const PrintJavaScriptFeature&) = delete;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_PRINT_PRINT_JAVA_SCRIPT_FEATURE_H_
