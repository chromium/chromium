// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptPromptOverlayRequestConfig);

JavaScriptPromptOverlayRequestConfig::JavaScriptPromptOverlayRequestConfig(
    const JavaScriptDialogSource& source,
    const std::string& message,
    const std::string& default_prompt_value)
    : source_(source),
      message_(message),
      default_prompt_value_(default_prompt_value) {}

JavaScriptPromptOverlayRequestConfig::~JavaScriptPromptOverlayRequestConfig() =
    default;

OVERLAY_USER_DATA_SETUP_IMPL(JavaScriptPromptOverlayResponseInfo);

JavaScriptPromptOverlayResponseInfo::JavaScriptPromptOverlayResponseInfo(
    const std::string& text_input)
    : text_input_(text_input) {}

JavaScriptPromptOverlayResponseInfo::~JavaScriptPromptOverlayResponseInfo() =
    default;
