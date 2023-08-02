// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

#import <Accelerate/Accelerate.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <stddef.h>
#import <stdint.h>
#import <cmath>

#import "base/check.h"
#import "base/check_op.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/numerics/math_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/gfx/ios/uikit_util.h"
#import "ui/gfx/scoped_cg_context_save_gstate_mac.h"

void SetA11yLabelAndUiAutomationName(
    NSObject<UIAccessibilityIdentification>* element,
    int idsAccessibilityLabel,
    NSString* englishUiAutomationName) {
  [element setAccessibilityLabel:l10n_util::GetNSString(idsAccessibilityLabel)];
  [element setAccessibilityIdentifier:englishUiAutomationName];
}

void SetUILabelScaledFont(UILabel* label, UIFont* font) {
  label.font = [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
  label.adjustsFontForContentSizeCategory = YES;
}

void MaybeSetUILabelScaledFont(BOOL maybe, UILabel* label, UIFont* font) {
  if (maybe) {
    SetUILabelScaledFont(label, font);
  } else {
    label.font = font;
  }
}

void SetUITextFieldScaledFont(UITextField* textField, UIFont* font) {
  textField.font = [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
  textField.adjustsFontForContentSizeCategory = YES;
}

void MaybeSetUITextFieldScaledFont(BOOL maybe,
                                   UITextField* textField,
                                   UIFont* font) {
  if (maybe) {
    SetUITextFieldScaledFont(textField, font);
  } else {
    textField.font = font;
  }
}

UIFont* CreateDynamicFont(UIFontTextStyle style, UIFontWeight weight) {
  UIFontDescriptor* fontDescriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:style];
  return [UIFont systemFontOfSize:fontDescriptor.pointSize weight:weight];
}

UIImage* CaptureViewWithOption(UIView* view,
                               CGFloat scale,
                               CaptureViewOption option) {
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = scale;
  format.opaque = NO;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:view.bounds.size
                                             format:format];

  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [view drawViewHierarchyInRect:view.bounds
               afterScreenUpdates:option == kClientSideRendering];
  }];
}

UIImage* CaptureView(UIView* view, CGFloat scale) {
  return CaptureViewWithOption(view, scale, kNoCaptureOption);
}

UIImage* GreyImage(UIImage* image) {
  DCHECK(image);
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  // Grey images are always non-retina to improve memory performance.
  format.scale = 1;
  format.opaque = YES;
  CGRect greyImageRect = CGRectMake(0, 0, image.size.width, image.size.height);
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:greyImageRect.size
                                             format:format];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    UIBezierPath* background = [UIBezierPath bezierPathWithRect:greyImageRect];
    [UIColor.blackColor set];
    [background fill];

    [image drawInRect:greyImageRect blendMode:kCGBlendModeLuminosity alpha:1.0];
  }];
}

UIImage* NativeReversibleImage(int imageID, BOOL reversible) {
  DCHECK(imageID);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  UIImage* image = rb.GetNativeImageNamed(imageID).ToUIImage();
  return (reversible && UseRTLLayout())
             ? [image imageFlippedForRightToLeftLayoutDirection]
             : image;
}

UIImage* NativeImage(int imageID) {
  return NativeReversibleImage(imageID, NO);
}

UIInterfaceOrientation GetInterfaceOrientation(UIWindow* window) {
  return window.windowScene.interfaceOrientation;
}

UIActivityIndicatorView* GetMediumUIActivityIndicatorView() {
  return [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
}

UIActivityIndicatorView* GetLargeUIActivityIndicatorView() {
  return [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
}

CGFloat CurrentKeyboardHeight(NSValue* keyboardFrameValue) {
  return [keyboardFrameValue CGRectValue].size.height;
}

UIImage* ImageWithColor(UIColor* color) {
  CGRect rect = CGRectMake(0, 0, 1, 1);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, [color CGColor]);
  CGContextFillRect(context, rect);
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

UIImage* CircularImageFromImage(UIImage* image, CGFloat width) {
  CGRect frame =
      CGRectMakeAlignedAndCenteredAt(width / 2.0, width / 2.0, width);

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = NO;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:frame.size format:format];
  return
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* UIContext) {
        CGContextRef context = UIContext.CGContext;
        CGContextBeginPath(context);
        CGContextAddEllipseInRect(context, frame);
        CGContextClosePath(context);
        CGContextClip(context);

        CGFloat scaleX = frame.size.width / image.size.width;
        CGFloat scaleY = frame.size.height / image.size.height;
        CGFloat scale = std::max(scaleX, scaleY);
        CGContextScaleCTM(context, scale, scale);

        [image
            drawInRect:CGRectMake(0, 0, image.size.width, image.size.height)];
      }];
}

bool IsPortrait(UIWindow* window) {
  UIInterfaceOrientation orient = GetInterfaceOrientation(window);
  return UIInterfaceOrientationIsPortrait(orient) ||
         orient == UIInterfaceOrientationUnknown;
}

bool IsLandscape(UIWindow* window) {
  return UIInterfaceOrientationIsLandscape(GetInterfaceOrientation(window));
}

bool IsCompactWidth(id<UITraitEnvironment> environment) {
  return IsCompactWidth(environment.traitCollection);
}

bool IsCompactWidth(UITraitCollection* traitCollection) {
  return traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact;
}

bool IsCompactHeight(id<UITraitEnvironment> environment) {
  return IsCompactHeight(environment.traitCollection);
}

bool IsCompactHeight(UITraitCollection* traitCollection) {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
}

bool IsRegularXRegularSizeClass(id<UITraitEnvironment> environment) {
  return IsRegularXRegularSizeClass(environment.traitCollection);
}

