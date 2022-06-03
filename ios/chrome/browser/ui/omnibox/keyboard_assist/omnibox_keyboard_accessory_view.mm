// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_keyboard_accessory_view.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxKeyboardAccessoryView ()

@property(nonatomic, retain) NSArray<NSString*>* buttonTitles;
@property(nonatomic, weak) id<OmniboxAssistiveKeyboardDelegate> delegate;

// Called when a keyboard shortcut button is pressed.
- (void)keyboardButtonPressed:(NSString*)title;
// Creates a button shortcut for |title|.
- (UIView*)shortcutButtonWithTitle:(NSString*)title;

@end

@implementation OmniboxKeyboardAccessoryView

@synthesize buttonTitles = _buttonTitles;
@synthesize delegate = _delegate;

- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                       delegate:(id<OmniboxAssistiveKeyboardDelegate>)delegate {
  self = [super initWithFrame:CGRectZero
               inputViewStyle:UIInputViewStyleKeyboard];
  if (self) {
    _buttonTitles = buttonTitles;
    _delegate = delegate;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.allowsSelfSizing = YES;
    [self addSubviews];
  }
  return self;
}

- (void)addSubviews {
  if (!self.subviews.count)
    return;

  const CGFloat kButtonMinWidth = 36.0;
  const CGFloat kButtonHeight = 36.0;
  const CGFloat kBetweenShortcutButtonSpacing = 5.0;
  const CGFloat kBetweenSearchButtonSpacing = 12.0;
  const CGFloat kHorizontalMargin = 10.0;
  const CGFloat kVerticalMargin = 4.0;

  // Create and add stackview filled with the shortcut buttons.
  UIStackView* shortcutStackView = [[UIStackView alloc] init];
  shortcutStackView.translatesAutoresizingMaskIntoConstraints = NO;
  shortcutStackView.spacing = kBetweenShortcutButtonSpacing;
  shortcutStackView.alignment = UIStackViewAlignmentCenter;
  for (NSString* title in self.buttonTitles) {
    UIView* button = [self shortcutButtonWithTitle:title];
    [button setTranslatesAutoresizingMaskIntoConstraints:NO];
    [button.widthAnchor constraintGreaterThanOrEqualToConstant:kButtonMinWidth]
        .active = YES;
    [button.heightAnchor constraintEqualToConstant:kButtonHeight].active = YES;
    [shortcutStackView addArrangedSubview:button];
  }
  [self addSubview:shortcutStackView];

  // Create and add a stackview containing the leading assistive buttons, i.e.
  // Voice search and camera search.
  NSArray<UIButton*>* leadingButtons =
      OmniboxAssistiveKeyboardLeadingButtons(_delegate);
  UIStackView* searchStackView = [[UIStackView alloc] init];
  searchStackView.translatesAutoresizingMaskIntoConstraints = NO;
  searchStackView.spacing = kBetweenSearchButtonSpacing;
  for (UIButton* button in leadingButtons) {
    [searchStackView addArrangedSubview:button];
  }
  [self addSubview:searchStackView];

  // Position the stack views.
  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [searchStackView.leadingAnchor
        constraintEqualToAnchor:layoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [shortcutStackView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
    [searchStackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:shortcutStackView.leadingAnchor],
    [searchStackView.topAnchor constraintEqualToAnchor:layoutGuide.topAnchor
                                              constant:kVerticalMargin],
    [searchStackView.bottomAnchor
        constraintEqualToAnchor:layoutGuide.bottomAnchor
                       constant:-kVerticalMargin],
    [shortcutStackView.topAnchor
        constraintEqualToAnchor:searchStackView.topAnchor],
    [shortcutStackView.bottomAnchor
        constraintEqualToAnchor:searchStackView.bottomAnchor],
  ]];
}

- (UIView*)shortcutButtonWithTitle:(NSString*)title {
  const CGFloat kHorizontalEdgeInset = 8;
  const CGFloat kButtonTitleFontSize = 16.0;
  UIColor* kTitleColorStateNormal = [UIColor colorWithWhite:0.0 alpha:1.0];
  UIColor* kTitleColorStateHighlighted = [UIColor colorWithWhite:0.0 alpha:0.3];

  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  [button setTitleColor:kTitleColorStateNormal forState:UIControlStateNormal];
  [button setTitleColor:kTitleColorStateHighlighted
               forState:UIControlStateHighlighted];

  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kTextPrimaryColor]
               forState:UIControlStateNormal];
  button.contentEdgeInsets =
      UIEdgeInsetsMake(0, kHorizontalEdgeInset, 0, kHorizontalEdgeInset);
  button.clipsToBounds = YES;
  [button.titleLabel setFont:[UIFont systemFontOfSize:kButtonTitleFontSize
                                               weight:UIFontWeightMedium]];

  [button addTarget:self
                action:@selector(keyboardButtonPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  button.isAccessibilityElement = YES;
  [button setAccessibilityLabel:title];
  return button;
}

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

- (void)keyboardButtonPressed:(id)sender {
  UIButton* button = base::mac::ObjCCastStrict<UIButton>(sender);
  [[UIDevice currentDevice] playInputClick];
  [_delegate keyPressed:[button currentTitle]];
}

@end
