// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTextTopMargin = 6;
const CGFloat kTrailingButtonSize = 24;
const CGFloat kTrailingButtonTrailingMargin = 14;

NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
    @"OmniboxPopupRowSwitchTabAccessibilityIdentifier";
}  // namespace

@interface OmniboxPopupRowCell ()

// The suggestion that this cell is currently displaying.
@property(nonatomic, strong) id<AutocompleteSuggestion> suggestion;
// Whether the cell is currently dispalying in incognito mode or not.
@property(nonatomic, assign) BOOL incognito;

// Stack view containing all text labels.
@property(nonatomic, strong) UIStackView* textStackView;
// Truncating label for the main text.
@property(nonatomic, strong) FadeTruncatingLabel* textTruncatingLabel;
// Truncating label for the detail text.
@property(nonatomic, strong) FadeTruncatingLabel* detailTruncatingLabel;
// Regular UILabel for the detail text when the suggestion is an answer.
// Answers have slightly different display requirements, like possibility of
// multiple lines and truncating with ellipses instead of a fade gradient.
@property(nonatomic, strong) UILabel* detailAnswerLabel;
// Trailing button for appending suggestion into omnibox or switching to open
// tab.
@property(nonatomic, strong) ExtendedTouchTargetButton* trailingButton;
// Separator line for adjacent cells.
@property(nonatomic, strong) UIView* separator;

@end

@implementation OmniboxPopupRowCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _incognito = NO;

    self.selectedBackgroundView = [[UIView alloc] initWithFrame:CGRectZero];
    self.selectedBackgroundView.backgroundColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kTableViewRowHighlightColor], _incognito,
        [UIColor colorNamed:kTableViewRowHighlightDarkColor]);

    _textTruncatingLabel =
        [[FadeTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_textTruncatingLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:UILayoutConstraintAxisVertical];

    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textTruncatingLabel ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textStackView.axis = UILayoutConstraintAxisVertical;
    _textStackView.alignment = UIStackViewAlignmentLeading;

    _detailTruncatingLabel =
        [[FadeTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;

    // Answers use a UILabel with NSLineBreakByTruncatingTail to produce a
    // truncation with an ellipse instead of fading on multi-line text.
    _detailAnswerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _detailAnswerLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _detailAnswerLabel.lineBreakMode = NSLineBreakByTruncatingTail;

    _leadingIconView = [[OmniboxIconView alloc] init];
    _leadingIconView.translatesAutoresizingMaskIntoConstraints = NO;

    _trailingButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
    _trailingButton.translatesAutoresizingMaskIntoConstraints = NO;
    _trailingButton.isAccessibilityElement = NO;
    [_trailingButton addTarget:self
                        action:@selector(trailingButtonTapped)
              forControlEvents:UIControlEventTouchUpInside];

    _separator = [[UIView alloc] initWithFrame:CGRectZero];
    _separator.translatesAutoresizingMaskIntoConstraints = NO;
    _separator.hidden = YES;

    self.backgroundColor = UIColor.clearColor;
  }
  return self;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];

  if (self.window) {
    [self attachToLayoutGuides];
  }
}

#pragma mark - Property setter/getters

- (void)setImageRetriever:(id<ImageRetriever>)imageRetriever {
  _imageRetriever = imageRetriever;
  self.leadingIconView.imageRetriever = imageRetriever;
}

- (void)setFaviconRetriever:(id<FaviconRetriever>)faviconRetriever {
  _faviconRetriever = faviconRetriever;
  self.leadingIconView.faviconRetriever = faviconRetriever;
}

- (void)setOmniboxSemanticContentAttribute:
    (UISemanticContentAttribute)omniboxSemanticContentAttribute {
  _omniboxSemanticContentAttribute = omniboxSemanticContentAttribute;
  self.contentView.semanticContentAttribute = omniboxSemanticContentAttribute;
  self.textStackView.semanticContentAttribute = omniboxSemanticContentAttribute;
}

- (BOOL)showsSeparator {
  return self.separator.hidden;
}

- (void)setShowsSeparator:(BOOL)showsSeparator {
  self.separator.hidden = !showsSeparator;
}

#pragma mark - Layout

