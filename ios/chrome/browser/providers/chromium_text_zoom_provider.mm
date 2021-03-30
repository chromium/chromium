// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_text_zoom_provider.h"

#include "ios/chrome/browser/web/features.h"
#import "ios/public/provider/chrome/browser/font_size_java_script_feature.h"
#include "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumTextZoomProvider::ChromiumTextZoomProvider() = default;

ChromiumTextZoomProvider::~ChromiumTextZoomProvider() = default;

void ChromiumTextZoomProvider::SetPageFontSize(web::WebState* web_state,
                                               int size) {
  FontSizeJavaScriptFeature::GetInstance()->AdjustFontSize(web_state, size);
}

bool ChromiumTextZoomProvider::IsTextZoomEnabled() {
  return ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET &&
         base::FeatureList::IsEnabled(web::kWebPageTextAccessibility);
}
