// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

@protocol ApplicationCommands;
@protocol InfobarBadgeUIDelegate;
@protocol InfobarContainer;

@class InfobarBannerTransitionDriver;
@class InfobarBannerViewController;
@class InfobarModalTransitionDriver;
@class InfobarModalViewController;

namespace ios {
class ChromeBrowserState;
}
namespace infobars {
class InfoBarDelegate;
}

namespace web {
class WebState;
}  // namespace web

enum class InfobarBannerPresentationState;

// Must be subclassed. Defines common behavior for all Infobars.
@interface InfobarCoordinator : ChromeCoordinator <InfobarUIDelegate,
                                                   InfobarBannerDelegate,
                                                   InfobarModalDelegate>

// Designated Initializer. |infoBarDelegate| is used to configure the Infobar
// and subsequently perform related actions. |badgeSupport| should be YES if the
// Infobar will add a Badge and support a Modal. |infobarType| is the unique
// identifier for each Infobar, there can't be more than one infobar with the
// same type added to the InfobarManager.
- (instancetype)initWithInfoBarDelegate:
                    (infobars::InfoBarDelegate*)infoBarDelegate
                           badgeSupport:(BOOL)badgeSupport
                                   type:(InfobarType)infobarType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Present the InfobarBanner using |self.baseViewController|.
- (void)presentInfobarBannerAnimated:(BOOL)animated
                          completion:(ProceduralBlock)completion;

// Present the InfobarModal using |self.baseViewController|.
- (void)presentInfobarModal;

// Dismisses the InfobarBanner immediately, if none is being presented
// |completion| will still run.
- (void)dismissInfobarBannerAnimated:(BOOL)animated
                          completion:(void (^)())completion;

// Stops this Coordinator.
- (void)stop NS_REQUIRES_SUPER;

// YES if the Coordinator has been started.
@property(nonatomic, assign) BOOL started;

// BannerViewController owned by this Coordinator. Can be nil.
@property(nonatomic, strong, readonly) UIViewController* bannerViewController;

// ModalViewController owned by this Coordinator. Can be nil.
@property(nonatomic, strong, readonly) UIViewController* modalViewController;

// Handles any followup actions to Infobar UI events. Should be nil if the
// Coordinator doesn't support a badge.
@property(nonatomic, weak) id<InfobarBadgeUIDelegate> badgeDelegate;

// The ChromeBrowserState owned by the Coordinator.
// TODO(crbug.com/927064): Once we create the coordinators in the UI Hierarchy
// browserState will be set on init.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The WebState that the InfobarCoordinator is associated with. Can be nil.
@property(nonatomic, assign) web::WebState* webState;

// The ChromeBrowserState owned by the Coordinator.
// TODO(crbug.com/927064): Once we create the coordinators in the UI Hierarchy
// baseViewController will be set on init.
@property(nonatomic, weak) UIViewController* baseViewController;

// The dispatcher for this Coordinator.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;

// The InfobarContainer for this InfobarCoordinator.
@property(nonatomic, weak) id<InfobarContainer> infobarContainer;

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

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATOR_H_
