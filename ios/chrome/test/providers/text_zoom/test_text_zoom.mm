// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/font_size/font_size_java_script_feature.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"

namespace ios {
namespace provider {

void SetTextZoomForWebState(web::WebState* web_state, int size) {
  FontSizeJavaScriptFeature::GetInstance()->AdjustFontSize(web_state, size);
}

bool IsTextZoomEnabled() {
  return true;
}

}  // namespace provider
}  // namespace ios
