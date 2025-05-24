// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_JAVA_SCRIPT_FEATURE_H_

#include <optional>
#include <vector>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

// A feature that extracts DOM attributes to use for web page distillation.
class ReaderModeJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // This feature holds no state, so only a single static instance is ever
  // needed.
  static ReaderModeJavaScriptFeature* GetInstance();

  // Triggers the heuristic to determine whether a web frame is eligible for
  // distillation.
  void TriggerReaderModeHeuristic(web::WebFrame* web_frame);

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

 private:
  friend class base::NoDestructor<ReaderModeJavaScriptFeature>;

  ReaderModeJavaScriptFeature();
  ~ReaderModeJavaScriptFeature() override;

  ReaderModeJavaScriptFeature(const ReaderModeJavaScriptFeature&) = delete;
  ReaderModeJavaScriptFeature& operator=(const ReaderModeJavaScriptFeature&) =
      delete;

  // Converts the JavaScript message `body` and `request_url` into a vector
  // that can be used to determine
  // whether the web page is distillable. Returns `std::nullopt` if any of the
  // parameters cannot be extracted.
  std::optional<std::vector<double>> TransformToDerivedFeatures(
      const base::Value::Dict& body,
      const GURL& request_url);
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_JAVA_SCRIPT_FEATURE_H_
