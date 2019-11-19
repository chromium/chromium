// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/copied_content_view.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/logging.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/search_widget_extension/search_widget_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kURLButtonMargin = 10;

}  // namespace

@interface CopiedContentView ()

// The type of the copied content
@property(nonatomic) CopiedContentType type;
// The copied text to be displayed if the type supports showing the string.
@property(nonatomic, copy) NSString* copiedText;
// The copied URL label containing the URL or a placeholder text.
@property(nonatomic, strong) UILabel* copiedContentLabel;
// The copied URL title label containing the title of the copied URL button.
@property(nonatomic, strong) UILabel* openCopiedContentTitleLabel;
// The hairline view potentially shown at the top of the copied URL view.
@property(nonatomic, strong) UIView* hairlineView;
// The button-shaped background view shown when there is a copied URL to open.
@property(nonatomic, strong) UIView* copiedButtonView;

@property(nonatomic, strong) UIVisualEffectView* primaryEffectView;
@property(nonatomic, strong) UIVisualEffectView* secondaryEffectView;

// Updates the view to show the currently set |type| and |copiedText|.
- (void)updateUI;

@end

@implementation CopiedContentView

- (instancetype)initWithActionTarget:(id)target
                      actionSelector:(SEL)actionSelector {
  DCHECK(target);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;

    [self addTarget:target
                  action:actionSelector
        forControlEvents:UIControlEventTouchUpInside];

    UIVibrancyEffect* primaryEffect =
        [UIVibrancyEffect widgetPrimaryVibrancyEffect];
    UIVibrancyEffect* secondaryEffect =
        [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    UIVibrancyEffect* backgroundEffect =
        [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    UIVibrancyEffect* hairlineEffect =
        [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    if (@available(iOS 13, *)) {
      primaryEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleLabel];
      secondaryEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSecondaryLabel];
      backgroundEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleTertiaryFill];
      hairlineEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSeparator];
    }

    _primaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:primaryEffect];
    _secondaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:secondaryEffect];
    UIVisualEffectView* backgroundEffectView =
        [[UIVisualEffectView alloc] initWithEffect:backgroundEffect];
    UIVisualEffectView* hairlineEffectView =
        [[UIVisualEffectView alloc] initWithEffect:hairlineEffect];
    for (UIVisualEffectView* effectView in @[
           _primaryEffectView, _secondaryEffectView, backgroundEffectView,
           hairlineEffectView
         ]) {
      [self addSubview:effectView];
      effectView.translatesAutoresizingMaskIntoConstraints = NO;
      AddSameConstraints(self, effectView);
      effectView.userInteractionEnabled = NO;
    }

    _hairlineView = [[UIView alloc] initWithFrame:CGRectZero];
    // The new widget vibrancy style API requires new colors for the views.
    if (@available(iOS 13, *)) {
      _hairlineView.backgroundColor = UIColor.separatorColor;
    } else {
      _hairlineView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.05];
    }
    _hairlineView.translatesAutoresizingMaskIntoConstraints = NO;
    [hairlineEffectView.contentView addSubview:_hairlineView];

    _copiedButtonView = [[UIView alloc] init];
    // The new widget vibrancy style API requires new colors for the views.
    if (@available(iOS 13, *)) {
      _copiedButtonView.backgroundColor = UIColor.whiteColor;
    } else {
      _copiedButtonView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.05];
    }
    _copiedButtonView.layer.cornerRadius = 5;
    _copiedButtonView.translatesAutoresizingMaskIntoConstraints = NO;
    [backgroundEffectView.contentView addSubview:_copiedButtonView];

    _openCopiedContentTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _openCopiedContentTitleLabel.textAlignment = NSTextAlignmentCenter;
    _openCopiedContentTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _openCopiedContentTitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    [_primaryEffectView.contentView addSubview:_openCopiedContentTitleLabel];

    _copiedContentLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _copiedContentLabel.textAlignment = NSTextAlignmentCenter;
    _copiedContentLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _copiedContentLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_secondaryEffectView.contentView addSubview:_copiedContentLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_hairlineView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_hairlineView.leftAnchor constraintEqualToAnchor:self.leftAnchor],
      [_hairlineView.rightAnchor constraintEqualToAnchor:self.rightAnchor],
      [_hairlineView.heightAnchor constraintEqualToConstant:0.5],

      [_copiedButtonView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor
                         constant:kContentMargin],
      [_copiedButtonView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor
                         constant:-kContentMargin],
      [_copiedButtonView.topAnchor constraintEqualToAnchor:self.topAnchor
                                                  constant:kContentMargin],
      [_copiedButtonView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                     constant:-kContentMargin],

      [_openCopiedContentTitleLabel.topAnchor
          constraintEqualToAnchor:_copiedButtonView.topAnchor
                         constant:kURLButtonMargin],
      [_openCopiedContentTitleLabel.leadingAnchor
          constraintEqualToAnchor:_copiedButtonView.leadingAnchor
                         constant:kContentMargin],
      [_openCopiedContentTitleLabel.trailingAnchor
          constraintEqualToAnchor:_copiedButtonView.trailingAnchor
                         constant:-kContentMargin],

      [_copiedContentLabel.topAnchor
          constraintEqualToAnchor:_openCopiedContentTitleLabel.bottomAnchor],
      [_copiedContentLabel.bottomAnchor
          constraintEqualToAnchor:_copiedButtonView.bottomAnchor
                         constant:-kURLButtonMargin],
      [_copiedContentLabel.leadingAnchor
          constraintEqualToAnchor:_openCopiedContentTitleLabel.leadingAnchor],
      [_copiedContentLabel.trailingAnchor
          constraintEqualToAnchor:_openCopiedContentTitleLabel.trailingAnchor],
    ]];
    [self setCopiedContentType:CopiedContentTypeNone copiedText:nil];
    self.highlightableViews = @[
      _hairlineView, _copiedButtonView, _openCopiedContentTitleLabel,
      _copiedContentLabel
    ];
  }
  return self;
}

