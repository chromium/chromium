// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_UIKIT_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_UIKIT_UI_UTIL_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/util/ui_util.h"

// UI Util containing functions that require UIKit.

enum { FONT_HELVETICA, FONT_HELVETICA_NEUE, FONT_HELVETICA_NEUE_LIGHT };

// Utility function to set the |element|'s accessibility label to the localized
// message corresponding to |idsAccessibilityLabel| and its accessibility
// identifier to |englishUiAutomationName|.
// Call SetA11yLabelAndUiAutomationName() if |element| is accessible and its
// a11y label should be localized.
// By convention |englishUiAutomationName| must be equal to the English
// localized string corresponding to |idsAccessibilityLabel|.
// |englishUiAutomationName| is the name used in JavaScript UI Automation test
// scripts to identify the |element|.
void SetA11yLabelAndUiAutomationName(
    NSObject<UIAccessibilityIdentification>* element,
    int idsAccessibilityLabel,
    NSString* englishUiAutomationName);

// Sets the given |button|'s width to exactly fit its image and text.  Does not
// modify the button's height.
void GetSizeButtonWidthToFit(UIButton* button);

// Translates the given |view|'s frame.  Sets a new frame instead of applying a
// transform to the existing frame.
void TranslateFrame(UIView* view, UIOffset offset);

// Returns a UIFont. |fontFace| is one of the defined enumerated values
// to avoid spelling mistakes.
UIFont* GetUIFont(int fontFace, bool isBold, CGFloat fontSize);

// Sets dynamic font for the given |font| on iOS 11+ on the givel |label| or
// |textField|. Use |maybe| versions to keep code short when dynamic types are
// not in use yet.
void SetUILabelScaledFont(UILabel* label, UIFont* font);
void MaybeSetUILabelScaledFont(BOOL maybe, UILabel* label, UIFont* font);
void SetUITextFieldScaledFont(UITextField* textField, UIFont* font);
void MaybeSetUITextFieldScaledFont(BOOL maybe,
                                   UITextField* textField,
                                   UIFont* font);

// Adds a border shadow around |view|.
void AddBorderShadow(UIView* view, CGFloat offset, UIColor* color);

// Adds a rounded-rectangle border shadow around a view.
void AddRoundedBorderShadow(UIView* view, CGFloat radius, UIColor* color);

enum CaptureViewOption {
  kNoCaptureOption,      // Equivalent to calling CaptureView without options.
  kAfterScreenUpdate,    // Require a synchronization with CA process which can
                         // have side effects.
  kClientSideRendering,  // Triggers a client side compositing, very slow.
};

// Captures and returns an autoreleased rendering of the |view|.
// The |view| is assumed to be opaque and the returned image does
// not have an alpha channel. The scale parameter is used as a scale factor
// for the rendering context. Using 0.0 as scale will result in the device's
// main screen scale to be used.
// The CaptureViewWithOption function can be used with the |option|
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
// On iOS < 9 this function is slow and always behave as if the option was set
// to kClientSideRendering.
UIImage* CaptureViewWithOption(UIView* view,
                               CGFloat scale,
                               CaptureViewOption option);
UIImage* CaptureView(UIView* view, CGFloat scale);

// Converts input image and returns a grey scaled version.
UIImage* GreyImage(UIImage* image);

// Returns the color that should be used for the background of all Settings
// pages.
UIColor* GetSettingsBackgroundColor();

// Returns the color used as the main color for primary action buttons.
UIColor* GetPrimaryActionButtonColor();

// Returns an UIColor with |rgb| and |alpha|. The caller should pass the RGB
// value in hexadecimal as this is the typical way they are provided by UX.
// For example a call to |UIColorFromRGB(0xFF7D40, 1.0)| returns an orange
// UIColor object.
inline UIColor* UIColorFromRGB(int rgb, CGFloat alpha = 1.0) {
  return [UIColor colorWithRed:((CGFloat)((rgb & 0xFF0000) >> 16)) / 255.0
                         green:((CGFloat)((rgb & 0x00FF00) >> 8)) / 255.0
                          blue:((CGFloat)(rgb & 0x0000FF)) / 255.0
                         alpha:alpha];
}

// Returns whether an image contains an alpha channel. If yes, displaying the
// image will require blending.
// Intended for use in debug.
BOOL ImageHasAlphaChannel(UIImage* image);

// Returns the image from the shared resource bundle with the image id
// |imageID|. If |reversable| is YES and RTL layout is in use, the image
// will be flipped for RTL.
UIImage* NativeReversableImage(int imageID, BOOL reversable);

// Convenience version of NativeReversableImage for images that are never
// reversable; equivalent to NativeReversableImage(imageID, NO).
UIImage* NativeImage(int imageID);

// Returns an image resized to |targetSize|. It first calculate the projection
// by calling CalculateProjection() and then create a new image of the desired
// size and project the correct subset of the original image onto it.
// The resulting image will have an alpha channel.
//
// Image interpolation level for resizing is set to kCGInterpolationDefault.
//
// The resize always preserves the scale of the original image.
UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode);

