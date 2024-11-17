// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_UIKIT_UI_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_UIKIT_UI_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/web/common/uikit_ui_util.h"

// UI Util containing functions that require UIKit.

// Utility function to set the `element`'s accessibility label to the localized
// message corresponding to `idsAccessibilityLabel` and its accessibility
// identifier to `englishUiAutomationName`.
// Call SetA11yLabelAndUiAutomationName() if `element` is accessible and its
// a11y label should be localized.
// By convention `englishUiAutomationName` must be equal to the English
// localized string corresponding to `idsAccessibilityLabel`.
// `englishUiAutomationName` is the name used in JavaScript UI Automation test
// scripts to identify the `element`.
void SetA11yLabelAndUiAutomationName(
    NSObject<UIAccessibilityIdentification>* element,
    int idsAccessibilityLabel,
    NSString* englishUiAutomationName);

// Sets dynamic font for the given `font` on iOS 11+ on the givel `label` or
// `textField`. Use `maybe` versions to keep code short when dynamic types are
// not in use yet.
void SetUILabelScaledFont(UILabel* label, UIFont* font);
void MaybeSetUILabelScaledFont(BOOL maybe, UILabel* label, UIFont* font);
void SetUITextFieldScaledFont(UITextField* textField, UIFont* font);
void MaybeSetUITextFieldScaledFont(BOOL maybe,
                                   UITextField* textField,
                                   UIFont* font);
// Creates a dynamically scablable custom font based on the given parameters.
UIFont* CreateDynamicFont(UIFontTextStyle style, UIFontWeight weight);
UIFont* CreateDynamicFont(UIFontTextStyle style,
                          UIFontWeight weight,
                          id<UITraitEnvironment> environment);

enum CaptureViewOption {
  kNoCaptureOption,      // Equivalent to calling CaptureView without options.
  kAfterScreenUpdate,    // Require a synchronization with CA process which can
                         // have side effects.
  kClientSideRendering,  // Triggers a client side compositing, very slow.
};

// Captures and returns an autoreleased rendering of the `view`.
// The `view` is assumed to be opaque and the returned image does
// not have an alpha channel. The scale parameter is used as a scale factor
// for the rendering context. Using 0.0 as scale will result in the device's
// main screen scale to be used.
// The CaptureViewWithOption function can be used with the `option`
// parameter set to kAfterScreenUpdate if some changes performed in the view
// and/or it's subtree that have not yet been part of a committed implicit
// transaction must be reflected in the snapshot.
// For example, it should be used if you just performed changes in the view or
// its subviews before calling that function and wants those changes to be
// reflected in the snapshot.
// Calling CaptureView without option gives the best performances. If you only
// need to hide subviews consider selectively rendering subviews in a bitmap
// context using drawViewHierarchyInRect:afterScreenUpdates:NO.
// The kClientSideRendering option can be used to directly re-render the view
// client side instead of reusing the core animation layer's backing store, this
// is slow.
UIImage* CaptureViewWithOption(UIView* view,
                               CGFloat scale,
                               CaptureViewOption option);
UIImage* CaptureView(UIView* view, CGFloat scale);

// Converts input image and returns a grey scaled version.
UIImage* GreyImage(UIImage* image);

// Returns an UIColor with `rgb` and `alpha`. The caller should pass the RGB
// value in hexadecimal as this is the typical way they are provided by UX.
// For example a call to `UIColorFromRGB(0xFF7D40, 1.0)` returns an orange
// UIColor object.
inline UIColor* UIColorFromRGB(int rgb, CGFloat alpha = 1.0) {
  return [UIColor colorWithRed:((CGFloat)((rgb & 0xFF0000) >> 16)) / 255.0
                         green:((CGFloat)((rgb & 0x00FF00) >> 8)) / 255.0
                          blue:((CGFloat)(rgb & 0x0000FF)) / 255.0
                         alpha:alpha];
}
// Returns the image from the shared resource bundle with the image id
// `imageID`. If `reversible` is YES and RTL layout is in use, the image
// will be flipped for RTL.
UIImage* NativeReversibleImage(int imageID, BOOL reversible);

// Convenience version of NativeReversibleImage for images that are never
// reversible; equivalent to NativeReversibleImage(imageID, NO).
UIImage* NativeImage(int imageID);

// Returns the first responder in the subviews of `view`, or nil if no view in
// the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view);

// Returns the interface orientation of the given window in the app.
UIInterfaceOrientation GetInterfaceOrientation(UIWindow* window);