// Setup the layout of the cell initially. This only adds the elements that are
// always in the cell.
- (void)setupLayout {
  [self.contentView addSubview:self.leadingIconView];
  [self.contentView addSubview:self.textStackView];
  [self.contentView addSubview:self.separator];

  [NSLayoutConstraint activateConstraints:@[
    // Row has a minimum height.
    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kOmniboxPopupCellMinimumHeight],

    // Position leadingIconView at the leading edge of the view.
    // Leave the horizontal position unconstrained as that will be added via a
    // layout guide once the cell has been added to the view hierarchy.
    [self.leadingIconView.heightAnchor
        constraintEqualToAnchor:self.leadingIconView.widthAnchor],
    [self.leadingIconView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],

    // Position textStackView "after" leadingIconView. The horizontal position
    // is actually left off because it will be added via a
    // layout guide once the cell has been added to the view hierarchy.
    // Top space should be at least the given top margin, but can be more if
    // the row is short enough to use the minimum height constraint above.
    [self.textStackView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:kTextTopMargin],
    [self.textStackView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],

    [self.separator.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [self.separator.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:1.0f / UIScreen.mainScreen.scale],
    [self.separator.leadingAnchor
        constraintEqualToAnchor:self.textStackView.leadingAnchor],
  ]];

  // If optional views have internal constraints (height is constant, etc.),
  // set those up here.
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingButton.heightAnchor
        constraintEqualToConstant:kTrailingButtonSize],
    [self.trailingButton.widthAnchor
        constraintEqualToAnchor:self.trailingButton.heightAnchor],
  ]];
}

// Add the trailing button as a subview and setup its constraints.
- (void)setupTrailingButtonLayout {
  [self.contentView addSubview:self.trailingButton];
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingButton.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.trailingButton.trailingAnchor
                       constant:kTrailingButtonTrailingMargin],
    [self.trailingButton.leadingAnchor
        constraintEqualToAnchor:self.textStackView.trailingAnchor],
  ]];
}

- (void)attachToLayoutGuides {
  NamedGuide* imageLayoutGuide =
      [NamedGuide guideWithName:kOmniboxLeadingImageGuide view:self];
  NamedGuide* textLayoutGuide = [NamedGuide guideWithName:kOmniboxTextFieldGuide
                                                     view:self];

  // Layout guides should both be setup
  DCHECK(imageLayoutGuide.isConstrained);
  DCHECK(textLayoutGuide.isConstrained);

  // The text stack view is attached to both ends of the layout gude. This is
  // because it needs to switch directions if the device is in LTR mode and the
  // user types in RTL. Furthermore, because the layout guide is added to the
  // main view, its direction will not change if the |semanticContentAttribute|
  // of this cell or the omnibox changes.
  // However, the text should still extend all the way to cell's trailing edge.
  // To do this, constrain the text to the layout guide using a low priority
  // constraint, so it will be there if possible, but add medium priority
  // constraint to the cell's trailing edge. This will pull the text past the
  // layout guide if necessary.

  NSLayoutConstraint* stackViewToLayoutGuideLeading =
      [self.textStackView.leadingAnchor
          constraintEqualToAnchor:textLayoutGuide.leadingAnchor];
  NSLayoutConstraint* stackViewToLayoutGuideTrailing =
      [self.textStackView.trailingAnchor
          constraintEqualToAnchor:textLayoutGuide.trailingAnchor];
  NSLayoutConstraint* stackViewToCellTrailing =
      [self.textStackView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor];

  UILayoutPriority highest = UILayoutPriorityRequired - 1;
  UILayoutPriority higher = UILayoutPriorityRequired - 2;

  stackViewToLayoutGuideLeading.priority = higher;
  stackViewToLayoutGuideTrailing.priority = higher;
  stackViewToCellTrailing.priority = highest;

  [NSLayoutConstraint activateConstraints:@[
    [self.leadingIconView.centerXAnchor
        constraintEqualToAnchor:imageLayoutGuide.centerXAnchor],
    [self.leadingIconView.widthAnchor
        constraintEqualToAnchor:imageLayoutGuide.widthAnchor],
    [self.textStackView.leadingAnchor
        constraintEqualToAnchor:textLayoutGuide.leadingAnchor],
    stackViewToLayoutGuideLeading,
    stackViewToLayoutGuideTrailing,
    stackViewToCellTrailing,
  ]];
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.suggestion = nil;
  self.incognito = NO;

  self.omniboxSemanticContentAttribute = UISemanticContentAttributeUnspecified;

  // Clear text.
  self.textTruncatingLabel.attributedText = nil;
  self.detailTruncatingLabel.attributedText = nil;
  self.detailAnswerLabel.attributedText = nil;

  [self.leadingIconView prepareForReuse];

  // Remove optional views.
  [self.trailingButton setImage:nil forState:UIControlStateNormal];
  [self.trailingButton removeFromSuperview];
  [self.detailTruncatingLabel removeFromSuperview];
  [self.detailAnswerLabel removeFromSuperview];

  self.trailingButton.accessibilityIdentifier = nil;

  self.accessibilityCustomActions = nil;
}