// Returns an image resized to |targetSize|. It first calculate the projection
// by calling CalculateProjection() and then create a new image of the desired
// size and project the correct subset of the original image onto it.
// |opaque| determine whether resulting image should have an alpha channel.
// Prefer setting |opaque| to YES for better performances.
//
// Image interpolation level for resizing is set to kCGInterpolationDefault.
//
// The resize always preserves the scale of the original image.
UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode,
                     BOOL opaque);

// Returns a slightly blurred image darkened enough to provide contrast for
// white text to be readable.
UIImage* DarkenImage(UIImage* image);

// Applies various effects to an image. This method can apply a blur over a
// |radius|, superimpose a |tintColor| (an alpha of 0.6 on the color is a good
// approximation to look like iOS tint colors) or saturate the image colors by
// applying a |saturationDeltaFactor| (negative to desaturate, positive to
// saturate). The optional |maskImage| is used to limit the effect of the blur
// and/or saturation to a portion of the image.
UIImage* BlurImage(UIImage* image,
                   CGFloat blurRadius,
                   UIColor* tintColor,
                   CGFloat saturationDeltaFactor,
                   UIImage* maskImage);

// Returns an output image where each pixel has RGB values equal to a color and
// the alpha value sampled from the given image. The RGB values of the image are
// ignored. If the color has alpha value of less than one, then the entire
// output image's alpha is scaled by the color's alpha value.
UIImage* TintImage(UIImage* image, UIColor* color);

// Returns the first responder in the subviews of |view|, or nil if no view in
// the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view);

// Returns a cropped image using |cropRect| on |image|.
UIImage* CropImage(UIImage* image, const CGRect& cropRect);

// Returns the interface orientation of the app.
UIInterfaceOrientation GetInterfaceOrientation();

// Returns the height of the keyboard in the current orientation.
CGFloat CurrentKeyboardHeight(NSValue* keyboardFrameValue);

// Create 1x1px image from |color|.
UIImage* ImageWithColor(UIColor* color);

// Returns a circular image of width |width| based on |image| scaled up or
// down. If the source image is not square, the image is first cropped.
UIImage* CircularImageFromImage(UIImage* image, CGFloat width);

// Returns the linear interpolated color from |firstColor| to |secondColor| by
// the given |fraction|. Requires that both colors are in RGB or monochrome
// color space. |fraction| is a decimal value between 0.0 and 1.0.
UIColor* InterpolateFromColorToColor(UIColor* firstColor,
                                     UIColor* secondColor,
                                     CGFloat fraction);

// Whether the |environment| has a compact horizontal size class.
bool IsCompactWidth(id<UITraitEnvironment> environment);

// Whether the main application window's rootViewController has a compact
// horizontal size class.
bool IsCompactWidth();

// Whether the |environment| has a compact iPad horizontal size class.
bool IsCompactTablet(id<UITraitEnvironment> environment);

// Whether the main application window's rootViewController has a compact
// iPad horizontal size class.
bool IsCompactTablet();

// Whether the main application window's rootViewController has a compact
// vertical size class.
bool IsCompactHeight();

// Whether the |environment| has a compact vertical size class.
bool IsCompactHeight(id<UITraitEnvironment> environment);

// Whether toolbar should be shown in compact mode.
bool ShouldShowCompactToolbar();
// Whether toolbar should be shown in compact mode in |traitCollection|.
bool ShouldShowCompactToolbar(UITraitCollection* traitCollection);

// Whether the the main application window's rootViewController has a regular
// vertical and regular horizontal size class.
bool IsRegularXRegularSizeClass();
// Whether the |environment| has a regular vertical and regular horizontal
// size class.
bool IsRegularXRegularSizeClass(id<UITraitEnvironment> environment);
// Whether the |traitCollection| has a regular vertical and regular horizontal
// size class.
bool IsRegularXRegularSizeClass(UITraitCollection* traitCollection);

// Returns whether the toolbar is split between top and bottom toolbar or if it
// is displayed as only one toolbar.
bool IsSplitToolbarMode();

// Returns whether the |environment|'s toolbar is split between top and bottom
// toolbar or if it is displayed as only one toolbar.
bool IsSplitToolbarMode(id<UITraitEnvironment> environment);

// Returns the current first responder for keyWindow.
UIResponder* GetFirstResponder();

// Trigger a haptic vibration for various types of actions. This is a no-op for
// devices that do not support haptic feedback.
void TriggerHapticFeedbackForSelectionChange();
// |impactStyle| should represent the mass of the object in the collision
// simulated by this feedback.
void TriggerHapticFeedbackForImpact(UIImpactFeedbackStyle impactStyle);
// |type| represent the type of notification associated with this feedback.
void TriggerHapticFeedbackForNotification(UINotificationFeedbackType type);

// Returns the text for tabs count to be displayed in toolbar and tab_grid.
// As an easter egg, show a smiley face instead of the count if the user has
// more than 99 tabs open.
NSString* TextForTabCount(long count);

#endif  // IOS_CHROME_BROWSER_UI_UTIL_UIKIT_UI_UTIL_H_
