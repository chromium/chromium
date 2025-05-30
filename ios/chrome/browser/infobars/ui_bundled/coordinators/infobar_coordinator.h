// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_COORDINATORS_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_COORDINATORS_INFOBAR_COORDINATOR_H_

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/infobars/ui_bundled/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol InfobarBannerContained;
class ProfileIOS;

enum class InfobarBannerPresentationState;

// Must be subclassed. Defines common behavior for all Infobars.
@interface InfobarCoordinator
    : ChromeCoordinator <InfobarBannerDelegate, InfobarModalDelegate>

// Designated Initializer. `infobarType` is the unique identifier for each
// Infobar, there can't be more than one infobar with the same type added to the
// InfobarManager.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Present the InfobarBanner using `self.baseViewController`.
- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion;

// Dismisses the InfobarBanner immediately, if none is being presented
// `completion` will still run.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion;

// Stops this Coordinator.
- (void)stop NS_REQUIRES_SUPER;

// The InfobarType for this Infobar.
@property(nonatomic, readonly) InfobarType infobarType;

// YES if the Coordinator has been started.
@property(nonatomic, assign) BOOL started;

// BannerViewController owned by this Coordinator. Can be nil.
@property(nonatomic, strong, readonly)
    UIViewController<InfobarBannerContained>* bannerViewController;

// ModalViewController owned by this Coordinator. Can be nil.
@property(nonatomic, strong, readonly) UIViewController* modalViewController;

// The InfobarBanner presentation state.
@property(nonatomic, assign) InfobarBannerPresentationState infobarBannerState;

// YES if the banner has ever been presented for this Coordinator.
@property(nonatomic, assign, readonly) BOOL bannerWasPresented;

// If YES this Coordinator's banner will have a higher presentation priority
// than other InfobarCoordinators with this property set to NO. The parent
// Coordinator will define what this means e.g. Longer presentation time before
// auto-dismiss and/or jumping the queue and being the next banner to present,
// etc.
@property(nonatomic, assign) BOOL highPriorityPresentation;

// If YES, then InfobarCoordinator will handle dismissing any infobar shown.
// If NO, then the coordinator should handle dismissal itself. Defaults to YES.
@property(nonatomic, assign) BOOL shouldUseDefaultDismissal;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_UI_BUNDLED_COORDINATORS_INFOBAR_COORDINATOR_H_