#pragma mark - Cell setup with data

// Use the given autocomplete suggestion and whether incognito is enabled to
// layout the cell correctly for that data.
- (void)setupWithAutocompleteSuggestion:(id<AutocompleteSuggestion>)suggestion
                              incognito:(BOOL)incognito {
  // Setup the view layout the first time the cell is setup.
  if (self.contentView.subviews.count == 0) {
    [self setupLayout];
  }
  self.suggestion = suggestion;
  self.incognito = incognito;

  // While iOS 12 is still supported, the background color needs to be reset
  // when the incognito mode changes. Once iOS 12 is no longer supported,
  // the color should only have to be set once.
  if (@available(iOS 13, *)) {
    // Empty because condition should be if (!@available(iOS 13, *)).
  } else {
    self.selectedBackgroundView.backgroundColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kTableViewRowHighlightColor], self.incognito,
        [UIColor colorNamed:kTableViewRowHighlightDarkColor]);
  }

  self.separator.backgroundColor =
      self.incognito ? [UIColor.whiteColor colorWithAlphaComponent:0.12]
                     : [UIColor.blackColor colorWithAlphaComponent:0.12];

  self.textTruncatingLabel.attributedText = self.suggestion.text;

  // URLs have have special layout requirements.
  self.detailTruncatingLabel.displayAsURL = suggestion.isURL;
  UILabel* detailLabel = self.suggestion.hasAnswer ? self.detailAnswerLabel
                                                   : self.detailTruncatingLabel;
  if ([self.suggestion.detailText length] > 0) {
    [self.textStackView addArrangedSubview:detailLabel];
    detailLabel.attributedText = self.suggestion.detailText;
    if (self.suggestion.hasAnswer) {
      detailLabel.numberOfLines = self.suggestion.numberOfLines;
    }
  }

  [self.leadingIconView setOmniboxIcon:self.suggestion.icon];

  if (self.suggestion.isAppendable || self.suggestion.isTabMatch) {
    [self setupTrailingButton];
  }
}

// Setup the trailing button. This includes both setting up the button's layout
// and popuplating it with the correct image and color.
- (void)setupTrailingButton {
  [self setupTrailingButtonLayout];

  // Show append button for search history/search suggestions or
  // switch-to-open-tab as the right control element (aka an accessory element
  // of a table view cell).
  NSString* trailingButtonActionName =
      self.suggestion.isTabMatch
          ? l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_SWITCH_TO_OPEN_TAB)
          : l10n_util::GetNSString(IDS_IOS_OMNIBOX_POPUP_APPEND);
  UIAccessibilityCustomAction* trailingButtonAction =
      [[UIAccessibilityCustomAction alloc]
          initWithName:trailingButtonActionName
                target:self
              selector:@selector(trailingButtonTapped)];

  self.accessibilityCustomActions = @[ trailingButtonAction ];

  UIImage* trailingButtonImage = nil;
  if (self.suggestion.isTabMatch) {
    trailingButtonImage = [UIImage imageNamed:@"omnibox_popup_tab_match"];
    trailingButtonImage =
        trailingButtonImage.imageFlippedForRightToLeftLayoutDirection;
    self.trailingButton.accessibilityIdentifier =
        kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
  } else {
    int trailingButtonResourceID = 0;
    trailingButtonResourceID = IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
    trailingButtonImage = NativeReversableImage(trailingButtonResourceID, YES);
  }
  trailingButtonImage = [trailingButtonImage
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  [self.trailingButton setImage:trailingButtonImage
                       forState:UIControlStateNormal];
  self.trailingButton.tintColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kBlueColor], self.incognito,
      [UIColor colorNamed:kBlueDarkColor]);
}

- (NSString*)accessibilityLabel {
  return self.textTruncatingLabel.attributedText.string;
}

- (NSString*)accessibilityValue {
  return self.suggestion.hasAnswer
             ? self.detailAnswerLabel.attributedText.string
             : self.detailTruncatingLabel.attributedText.string;
}

- (NSString*)accessibilityIdentifier {
  return self.textTruncatingLabel.attributedText.string;
}

- (void)trailingButtonTapped {
  [self.delegate trailingButtonTappedForCell:self];
}

@end
