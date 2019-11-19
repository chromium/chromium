// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/translate/translate_infobar_delegate_observer_bridge.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TranslateInfobarDelegateObserverBridge::TranslateInfobarDelegateObserverBridge(
    translate::TranslateInfoBarDelegate* translate_infobar_delegate,
    id<TranslateInfobarDelegateObserving> owner)
    : translate_infobar_delegate_(translate_infobar_delegate), owner_(owner) {
  translate_infobar_delegate_->SetObserver(this);
}

TranslateInfobarDelegateObserverBridge::
    ~TranslateInfobarDelegateObserverBridge() {
  if (translate_infobar_delegate_) {
    translate_infobar_delegate_->SetObserver(nullptr);
  }
}

void TranslateInfobarDelegateObserverBridge::OnTranslateStepChanged(
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type) {
  [owner_ translateInfoBarDelegate:translate_infobar_delegate_
            didChangeTranslateStep:step
                     withErrorType:error_type];
}

bool TranslateInfobarDelegateObserverBridge::IsDeclinedByUser() {
  return [owner_ translateInfoBarDelegateDidDismissWithoutInteraction:
                     translate_infobar_delegate_];
}

void TranslateInfobarDelegateObserverBridge::
    OnTranslateInfoBarDelegateDestroyed(
        translate::TranslateInfoBarDelegate* delegate) {
  DCHECK_EQ(translate_infobar_delegate_, delegate);
  translate_infobar_delegate_->SetObserver(nullptr);
  translate_infobar_delegate_ = nullptr;
}
