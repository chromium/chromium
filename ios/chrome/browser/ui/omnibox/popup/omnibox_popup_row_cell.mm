// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#import "base/check.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTextTopMargin = 6;
const CGFloat kTrailingButtonSize = 24;
const CGFloat kTrailingButtonTrailingMargin = 14;
const CGFloat kTopGradientColorOpacity = 0.85;
const CGFloat kTextSpacingActionsEnabled = 2.0f;
/// In Variation 2, the images and the text in the popup don't align with the
/// omnibox image. If Variation 2 becomes default, probably we don't need the
/// fancy layout guide setup and can get away with simple margins.
const CGFloat kImageOffsetVariation2 = 8.0f;
const CGFloat kImageAdditionalOffsetVariation2PopoutOmnibox = 10.0f;
const CGFloat kAdditionalTextOffsetVariation2 = 8.0f;
const CGFloat kTextOffsetVariation2 = 8.0f;
const CGFloat kTrailingButtonPointSize = 17.0f;

NSString* const kOmniboxPopupRowSwitchTabAccessibilityIdentifier =
    @"OmniboxPopupRowSwitchTabAccessibilityIdentifier";
}  // namespace

@interface OmniboxPopupRowCell ()

/// The suggestion that this cell is currently displaying.
@property(nonatomic, strong) id<AutocompleteSuggestion> suggestion;
/// Whether the cell is currently displaying in incognito mode or not.
@property(nonatomic, assign) BOOL incognito;

/// Stack view containing all text labels.
@property(nonatomic, strong) UIStackView* textStackView;
/// Truncating label for the main text.
@property(nonatomic, strong) FadeTruncatingLabel* textTruncatingLabel;
/// Truncating label for the detail text.
@property(nonatomic, strong) FadeTruncatingLabel* detailTruncatingLabel;
/// Regular UILabel for the detail text when the suggestion is an answer.
/// Answers have slightly different display requirements, like possibility of
/// multiple lines and truncating with ellipses instead of a fade gradient.
@property(nonatomic, strong) UILabel* detailAnswerLabel;
/// Trailing button for appending suggestion into omnibox or switching to open
/// tab.
@property(nonatomic, strong) ExtendedTouchTargetButton* trailingButton;
/// Separator line for adjacent cells.
@property(nonatomic, strong) UIView* separator;

/// Stores the extra constraints activated when the cell enters deletion mode.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* deletingLayoutGuideConstraints;
/// Stores the extra constrants activated when the cell is not in deletion mode.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* nonDeletingLayoutGuideConstraints;

@end

@implementation OmniboxPopupRowCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _incognito = NO;

    if (IsOmniboxActionsEnabled()) {
      self.selectedBackgroundView = [[GradientView alloc]
          initWithTopColor:
              [[UIColor colorNamed:@"omnibox_suggestion_row_highlight_color"]
                  colorWithAlphaComponent:kTopGradientColorOpacity]
               bottomColor:
                   [UIColor
                       colorNamed:@"omnibox_suggestion_row_highlight_color"]];
    } else {
      self.selectedBackgroundView = [[UIView alloc] initWithFrame:CGRectZero];
      self.selectedBackgroundView.backgroundColor =
          [UIColor colorNamed:kTableViewRowHighlightColor];
    }

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
    if (IsOmniboxActionsEnabled()) {
      _textStackView.spacing = kTextSpacingActionsEnabled;
    }

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

    self.backgroundColor =
        IsOmniboxActionsVisualTreatment2()
            ? [UIColor colorNamed:kGroupedSecondaryBackgroundColor]
            : UIColor.clearColor;

    [self addInteraction:[[ViewPointerInteraction alloc] init]];
  }
  return self;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];

  if (self.window) {
    // Setup the layout when the view has a window.
    if (self.contentView.subviews.count == 0) {
      [self setupLayout];
    }
    if (self.suggestion.isAppendable || self.suggestion.isTabMatch) {
      [self setupTrailingButtonLayout];
    }
    [self attachToLayoutGuides];
  }
}

