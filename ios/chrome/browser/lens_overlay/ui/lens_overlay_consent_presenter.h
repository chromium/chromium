// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONSENT_PRESENTER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONSENT_PRESENTER_H_

#import <UIKit/UIKit.h>

@class LensOverlayConsentPresenter;
@class LensOverlayConsentViewController;
@protocol LensOverlayCommands;

/// Delegate to LensOverlayConsentPresenter.
@protocol LensOverlayConsentPresenterDelegate <NSObject>

// Called when the sheet is dismissed.
- (void)requestDismissalOfConsentDialog:(LensOverlayConsentPresenter*)presenter;

// Called before consent view is shown.
- (void)lensOverlayConsentPresenterWillShowConsent:
    (LensOverlayConsentPresenter*)presented;

// Called before consent view is dismissed.
- (void)lensOverlayConsentPresenterWillDismissConsent:
    (LensOverlayConsentPresenter*)presented;

@end

/// Object incapsulating bottom sheet presentation behavior for Lens Overlay
/// consent dialog.
@interface LensOverlayConsentPresenter : NSObject

// Whether the consent dialog is currently presented.
@property(nonatomic, assign, readonly) BOOL isConsentVisible;

- (instancetype)initWithPresentingViewController:(UIViewController*)presentingVC
                  presentedConsentViewController:
                      (LensOverlayConsentViewController*)
                          presentedConsentViewController;

// The consent presenter delegate.
@property(nonatomic, weak) id<LensOverlayConsentPresenterDelegate> delegate;

// Presents the consent VC.
- (void)showConsentViewController;

// Dismisses the consent dialog.
- (void)dismissConsentViewControllerAnimated:(BOOL)animated
                                  completion:(void (^)(void))completion;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_CONSENT_PRESENTER_H_
