// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell_experimental.h"

#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/attributed_string_util.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
const CGFloat kTextTopMargin = 6.0;
const CGFloat kMultilineTextTopMargin = 12.0;
/// Trailing margin of the text. This margin is increased when the text is on
/// multiple lines, otherwise text of the first lines without the gradient seems
/// too close to the trailing (button/end).
const CGFloat kTextTrailingMargin = 0.0;
const CGFloat kMultilineTextTrailingMargin = 4.0;
const CGFloat kMultilineLineSpacing = 2.0;
const CGFloat kTrailingButtonSize = 24;
const CGFloat kTrailingButtonTrailingMargin = 14;
const CGFloat kTopGradientColorOpacity = 0.85;
const CGFloat kTextSpacing = 2.0f;
const CGFloat kLeadingIconViewWidth = 30.0f;
const CGFloat kContentViewLeadingSpace = 17.0f;
const CGFloat kTextIconSpace = 14.0f;
/// In Variation 2, the images and the text in the popup don't align with the
/// omnibox image. If Variation 2 becomes default, probably we don't need the
/// fancy layout guide setup and can get away with simple margins.
const CGFloat kTrailingButtonPointSize = 17.0f;
/// Maximum number of lines displayed for search suggestion when
/// `kOmniboxMultilineSearchSuggest` is enabled.
const NSInteger kSearchSuggestNumberOfLines = 2;

/// Name of the histogram recording the number of lines in search suggestions.
const char kOmniboxSearchSuggestionNumberOfLines[] =
    "IOS.Omnibox.SearchSuggestionNumberOfLines";

/// Returns `YES` if `kOmniboxMultilineSearchSuggest` is enabled.
BOOL IsMultilineSearchSuggestionEnabled() {
  return base::FeatureList::IsEnabled(kOmniboxMultilineSearchSuggest);
}

}  // namespace

@interface OmniboxPopupRowCellExperimental ()

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

/// Constraints that changes when the text is a multi-lines search suggestion.
@property(nonatomic, strong) NSLayoutConstraint* textTopConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textTrailingToButtonConstraint;
@property(nonatomic, strong) NSLayoutConstraint* textTrailingConstraint;

@end

@implementation OmniboxPopupRowCellExperimental

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _incognito = NO;

    self.selectedBackgroundView = [[GradientView alloc]
        initWithTopColor:
            [[UIColor colorNamed:@"omnibox_suggestion_row_highlight_color"]
                colorWithAlphaComponent:kTopGradientColorOpacity]
             bottomColor:
                 [UIColor
                     colorNamed:@"omnibox_suggestion_row_highlight_color"]];

    _textTruncatingLabel =
        [[FadeTruncatingLabel alloc] initWithFrame:CGRectZero];
    _textTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_textTruncatingLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                        forAxis:UILayoutConstraintAxisVertical];
    _textTruncatingLabel.lineSpacing = kMultilineLineSpacing;

    _textStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textTruncatingLabel ]];
    _textStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _textStackView.axis = UILayoutConstraintAxisVertical;
    _textStackView.alignment = UIStackViewAlignmentFill;
    _textStackView.spacing = kTextSpacing;

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
    BOOL suggestionNeedsTrailingButton =
        self.suggestion.isAppendable || self.suggestion.isTabMatch;

    if (suggestionNeedsTrailingButton && !self.trailingButton.superview) {
      [self setupTrailingButtonLayout];
    }
  }
}