bool IsRegularXRegularSizeClass(UITraitCollection* traitCollection) {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

bool ShouldShowCompactToolbar(id<UITraitEnvironment> environment) {
  return ShouldShowCompactToolbar(environment.traitCollection);
}

bool ShouldShowCompactToolbar(UITraitCollection* traitCollection) {
  return !IsRegularXRegularSizeClass(traitCollection);
}

bool IsSplitToolbarMode(id<UITraitEnvironment> environment) {
  return IsSplitToolbarMode(environment.traitCollection);
}

bool IsSplitToolbarMode(UITraitCollection* traitCollection) {
  return IsCompactWidth(traitCollection) && !IsCompactHeight(traitCollection);
}

UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder]) {
    return view;
  }

  for (UIView* subview in [view subviews]) {
    UIView* firstResponder = GetFirstResponderSubview(subview);
    if (firstResponder) {
      return firstResponder;
    }
  }

  return nil;
}

UIResponder* GetFirstResponder() {
  DCHECK(NSThread.isMainThread);
  return GetFirstResponderSubview(GetAnyKeyWindow());
}

UIResponder* GetFirstResponderInWindowScene(UIWindowScene* windowScene) {
  DCHECK(NSThread.isMainThread);
  if (!windowScene) {
    return nil;
  }

  // First checking the key window for this window scene.
  UIResponder* responder = GetFirstResponderSubview(windowScene.keyWindow);
  if (responder) {
    return responder;
  }

  for (UIWindow* window in windowScene.windows) {
    if (window.isKeyWindow) {
      continue;
    }
    responder = GetFirstResponderSubview(window);
    if (responder) {
      return responder;
    }
  }

  return nil;
}

// Trigger a haptic vibration for the user selecting an action. This is a no-op
// for devices that do not support it.
void TriggerHapticFeedbackForImpact(UIImpactFeedbackStyle impactStyle) {
  // Although Apple documentation claims that UIFeedbackGenerator and its
  // concrete subclasses are available on iOS 10+, they are not really
  // available on an app whose deployment target is iOS 10.0 (iOS 10.1+ are
  // okay) and Chrome will fail at dynamic link time and instantly crash.
  // NSClassFromString() checks if Objective-C run-time has the class before
  // using it.
  Class generatorClass = NSClassFromString(@"UIImpactFeedbackGenerator");
  if (generatorClass) {
    UIImpactFeedbackGenerator* generator =
        [[generatorClass alloc] initWithStyle:impactStyle];
    [generator impactOccurred];
  }
}

// Trigger a haptic vibration for the change in selection. This is a no-op for
// devices that do not support it.
void TriggerHapticFeedbackForSelectionChange() {
  // Although Apple documentation claims that UIFeedbackGenerator and its
  // concrete subclasses are available on iOS 10+, they are not really
  // available on an app whose deployment target is iOS 10.0 (iOS 10.1+ are
  // okay) and Chrome will fail at dynamic link time and instantly crash.
  // NSClassFromString() checks if Objective-C run-time has the class before
  // using it.
  Class generatorClass = NSClassFromString(@"UISelectionFeedbackGenerator");
  if (generatorClass) {
    UISelectionFeedbackGenerator* generator = [[generatorClass alloc] init];
    [generator selectionChanged];
  }
}

// Trigger a haptic vibration for a notification. This is a no-op for devices
// that do not support it.
void TriggerHapticFeedbackForNotification(UINotificationFeedbackType type) {
  // Although Apple documentation claims that UIFeedbackGenerator and its
  // concrete subclasses are available on iOS 10+, they are not really
  // available on an app whose deployment target is iOS 10.0 (iOS 10.1+ are
  // okay) and Chrome will fail at dynamic link time and instantly crash.
  // NSClassFromString() checks if Objective-C run-time has the class before
  // using it.
  Class generatorClass = NSClassFromString(@"UINotificationFeedbackGenerator");
  if (generatorClass) {
    UINotificationFeedbackGenerator* generator = [[generatorClass alloc] init];
    [generator notificationOccurred:type];
  }
}

NSAttributedString* TextForTabCount(int count, CGFloat font_size) {
  NSString* string;
  if (count <= 0) {
    string = @"";
  } else if (count > 99) {
    string = @":)";
  } else {
    string = [NSString stringWithFormat:@"%d", count];
  }

  UIFontWeight weight =
      UIAccessibilityIsBoldTextEnabled() ? UIFontWeightHeavy : UIFontWeightBold;
  UIFont* font = [UIFont systemFontOfSize:font_size weight:weight];
  UIFontDescriptor* descriptor = [font.fontDescriptor
      fontDescriptorWithDesign:UIFontDescriptorSystemDesignRounded];
  font = [UIFont fontWithDescriptor:descriptor size:font_size];

  return [[NSAttributedString alloc] initWithString:string
                                         attributes:@{
                                           NSFontAttributeName : font,
                                           NSKernAttributeName : @(-0.8),
                                         }];
}

void RegisterEditMenuItem(UIMenuItem* item) {
  UIMenuController* menu = [UIMenuController sharedMenuController];
  NSArray<UIMenuItem*>* items = [menu menuItems];

  for (UIMenuItem* existingItem in items) {
    if ([existingItem action] == [item action]) {
      return;
    }
  }

  items = items ? [items arrayByAddingObject:item] : @[ item ];

  [menu setMenuItems:items];
}

UIView* ViewHierarchyRootForView(UIView* view) {
  if (view.window) {
    return view.window;
  }

  if (!view.superview) {
    return view;
  }

  return ViewHierarchyRootForView(view.superview);
}
