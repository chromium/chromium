// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/web_content_area/spinning_overlay_request_config.h"

SpinningOverlayRequestConfig::SpinningOverlayRequestConfig(NSString* label_text,
                                                           bool is_cancellable)
    : label_text_([label_text copy]), is_cancellable_(is_cancellable) {}

SpinningOverlayRequestConfig::~SpinningOverlayRequestConfig() = default;

SpinningOverlayResponse::SpinningOverlayResponse(bool canceled)
    : canceled_(canceled) {}

SpinningOverlayResponse::~SpinningOverlayResponse() = default;
