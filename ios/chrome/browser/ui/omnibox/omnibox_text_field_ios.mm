// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

#import <CoreText/CoreText.h>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"

#include "base/strings/sys_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/browser/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/ui/util/reversed_animation.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/common/ui/util/dynamic_type_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#include "ui/gfx/scoped_cg_context_save_gstate_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
