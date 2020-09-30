// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/text_zoom_provider.h"

#import "base/values.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TextZoomProvider::TextZoomProvider() = default;

TextZoomProvider::~TextZoomProvider() = default;

void TextZoomProvider::SetPageFontSize(web::WebState* web_state, int size) {
  SetPageFontSizeJavascript(web_state, size);
}

void TextZoomProvider::SetPageFontSizeJavascript(web::WebState* web_state,
                                                 int size) {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(size));
  for (web::WebFrame* frame :
       web_state->GetWebFramesManager()->GetAllWebFrames()) {
    frame->CallJavaScriptFunction("accessibility.adjustFontSize", parameters);
  }
}

bool TextZoomProvider::IsTextZoomEnabled() {
  return false;
}