- (void)willTransitionToState:(UITableViewCellStateMask)state {
  // `UITableViewCellStateDefaultMask` is actually 0, so it must be checked
  // manually, and can't be checked with bitwise AND.
  if (state == UITableViewCellStateDefaultMask) {
    for (NSLayoutConstraint* constraint in self
             .deletingLayoutGuideConstraints) {
      DCHECK(constraint.active);
    }
    [self unfreezeLayoutGuidePositions];
  } else if (state & UITableViewCellStateShowingDeleteConfirmationMask) {
    for (NSLayoutConstraint* constraint in self
             .nonDeletingLayoutGuideConstraints) {
      DCHECK(constraint.active);
    }
    [self freezeLayoutGuidePositions];
  }
}

#pragma mark - UITableViewCell

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated {
  [super setHighlighted:highlighted animated:animated];

  if (!IsOmniboxActionsEnabled()) {
    return;
  }

  UIColor* textColor = highlighted ? [UIColor whiteColor] : nil;
  self.textTruncatingLabel.textColor = textColor;
  self.detailTruncatingLabel.textColor = textColor;
  self.detailAnswerLabel.textColor = textColor;

  self.leadingIconView.highlighted = highlighted;
  self.trailingButton.tintColor =
      highlighted ? [UIColor whiteColor] : [UIColor colorNamed:kBlueColor];
  [self setupWithCurrentData];
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
  if (omniboxSemanticContentAttribute == _omniboxSemanticContentAttribute) {
    return;
  }
  _omniboxSemanticContentAttribute = omniboxSemanticContentAttribute;
  self.contentView.semanticContentAttribute = omniboxSemanticContentAttribute;
  self.textStackView.semanticContentAttribute = omniboxSemanticContentAttribute;
  // The layout guides may have been repositioned before this, so re-freeze.
  if (self.showingDeleteConfirmation) {
    [self unfreezeLayoutGuidePositions];
    [self freezeLayoutGuidePositions];
  } else if (self.window) {
    // The layout guides may have been repositioned, so remove the constraints
    // and add them again.
    [self attachToLayoutGuides];
  }
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
        constraintEqualToConstant:1.0f / self.window.screen.scale],
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

/// Add the trailing button as a subview and setup its constraints.
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
  DCHECK(imageLayoutGuide);
  DCHECK(textLayoutGuide);

  // The text stack view is attached to both ends of the layout gude. This is
  // because it needs to switch directions if the device is in LTR mode and the
  // user types in RTL. Furthermore, because the layout guide is added to the
  // main view, its direction will not change if the `semanticContentAttribute`
  // of this cell or the omnibox changes.
  // However, the text should still extend all the way to cell's trailing edge.
  // To do this, constrain the text to the layout guide using a low priority
  // constraint, so it will be there if possible, but add medium priority
  // constraint to the cell's trailing edge. This will pull the text past the
  // layout guide if necessary.

  NSLayoutConstraint* stackViewToLayoutGuideLeading =
      [self.textStackView.leadingAnchor
          constraintEqualToAnchor:textLayoutGuide.leadingAnchor
                         constant:IsOmniboxActionsVisualTreatment2()
                                      ? kTextOffsetVariation2
                                      : 0];
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

  // These constraints need to be removed when freezing the position of these
  // views. See -freezeLayoutGuidePositions for the reason why.

  CGFloat iconXOffset = 0;
  BOOL isRTL = [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                           self.omniboxSemanticContentAttribute] ==
               UIUserInterfaceLayoutDirectionRightToLeft;

  if (IsOmniboxActionsVisualTreatment2() && !IsRegularXRegularSizeClass(self)) {
    // Inset the icons in variation 2, except in reg x reg size class where the
    // alignment works well already. Flip the inset on RTL as it's not flipped
    // automatically.
    iconXOffset = isRTL ? -kImageOffsetVariation2 : kImageOffsetVariation2;
  }

  if (IsIpadPopoutOmniboxEnabled() && IsOmniboxActionsVisualTreatment2()) {
    iconXOffset += kImageAdditionalOffsetVariation2PopoutOmnibox;
  }

  [NSLayoutConstraint
      deactivateConstraints:self.nonDeletingLayoutGuideConstraints];
  self.nonDeletingLayoutGuideConstraints = @[
    [self.leadingIconView.centerXAnchor
        constraintEqualToAnchor:imageLayoutGuide.centerXAnchor
                       constant:iconXOffset],
    [self.leadingIconView.widthAnchor
        constraintEqualToAnchor:imageLayoutGuide.widthAnchor],
    stackViewToLayoutGuideLeading,
    stackViewToLayoutGuideTrailing,
    stackViewToCellTrailing,
  ];

  [NSLayoutConstraint
      activateConstraints:self.nonDeletingLayoutGuideConstraints];
}

