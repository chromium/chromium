// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

#import <CoreText/CoreText.h>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/command_line.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/grit/components_scaled_resources.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/reversed_animation.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/color_palette.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/scoped_cg_context_save_gstate_mac.h"

@interface OmniboxTextFieldIOS ()
@end

@implementation OmniboxTextFieldIOS

@dynamic delegate;

#pragma mark - Public methods

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor {
  self = [super initWithFrame:frame];
  return self;
}

- (instancetype)initWithCoder:(nonnull NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)setText:(NSAttributedString*)text
    userTextLength:(size_t)userTextLength {
}

- (void)insertTextWhileEditing:(NSString*)text {
}

- (NSString*)displayedText {
  return @"";
}

- (NSString*)userText {
  return @"";
}

- (NSString*)autocompleteText {
  return @"";
}

- (BOOL)hasAutocompleteText {
  return YES;
}

- (void)clearAutocompleteText {
}

- (NSString*)markedText {
  return @"";
}

- (NSRange)selectedNSRange {
  DCHECK([self isFirstResponder]);
  UITextPosition* beginning = [self beginningOfDocument];
  UITextRange* selectedRange = [self selectedTextRange];
  NSInteger start = [self offsetFromPosition:beginning
                                  toPosition:[selectedRange start]];
  NSInteger length = [self offsetFromPosition:[selectedRange start]
                                   toPosition:[selectedRange end]];
  return NSMakeRange(start, length);
}

- (NSTextAlignment)bestTextAlignment {
  return NSTextAlignmentNatural;
}

- (UISemanticContentAttribute)bestSemanticContentAttribute {
  return UISemanticContentAttributeForceLeftToRight;
}

- (void)updateTextDirection {
}

- (UIColor*)displayedTextColor {
  return [UIColor whiteColor];
}

#pragma mark animations

- (void)animateFadeWithStyle:(OmniboxTextFieldFadeStyle)style {
}

- (void)cleanUpFadeAnimations {
}

#pragma mark - UI Refresh animation public helpers

- (CGFloat)offsetForString:(NSString*)string {
  return 0;
}

#pragma mark pre-edit

- (void)enterPreEditState {
}

- (void)exitPreEditState {
}

- (BOOL)isPreEditing {
  return NO;
}

#pragma mark - TestingUtilities category

- (UILabel*)preEditStaticLabel {
  return nil;
}

#pragma mark - OmniboxKeyboardDelegate

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  return NO;
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
}

#pragma mark - Properties

- (UIFont*)largerFont {
  return nil;
}

- (UIFont*)normalFont {
  return nil;
}

- (UIFont*)currentFont {
  return nil;
}
@end
