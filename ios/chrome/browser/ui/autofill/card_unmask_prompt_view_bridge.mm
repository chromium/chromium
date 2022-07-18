// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"

#import "base/notreached.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

#pragma mark CardUnmaskPromptViewBridge

CardUnmaskPromptViewBridge::~CardUnmaskPromptViewBridge() {
  NOTIMPLEMENTED();
}

void CardUnmaskPromptViewBridge::Show() {
  NOTIMPLEMENTED();
}

void CardUnmaskPromptViewBridge::ControllerGone() {
  NOTIMPLEMENTED();
}

void CardUnmaskPromptViewBridge::DisableAndWaitForVerification() {
  NOTIMPLEMENTED();
}

void CardUnmaskPromptViewBridge::GotVerificationResult(
    const std::u16string& error_message,
    bool allow_retry) {
  NOTIMPLEMENTED();
}

}  // namespace autofill