- (void)setCopiedContentType:(CopiedContentType)type
                  copiedText:(NSString*)copiedText {
  self.type = type;
  self.copiedText = copiedText;
  [self updateUI];
}

- (void)updateUI {
  BOOL hasContent = self.type != CopiedContentTypeNone;
  self.userInteractionEnabled = hasContent;
  self.copiedButtonView.hidden = !hasContent;
  self.hairlineView.hidden = hasContent;
  self.accessibilityTraits =
      (hasContent) ? UIAccessibilityTraitLink : UIAccessibilityTraitNone;
  if (@available(iOS 13, *)) {
    if (hasContent) {
      self.primaryEffectView.effect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleLabel];
      self.secondaryEffectView.effect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSecondaryLabel];
    } else {
      self.primaryEffectView.effect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSecondaryLabel];
      self.secondaryEffectView.effect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleTertiaryLabel];
    }
  } else {
    self.copiedContentLabel.alpha = (hasContent) ? 1 : 0.5;
    self.openCopiedContentTitleLabel.alpha = (hasContent) ? 1 : 0.5;
  }

  NSString* titleText;
  NSString* contentText;
  switch (self.type) {
    case CopiedContentTypeNone: {
      titleText = NSLocalizedString(@"IDS_IOS_NO_COPIED_CONTENT_TITLE", nil);
      contentText =
          NSLocalizedString(@"IDS_IOS_NO_COPIED_CONTENT_MESSAGE", nil);
      break;
    }
    case CopiedContentTypeURL: {
      titleText = NSLocalizedString(@"IDS_IOS_OPEN_COPIED_LINK", nil);
      contentText = self.copiedText;
      break;
    }
    case CopiedContentTypeString: {
      titleText = NSLocalizedString(@"IDS_IOS_OPEN_COPIED_TEXT", nil);
      contentText = self.copiedText;
      break;
    }
    case CopiedContentTypeImage: {
      titleText = NSLocalizedString(@"IDS_IOS_OPEN_COPIED_IMAGE", nil);
      break;
    }
  }
  self.openCopiedContentTitleLabel.text = titleText;
  self.copiedContentLabel.text = contentText;
  NSMutableArray<NSString*>* accessibilityPieces =
      [[NSMutableArray alloc] init];
  [accessibilityPieces addObject:titleText];
  if (contentText) {
    [accessibilityPieces addObject:contentText];
  }
  self.accessibilityLabel =
      [accessibilityPieces componentsJoinedByString:@" - "];
}

@end
