// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/text_zoom_provider.h"

#import "ios/public/provider/chrome/browser/font_size_java_script_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TextZoomProvider::TextZoomProvider() = default;

TextZoomProvider::~TextZoomProvider() = default;

void TextZoomProvider::SetPageFontSize(web::WebState* web_state, int size) {
  FontSizeJavaScriptFeature::GetInstance()->AdjustFontSize(web_state, size);
}

bool TextZoomProvider::IsTextZoomEnabled() {
  return false;
}
