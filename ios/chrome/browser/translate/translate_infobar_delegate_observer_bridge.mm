// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"

#include "base/check_op.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TranslateInfobarDelegateObserverBridge::TranslateInfobarDelegateObserverBridge(
    translate::TranslateInfoBarDelegate* translate_infobar_delegate,
    id<TranslateInfobarDelegateObserving> owner)
    : translate_infobar_delegate_(translate_infobar_delegate), owner_(owner) {
  translate_infobar_delegate_->AddObserver(this);
}

TranslateInfobarDelegateObserverBridge::
    ~TranslateInfobarDelegateObserverBridge() {
  if (translate_infobar_delegate_) {
    translate_infobar_delegate_->RemoveObserver(this);
  }
}

void TranslateInfobarDelegateObserverBridge::OnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
  [owner_ translateInfoBarDelegate:translate_infobar_delegate_
            didChangeTranslateStep:step
                     withErrorType:error_type];
}

void TranslateInfobarDelegateObserverBridge::OnTargetLanguageChanged(
    const std::string& target_language_code) {
  // Unimplemented on iOS as target language changes are initiated solely by the
  // UI. This method should always be a no-op.
  DCHECK_EQ(translate_infobar_delegate_->target_language_code(),
            target_language_code);
}

bool TranslateInfobarDelegateObserverBridge::IsDeclinedByUser() {
  return [owner_ translateInfoBarDelegateDidDismissWithoutInteraction:
                     translate_infobar_delegate_];
}

void TranslateInfobarDelegateObserverBridge::
    OnTranslateInfoBarDelegateDestroyed(
        translate::TranslateInfoBarDelegate* delegate) {
  DCHECK_EQ(translate_infobar_delegate_, delegate);
  translate_infobar_delegate_->RemoveObserver(this);
  translate_infobar_delegate_ = nullptr;
}
