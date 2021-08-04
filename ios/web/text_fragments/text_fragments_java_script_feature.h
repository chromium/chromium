// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_JAVA_SCRIPT_FEATURE_H_

#include <string>

#include "base/no_destructor.h"
#include "base/values.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {

/**
 * Handles JS communication for the Text Fragments feature.
 */
class TextFragmentsJavaScriptFeature : public JavaScriptFeature {
 public:
  static TextFragmentsJavaScriptFeature* GetInstance();

  // For a given WebState, invokes the JS-side handlers needed to highlight the
  // text fragments described in |parsed_fragments|. Will use the colors in
  // the |*_color_hex_rgb| args to style the highlight in the page, or a default
  // coloring if empty strings are passed.
  virtual void ProcessTextFragments(WebState* web_state,
                                    base::Value parsed_fragments,
                                    std::string background_color_hex_rgb,
                                    std::string foreground_color_hex_rgb);

 protected:
  void ScriptMessageReceived(WebState* web_state,
                             const ScriptMessage& script_message) override;
  TextFragmentsJavaScriptFeature();
  ~TextFragmentsJavaScriptFeature() override;

 private:
  friend class base::NoDestructor<TextFragmentsJavaScriptFeature>;

  TextFragmentsJavaScriptFeature(const TextFragmentsJavaScriptFeature&) =
      delete;
  TextFragmentsJavaScriptFeature& operator=(
      const TextFragmentsJavaScriptFeature&) = delete;
};

}  // namespace web

#endif  // IOS_WEB_TEXT_FRAGMENTS_TEXT_FRAGMENTS_JAVA_SCRIPT_FEATURE_H_