// Returns the height of the keyboard in the current orientation.
CGFloat CurrentKeyboardHeight(NSValue* keyboardFrameValue);

// Create 1x1px image from `color`.
UIImage* ImageWithColor(UIColor* color);

// Returns a circular image of width `width` based on `image` scaled up or
// down. If the source image is not square, the image is first cropped.
UIImage* CircularImageFromImage(UIImage* image, CGFloat width);

// Returns true if the window is in portrait orientation or if orientation is
// unknown.
bool IsPortrait(UIWindow* window);

// Returns true if the window is in landscape orientation.
bool IsLandscape(UIWindow* window);

// C does not support function overloading.
#ifdef __cplusplus
// Whether the `environment` has a compact horizontal size class.
bool IsCompactWidth(id<UITraitEnvironment> environment);

// Whether the `traitCollection` has a compact horizontal size class.
bool IsCompactWidth(UITraitCollection* traitCollection);

// Whether the `environment` has a compact vertical size class.
bool IsCompactHeight(id<UITraitEnvironment> environment);

// Whether the `traitCollection` has a compact vertical size class.
bool IsCompactHeight(UITraitCollection* traitCollection);

// Whether toolbar should be shown in compact mode in `environment`.
bool ShouldShowCompactToolbar(id<UITraitEnvironment> environment);

// Whether toolbar should be shown in compact mode in `traitCollection`.
bool ShouldShowCompactToolbar(UITraitCollection* traitCollection);

// Returns whether the `environment`'s toolbar is split between top and bottom
// toolbar or if it is displayed as only one toolbar.
bool IsSplitToolbarMode(id<UITraitEnvironment> environment);

// Returns whether the `traitCollection`'s toolbar is split between top and
// bottom toolbar or if it is displayed as only one toolbar.
bool IsSplitToolbarMode(UITraitCollection* traitCollection);
#endif  // __cplusplus

// Returns the current first responder for keyWindow.
UIResponder* GetFirstResponder();
// Returns the current first responder for `windowScene.keyWindow`. If none,
// returns the current first responder for the first window which has one in
// this scene, if any.
UIResponder* GetFirstResponderInWindowScene(UIWindowScene* windowScene);

// Trigger a haptic vibration for various types of actions. This is a no-op for
// devices that do not support haptic feedback.
void TriggerHapticFeedbackForSelectionChange();
// `impactStyle` should represent the mass of the object in the collision
// simulated by this feedback.
void TriggerHapticFeedbackForImpact(UIImpactFeedbackStyle impactStyle);
// `type` represent the type of notification associated with this feedback.
void TriggerHapticFeedbackForNotification(UINotificationFeedbackType type);

// Returns the attributed text for tabs count to be displayed in toolbar and
// TabGrid, with the correct font. As an easter egg, show a smiley face instead
// of the count if the user has more than 99 tabs open.
NSAttributedString* TextForTabCount(int count, CGFloat font_size);

// Returns the attributed text for tabs count to be displayed in the bottom
// trailing view of a group cell with the correct font.
NSAttributedString* TextForTabGroupCount(int count, CGFloat font_size);

// Finds the root of `view`'s view hierarchy -- its window if it has one, or
// the first (recursive) superview with no superview.
UIView* ViewHierarchyRootForView(UIView* view);

// Creates and inits a medium-sized UIActivityIndicatorView, regardless of iOS
// version.
UIActivityIndicatorView* GetMediumUIActivityIndicatorView();

// Creates and inits a large-sized UIActivityIndicatorView, regardless of iOS
// version.
UIActivityIndicatorView* GetLargeUIActivityIndicatorView();

// Whether the given scroll view is considered scrolled to its top/bottom.
bool IsScrollViewScrolledToTop(UIScrollView* scroll_view);
bool IsScrollViewScrolledToBottom(UIScrollView* scroll_view);

// Returns the approximate corner radius of the current device.
CGFloat DeviceCornerRadius();

// Returns whether bottom omnibox is an available option.
bool IsBottomOmniboxAvailable();

// Returns the `traits` array provided in the function's parameter if the
// feature flag for the 'traitCollectionDidChange' refactor work is enabled.
// Otherwise, return an array containing every iOS UITrait.
NSArray<UITrait>* TraitCollectionSetForTraits(NSArray<UITrait>* traits)
    API_AVAILABLE(ios(17.0));

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_UIKIT_UI_UTIL_H_
