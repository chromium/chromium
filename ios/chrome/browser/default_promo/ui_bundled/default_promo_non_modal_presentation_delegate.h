// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_PROMO_NON_MODAL_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_PROMO_NON_MODAL_PRESENTATION_DELEGATE_H_

// Delegate used for presenting and dismissing the non-modal default browser
// promo.
@protocol DefaultPromoNonModalPresentationDelegate

// Whether the default promo is currently showing.
- (BOOL)defaultNonModalPromoIsShowing;

// Asks the delegate to dismiss the promo, `animated`, and call `completion`
// when the dismissal is done.
- (void)dismissDefaultNonModalPromoAnimated:(BOOL)animated
                                 completion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_PROMO_NON_MODAL_PRESENTATION_DELEGATE_H_
