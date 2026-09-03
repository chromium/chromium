// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_EDUCATION_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_EDUCATION_JAVA_SCRIPT_FEATURE_H_

#import <optional>

#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "base/time/time.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
}  // namespace web

// Holds extracted DOM structural metrics for the Education vertical.
struct EducationDOMFeatures {
  int word_count = 0;
  int heading_count = 0;

  bool operator==(const EducationDOMFeatures& other) const = default;
};

// JavaScriptFeature that injects education_page_detector.ts to extract
// structural DOM features (word count, headings) for Education vertical.
class EducationJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // Returns the shared singleton instance.
  static EducationJavaScriptFeature* GetInstance();

  // Triggers DOM feature extraction on `frame` with the specified `timeout`.
  // Calls `callback` with extracted features, or std::nullopt on error/timeout.
  void ExtractDOMFeatures(
      web::WebFrame* frame,
      base::TimeDelta timeout,
      base::OnceCallback<void(std::optional<EducationDOMFeatures>)> callback);

 private:
  friend class base::NoDestructor<EducationJavaScriptFeature>;

  EducationJavaScriptFeature();
  ~EducationJavaScriptFeature() override;

  EducationJavaScriptFeature(const EducationJavaScriptFeature&) = delete;
  EducationJavaScriptFeature& operator=(const EducationJavaScriptFeature&) =
      delete;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_EDUCATION_JAVA_SCRIPT_FEATURE_H_
