// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_OBSERVER_BRIDGE_H_

#include "base/macros.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"

// Objective-C equivalent of translate::TranslateInfoBarDelegate::Observer.
@protocol TranslateInfobarDelegateObserving <NSObject>

- (void)translateInfoBarDelegate:(translate::TranslateInfoBarDelegate*)delegate
          didChangeTranslateStep:(translate::TranslateStep)step
                   withErrorType:(translate::TranslateErrors::Type)errorType;

- (BOOL)translateInfoBarDelegateDidDismissWithoutInteraction:
    (translate::TranslateInfoBarDelegate*)delegate;

@end

// Bridge class to observe translate::TranslateInfoBarDelegate in Objective-C.
class TranslateInfobarDelegateObserverBridge
    : public translate::TranslateInfoBarDelegate::Observer {
 public:
  // |owner| will not be retained.
  TranslateInfobarDelegateObserverBridge(
      translate::TranslateInfoBarDelegate* translate_infobar_delegate,
      id<TranslateInfobarDelegateObserving> owner);
  ~TranslateInfobarDelegateObserverBridge() override;

  // translate::TranslateInfoBarDelegate::Observer.
  void OnTranslateStepChanged(
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type) override;
  bool IsDeclinedByUser() override;
  void OnTranslateInfoBarDelegateDestroyed(
      translate::TranslateInfoBarDelegate* delegate) override;

 private:
  translate::TranslateInfoBarDelegate* translate_infobar_delegate_ = nullptr;
  __weak id<TranslateInfobarDelegateObserving> owner_ = nil;

  DISALLOW_COPY_AND_ASSIGN(TranslateInfobarDelegateObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_TRANSLATE_INFOBAR_DELEGATE_OBSERVER_BRIDGE_H_