#pragma mark - UITableViewCell

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated {
  [super setHighlighted:highlighted animated:animated];

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

  // Top space should be at least the given top margin, but can be more if
  // the row is short enough to use the minimum height constraint above.
  self.textTopConstraint = [self.textStackView.topAnchor
      constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                  constant:kTextTopMargin];

  // When there is no trailing button, the text should extend to the cell's
  // trailing edge with a padding.
  self.textTrailingConstraint = [self.contentView.trailingAnchor
      constraintEqualToAnchor:self.textStackView.trailingAnchor
                     constant:kTextTrailingMargin];
  self.textTrailingConstraint.priority = UILayoutPriorityRequired - 1;

  NSLayoutAnchor* contentViewLeadingAnchor =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? self.contentView.layoutMarginsGuide.leadingAnchor
          : self.contentView.leadingAnchor;

  [NSLayoutConstraint activateConstraints:@[
    // Row has a minimum height.
    [self.contentView.heightAnchor
        constraintGreaterThanOrEqualToConstant:kOmniboxPopupCellMinimumHeight],

    // Position leadingIconView at the leading edge of the view.
    [self.leadingIconView.widthAnchor
        constraintEqualToConstant:kLeadingIconViewWidth],
    [self.leadingIconView.heightAnchor
        constraintEqualToAnchor:self.leadingIconView.widthAnchor],
    [self.leadingIconView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [self.leadingIconView.leadingAnchor
        constraintEqualToAnchor:contentViewLeadingAnchor
                       constant:kContentViewLeadingSpace],

    // Position textStackView "after" leadingIconView.
    self.textTopConstraint,
    self.textTrailingConstraint,
    [self.textStackView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [self.textStackView.leadingAnchor
        constraintEqualToAnchor:self.leadingIconView.trailingAnchor
                       constant:kTextIconSpace],

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

  self.textTrailingToButtonConstraint = [self.trailingButton.leadingAnchor
      constraintEqualToAnchor:self.textStackView.trailingAnchor
                     constant:kTextTrailingMargin];
  [NSLayoutConstraint activateConstraints:@[
    [self.trailingButton.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.trailingButton.trailingAnchor
                       constant:kTrailingButtonTrailingMargin],
    self.textTrailingToButtonConstraint,
  ]];
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
  self.textTruncatingLabel.truncateMode = FadeTruncatingTail;
  self.textTruncatingLabel.semanticContentAttribute =
      UISemanticContentAttributeUnspecified;
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

/// Updates the text constraint according to `isMultiline`.
- (void)updateTextConstraints:(BOOL)isMultiline {
  if (isMultiline) {
    self.textTopConstraint.constant = kMultilineTextTopMargin;
    self.textTrailingConstraint.constant = kMultilineTextTrailingMargin;
    self.textTrailingToButtonConstraint.constant = kMultilineTextTrailingMargin;
  } else {
    self.textTopConstraint.constant = kTextTopMargin;
    self.textTrailingConstraint.constant = kTextTrailingMargin;
    self.textTrailingToButtonConstraint.constant = kTextTrailingMargin;
  }
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
  if (suggestion.isWrapping) {
    [self logNumberOfLinesSearchSuggestions:self.textTruncatingLabel
                                                .attributedText];
    if (base::FeatureList::IsEnabled(kOmniboxMultilineSearchSuggest)) {
      self.textTruncatingLabel.numberOfLines = kSearchSuggestNumberOfLines;
      base::i18n::TextDirection textDirection = base::i18n::GetStringDirection(
          base::SysNSStringToUTF16(self.textTruncatingLabel.text));
      if (textDirection == base::i18n::RIGHT_TO_LEFT) {
        self.textTruncatingLabel.semanticContentAttribute =
            UISemanticContentAttributeForceRightToLeft;
        self.textTruncatingLabel.truncateMode = FadeTruncatingHead;
      }
    }
  } else {
    // Default values for FadeTruncatingLabel.
    self.textTruncatingLabel.lineBreakMode = NSLineBreakByClipping;
    self.textTruncatingLabel.numberOfLines = 1;
  }

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
  [self updateTextConstraints:IsMultilineSearchSuggestionEnabled() &&
                              suggestion.isWrapping];

  self.leadingIconView.highlighted = self.highlighted;
  self.trailingButton.tintColor =
      self.highlighted ? [UIColor whiteColor] : [UIColor colorNamed:kBlueColor];
}

/// Setup the trailing button. This includes both setting up the button's layout
/// and popuplating it with the correct image and color.
- (void)setupTrailingButton {
  if (self.window && !self.trailingButton.superview) {
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

  UIImage* trailingButtonImage =
      self.suggestion.isTabMatch
          ? DefaultSymbolWithPointSize(kNavigateToTabSymbol,
                                       kTrailingButtonPointSize)
          : DefaultSymbolWithPointSize(kRefineQuerySymbol,
                                       kTrailingButtonPointSize);
  trailingButtonImage =
      trailingButtonImage.imageFlippedForRightToLeftLayoutDirection;

  trailingButtonImage = [trailingButtonImage
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  [self.trailingButton setImage:trailingButtonImage
                       forState:UIControlStateNormal];
  self.trailingButton.tintColor = [UIColor colorNamed:kBlueColor];

  self.trailingButton.accessibilityIdentifier =
      self.suggestion.isTabMatch
          ? kOmniboxPopupRowSwitchTabAccessibilityIdentifier
          : kOmniboxPopupRowAppendAccessibilityIdentifier;
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
  [self.delegate trailingButtonTappedForCell:((OmniboxPopupRowCell*)self)];
}

#pragma mark - Metrics

- (void)logNumberOfLinesSearchSuggestions:
    (NSAttributedString*)attributedString {
  CGFloat width = CGRectGetWidth(self.textStackView.frame);
  NSInteger numberOfLines =
      NumberOfLinesOfAttributedString(attributedString, width);
  UMA_HISTOGRAM_EXACT_LINEAR(kOmniboxSearchSuggestionNumberOfLines,
                             static_cast<int>(numberOfLines), 10);
}

@end
