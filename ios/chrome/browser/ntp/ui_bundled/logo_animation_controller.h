// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_LOGO_ANIMATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_LOGO_ANIMATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

@protocol LogoAnimationControllerOwner;
@protocol LogoAnimationControllerOwnerOwner;

typedef NS_ENUM(NSUInteger, LogoAnimationControllerLogoState) {
  // A state in which the logo is displayed.
  LogoAnimationControllerLogoStateLogo,
  // A state in which the microphone icon is displayed.
  LogoAnimationControllerLogoStateMic,
};

#pragma mark LogoAnimationController

// Protocol for objects that act as logo animation controllers.
@protocol LogoAnimationController <NSObject>
@end

#pragma mark LogoAnimationControllerOwner

// Protocol used to transfer ownership of LogoAnimationControllers between
// objects.
@protocol LogoAnimationControllerOwner

// The controller to be used during the animation.  Setting this property to a
// new controller transfers ownership to the receiver, and resetting it to nil
// relinquishes the receiver's ownership of the object.  When this property is
// set, the owner must add the new controller's view to its view hierarchy and
// remove the old controller's view.
@property(nonatomic, strong) id<LogoAnimationController>
    logoAnimationController;

// The default state used when the LogoAnimationController owned by the
// receiver is not showing dots or animating.
@property(nonatomic, readonly)
    LogoAnimationControllerLogoState defaultLogoState;
@end

#pragma mark LogoAnimationControllerOwnerOwner

// Protocol for objects that potentially own LogoAnimationControllerOwners and
// sometimes (but not always) use them to display UI.  This extra layer of
// indirection is necessary to decide whether to reparent animation views during
// transition animations, as simply checking whether a
// LogoAnimationControllerOwner's logoAnimationController is nil is
// insufficient.  This is because a LogoAnimationController may have been
// reparented, but its LogoAnimationControllerOwner should still be used as the
// endpoint in the animation if it is in a state where it can be displayed.
@protocol LogoAnimationControllerOwnerOwner

// The LogoAnimationControllerOwner held by the conforming object.
@property(nonatomic, readonly) id<LogoAnimationControllerOwner>
    logoAnimationControllerOwner;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_LOGO_ANIMATION_CONTROLLER_H_