/// Freezes the position of any view that is positioned relative to the layout
/// guides. When the view enters deletion mode (swipe-to-delete), the layout
/// guides do not move. This means that the views in this cell positioned
/// relative to the layout guide also do not move with the swipe. This method
/// freezes those views with constraints relative to the cell content view so
/// they do move with the swipe-to-delete.
- (void)freezeLayoutGuidePositions {
  [NSLayoutConstraint
      deactivateConstraints:self.nonDeletingLayoutGuideConstraints];

  NamedGuide* imageLayoutGuide =
      [NamedGuide guideWithName:kOmniboxLeadingImageGuide view:self];
  NamedGuide* textLayoutGuide = [NamedGuide guideWithName:kOmniboxTextFieldGuide
                                                     view:self];

  // Layout guides should both be setup
  DCHECK(imageLayoutGuide.isConstrained);
  DCHECK(textLayoutGuide.isConstrained);

  self.deletingLayoutGuideConstraints = @[
    [self.leadingIconView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:
                           [self leadingSpaceForLayoutGuide:imageLayoutGuide]],
    [self.textStackView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:
                           [self leadingSpaceForLayoutGuide:textLayoutGuide]],
  ];

  [NSLayoutConstraint activateConstraints:self.deletingLayoutGuideConstraints];
}

/// Helper method for -freezeLayoutGuidePositions to calculate the actual
/// distance between the leading edge of a layout guide and the leading edge
/// of the cell's content view.
- (CGFloat)leadingSpaceForLayoutGuide:(UILayoutGuide*)layoutGuide {
  CGRect layoutGuideFrame =
      [layoutGuide.owningView convertRect:layoutGuide.layoutFrame
                                   toView:self.contentView];
  CGFloat leadingSpace = self.omniboxSemanticContentAttribute ==
                                 UISemanticContentAttributeForceRightToLeft
                             ? self.contentView.bounds.size.width -
                                   layoutGuideFrame.origin.x -
                                   layoutGuideFrame.size.width
                             : layoutGuideFrame.origin.x;

  if (IsIpadPopoutOmniboxEnabled() && IsOmniboxActionsVisualTreatment2()) {
    leadingSpace += self.omniboxSemanticContentAttribute ==
                            UISemanticContentAttributeForceRightToLeft
                        ? -kAdditionalTextOffsetVariation2
                        : kAdditionalTextOffsetVariation2;
  }

  return leadingSpace;
}

