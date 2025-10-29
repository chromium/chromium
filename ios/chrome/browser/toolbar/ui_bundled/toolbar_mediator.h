// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_type.h"

namespace segmentation_platform {
class DeviceSwitcherResultDispatcher;
}  // namespace segmentation_platform

@protocol ToolbarOmniboxConsumer;
class WebStateList;

/// Delegate for events in `ToolbarMediator`.
@protocol ToolbarMediatorDelegate <NSObject>

/// Updates toolbar appearance.
- (void)updateToolbar;

/// Transitions the omnibox position to the toolbar of type `toolbarType`.
- (void)transitionOmniboxToToolbarType:(ToolbarType)toolbarType;

/// Transitions the steady state omnibox position to the toolbar of type
/// `toolbarType`. The steady state omnibox is when the omnibox is not focused.
- (void)transitionSteadyStateOmniboxToToolbarType:(ToolbarType)toolbarType;

/// The height of the bottom omnibox when it is keyboard attached.
- (CGFloat)keyboardAttachedBottomOmniboxHeight;

@end

@interface ToolbarMediator : NSObject

/// Delegate for events in `ToolbarMediator`.
@property(nonatomic, weak) id<ToolbarMediatorDelegate> delegate;
/// The omnibox consumer for this object.
@property(nonatomic, weak) id<ToolbarOmniboxConsumer> omniboxConsumer;

@property(nonatomic, assign)
    segmentation_platform::DeviceSwitcherResultDispatcher*
        deviceSwitcherResultDispatcher;

/// Preferred toolbar to contain the omnibox.
@property(nonatomic, readonly) ToolbarType preferredOmniboxPosition;

/// Creates an instance of the mediator. Observers will be installed into all
/// existing web states in `webStateList`. While the mediator is alive,
/// observers will be added and removed from web states when they are inserted
/// into or removed from the web state list.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         isIncognito:(BOOL)isIncognito;

/// Disconnects all observers set by the mediator on any web states in its
/// web state list. After `disconnect` is called, the mediator will not add
/// observers to further webstates.
- (void)disconnect;

/// Location bar (omnibox) focus has changed to `focused`.
- (void)locationBarFocusChangedTo:(BOOL)focused;

/// NTP became active on the active web state. This can happen after web state
/// finish navigation.
- (void)didNavigateToNTPOnActiveWebState;

/// Toolbar's trait collection changed to `traitCollection`.
- (void)toolbarTraitCollectionChangedTo:(UITraitCollection*)traitCollection;

/// Sets the omnibox initial position to the correct toolbar.
- (void)setInitialOmniboxPosition;

/// Sets the bottom offset required by the omnibox.
- (void)setBottomOmniboxOffsetForPopup:(CGFloat)bottomOffset;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_TOOLBAR_MEDIATOR_H_
