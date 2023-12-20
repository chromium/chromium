// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_FONT_SIZE_FONT_SIZE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_FONT_SIZE_FONT_SIZE_JAVA_SCRIPT_FEATURE_H_

#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

// Feature which adjusts the font size on a page.
class FontSizeJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static FontSizeJavaScriptFeature* GetInstance();

  // Adjusts the font size in all frames of `web_state` by `size` percentage.
  void AdjustFontSize(web::WebState* web_state, int size);

  // Adjusts the font size in `web_frame` by `size` percentage.
  void AdjustFontSize(web::WebFrame* web_frame, int size);

 private:
  friend class base::NoDestructor<FontSizeJavaScriptFeature>;

  FontSizeJavaScriptFeature();
  ~FontSizeJavaScriptFeature() override;

  FontSizeJavaScriptFeature(const FontSizeJavaScriptFeature&) = delete;
  FontSizeJavaScriptFeature& operator=(const FontSizeJavaScriptFeature&) =
      delete;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_FONT_SIZE_FONT_SIZE_JAVA_SCRIPT_FEATURE_H_