/// Unfreezes the position of any view that is positioned relative to a layout
/// guide. See the comment on -freezeLayoutGuidePositions for why that is
/// necessary.
- (void)unfreezeLayoutGuidePositions {
  [NSLayoutConstraint
      deactivateConstraints:self.deletingLayoutGuideConstraints];
  self.deletingLayoutGuideConstraints = @[];
  [NSLayoutConstraint
      activateConstraints:self.nonDeletingLayoutGuideConstraints];
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.highlighted = NO;
  self.selected = NO;
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

/// Use the given autocomplete suggestion and whether incognito is enabled to
/// layout the cell correctly for that data.
- (void)setupWithAutocompleteSuggestion:(id<AutocompleteSuggestion>)suggestion
                              incognito:(BOOL)incognito {
  self.suggestion = suggestion;
  self.incognito = incognito;

  [self setupWithCurrentData];
}

/// Returns the input string but painted white when the blue and white
/// highlighting is enabled in pedals. Returns the original string otherwise.
- (NSAttributedString*)highlightedAttributedStringWithString:
    (NSAttributedString*)string {
  if (!IsOmniboxActionsEnabled()) {
    return string;
  }
  NSMutableAttributedString* mutableString =
      [[NSMutableAttributedString alloc] initWithAttributedString:string];
  [mutableString addAttribute:NSForegroundColorAttributeName
                        value:[UIColor whiteColor]
                        range:NSMakeRange(0, string.length)];
  return mutableString;
}

- (void)setupWithCurrentData {
  id<AutocompleteSuggestion> suggestion = self.suggestion;

  self.separator.backgroundColor =
      self.incognito ? [UIColor.whiteColor colorWithAlphaComponent:0.12]
                     : [UIColor.blackColor colorWithAlphaComponent:0.12];

  self.textTruncatingLabel.attributedText =
      self.highlighted
          ? [self highlightedAttributedStringWithString:suggestion.text]
          : suggestion.text;

  // URLs have have special layout requirements.
  self.detailTruncatingLabel.displayAsURL = suggestion.isURL;
  UILabel* detailLabel = suggestion.hasAnswer ? self.detailAnswerLabel
                                              : self.detailTruncatingLabel;
  if (suggestion.detailText.length > 0) {
    [self.textStackView addArrangedSubview:detailLabel];
    detailLabel.attributedText =
        self.highlighted
            ? [self highlightedAttributedStringWithString:suggestion.detailText]
            : suggestion.detailText;
    if (suggestion.hasAnswer) {
      detailLabel.numberOfLines = suggestion.numberOfLines;
    }
  }

  [self.leadingIconView setOmniboxIcon:suggestion.icon];

  if (suggestion.isAppendable || suggestion.isTabMatch) {
    [self setupTrailingButton];
  }

  if (IsOmniboxActionsEnabled()) {
    self.leadingIconView.highlighted = self.highlighted;
    self.trailingButton.tintColor = self.highlighted
                                        ? [UIColor whiteColor]
                                        : [UIColor colorNamed:kBlueColor];
  }
}

/// Setup the trailing button. This includes both setting up the button's layout
/// and popuplating it with the correct image and color.
- (void)setupTrailingButton {
  if (self.window) {
    [self setupTrailingButtonLayout];
  }
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
  if (IsOmniboxActionsVisualTreatment2()) {
    trailingButtonImage =
        self.suggestion.isTabMatch
            ? DefaultSymbolWithPointSize(kNavigateToTabSymbol,
                                         kTrailingButtonPointSize)
            : DefaultSymbolWithPointSize(kRefineQuerySymbol,
                                         kTrailingButtonPointSize);
    trailingButtonImage =
        trailingButtonImage.imageFlippedForRightToLeftLayoutDirection;
  } else {
    if (UseSymbolsInOmnibox()) {
      trailingButtonImage =
          self.suggestion.isTabMatch
              ? DefaultSymbolWithPointSize(kNavigateToTabSymbol,
                                           kTrailingButtonPointSize)
              : DefaultSymbolWithPointSize(kRefineQuerySymbol,
                                           kTrailingButtonPointSize);
      trailingButtonImage =
          trailingButtonImage.imageFlippedForRightToLeftLayoutDirection;
    } else {
      if (self.suggestion.isTabMatch) {
        trailingButtonImage = [UIImage imageNamed:@"omnibox_popup_tab_match"];
        trailingButtonImage =
            trailingButtonImage.imageFlippedForRightToLeftLayoutDirection;
      } else {
        int trailingButtonResourceID = 0;
        trailingButtonResourceID = IDR_IOS_OMNIBOX_KEYBOARD_VIEW_APPEND;
        trailingButtonImage =
            NativeReversableImage(trailingButtonResourceID, YES);
      }
    }
  }
  trailingButtonImage = [trailingButtonImage
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  [self.trailingButton setImage:trailingButtonImage
                       forState:UIControlStateNormal];
  self.trailingButton.tintColor = [UIColor colorNamed:kBlueColor];
  if (self.suggestion.isTabMatch) {
    self.trailingButton.accessibilityIdentifier =
        kOmniboxPopupRowSwitchTabAccessibilityIdentifier;
  }
}

- (NSString*)accessibilityLabel {
  return self.textTruncatingLabel.attributedText.string;
}

- (NSString*)accessibilityValue {
  return self.suggestion.hasAnswer
             ? self.detailAnswerLabel.attributedText.string
             : self.detailTruncatingLabel.attributedText.string;
}

- (void)trailingButtonTapped {
  [self.delegate trailingButtonTappedForCell:self];
}

@end
