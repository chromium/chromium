// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_suggestion_label.h"

#import <QuartzCore/QuartzCore.h>
#import <stddef.h>

#import <cmath>

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/data_quality/autofill_data_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

using autofill::SuggestionType;

namespace {

// Font size of button titles.
constexpr CGFloat kIpadFontSize = 15;
constexpr CGFloat kIphoneFontSize = 14;

// The horizontal space between the edge of the background and the text.
constexpr CGFloat kBorderWidth = 12;
// The space between items in the label.
constexpr CGFloat kSpacing = 4;
// The corner radius of the label.
constexpr CGFloat kCornerRadius = 8;

// The size adjustment for the subtitle font from the default font size.
constexpr CGFloat kSubtitleFontPointSizeAdjustment = -1;
// The size adjustment for the title font from the default font size.
constexpr CGFloat kTitleFontPointSizeAdjustment = 1;
// The extra space between the title label and the subtitle.
constexpr CGFloat kVerticalSpacing = 2;

// Shadow parameters.
constexpr CGFloat kShadowRadius = 0.5;
constexpr CGFloat kShadowVerticalOffset = 1.0;
constexpr CGFloat kShadowOpacity = 1.0;

// The preferred minimum width of the icon shown on the label.
constexpr CGFloat kSuggestionIconWidth = 40;

// The highlight color's alpha when using liquid glass.
constexpr CGFloat kHighlightColorAlpha = 0.5;

// Offset required to see half of the icon of the 2nd credit card suggestion
// when the first credit card suggestion is at maximum width. This number
// represents the width of the stack view minus the width of the first
// suggestion.
constexpr CGFloat kHalfCreditCardIconOffset =
    2 * kBorderWidth + 2 * kSpacing + 0.5 * kSuggestionIconWidth;

// Returns the font for the title line of the suggestion.
UIFont* TitleFont(CGFloat font_size) {
  return [UIFont systemFontOfSize:font_size + kTitleFontPointSizeAdjustment
                           weight:UIFontWeightMedium];
}

// Returns the font for the subtitle line of the suggestion.
UIFont* SubtitleFont(CGFloat font_size) {
  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  return [font fontWithSize:font_size + kSubtitleFontPointSizeAdjustment];
}

// Returns the font used by a section of the suggestion description text.
UIFont* TextFont(BOOL bold, BOOL is_title) {
  CGFloat font_size =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kIpadFontSize
          : kIphoneFontSize;
  return is_title ? TitleFont(font_size) : SubtitleFont(font_size);
}

// Creates a label with the given `text` and `alpha` suitable for use in a
// suggestion button in the keyboard accessory view.
UILabel* TextLabel(NSString* text,
                   UIColor* text_color,
                   BOOL bold,
                   BOOL is_title) {
  UILabel* label = [[UILabel alloc] init];
  [label setText:text];
  [label setFont:TextFont(bold, is_title)];
  label.textColor = text_color;
  [label setBackgroundColor:[UIColor clearColor]];
  return label;
}

// Creates a string with the given `text` and `text_color` suitable for use in a
// suggestion button in the keyboard accessory view.
NSAttributedString* AsAttributedString(NSString* text,
                                       UIColor* text_color,
                                       BOOL bold,
                                       BOOL is_title,
                                       BOOL has_icon) {
  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  [style setLineSpacing:kVerticalSpacing];

  if (has_icon) {
    [style setFirstLineHeadIndent:kSpacing];
    [style setHeadIndent:kSpacing];
  }

  // If it's not a title, then prepend a new line to the text, so that it shows
  // up on the next line.
  return [[NSAttributedString alloc]
      initWithString:is_title ? text : [NSString stringWithFormat:@"\n%@", text]
          attributes:@{
            NSFontAttributeName : TextFont(bold, is_title),
            NSForegroundColorAttributeName : text_color,
            NSBackgroundColorAttributeName : [UIColor clearColor],
            NSParagraphStyleAttributeName : style
          }];
}

// Returns a potentially multiline UILabel containing the formatted suggestion
// information.
UILabel* AttributedTextLabel(NSString* suggestion_text,
                             NSString* minor_value,
                             NSString* display_description,
                             BOOL has_icon) {
  NSMutableAttributedString* full_text =
      [[NSMutableAttributedString alloc] init];

  [full_text
      appendAttributedString:AsAttributedString(
                                 suggestion_text,
                                 [UIColor colorNamed:kTextPrimaryColor],
                                 /*bold=*/YES, /*is_title=*/YES, has_icon)];
  NSInteger numberOfLines = 1;

  if ([minor_value length] > 0) {
    [full_text
        appendAttributedString:AsAttributedString(
                                   minor_value,
                                   [UIColor colorNamed:kTextPrimaryColor],
                                   /*bold=*/YES, /*is_title=*/NO, has_icon)];
    numberOfLines++;
  }

  if ([display_description length] > 0) {
    [full_text
        appendAttributedString:AsAttributedString(
                                   display_description,
                                   [UIColor colorNamed:kTextSecondaryColor],
                                   /*bold=*/NO, /*is_title=*/NO, has_icon)];
    numberOfLines++;
  }

  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = numberOfLines;
  label.attributedText = full_text;
  return label;
}

// Splits a credit card label into 2 labels, with one being an incompressible
// credit card number label. Returns the label as is if this is not a credit
// card.
UIView* SplitLabel(UILabel* label, BOOL is_credit_card) {
  if (!is_credit_card) {
    return label;
  }

  // Look for a credit card number in the string. Note that U+202A is the
  // "Left-to-right embedding" character and U+202C is the "Pop directional
  // formatting" character. Credit card numbers are surrounded by these two
  // Unicode characters.
  NSRange range =
      [label.text rangeOfString:@"\U0000202a•⁠ ⁠•⁠ ⁠"];
  if (range.location == NSNotFound || range.location < 1) {
    return label;
  }

  // Split the string in pre and post credit card number labels.
  UILabel* credit_card_label = [[UILabel alloc] init];
  credit_card_label.font = label.font;
  credit_card_label.text = [label.text substringFromIndex:range.location];
  credit_card_label.textColor = label.textColor;
  // The credit card number should not be compressible.
  [credit_card_label
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Remove credit card number from the original string.
  label.text = [label.text substringToIndex:range.location - 1];

  // Stack both labels horizontally.
  UIStackView* stack_view = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  stack_view.axis = UILayoutConstraintAxisHorizontal;
  stack_view.alignment = UIStackViewAlignmentCenter;
  stack_view.spacing = kSpacing;
  [stack_view addArrangedSubview:label];
  [stack_view addArrangedSubview:credit_card_label];
  return stack_view;
}

// Returns an array of views, each view containing one piece of formatted
// suggestion related information.
NSArray<UIView*>* TextViews(NSString* suggestion_text,
                            NSString* minor_value,
                            NSString* display_description,
                            BOOL is_credit_card) {
  NSMutableArray<UIView*>* views = [NSMutableArray array];
  UILabel* value_label =
      TextLabel(suggestion_text, [UIColor colorNamed:kTextPrimaryColor],
                /*bold=*/YES, /*is_title=*/YES);
  [views addObject:SplitLabel(value_label, is_credit_card)];

  if ([minor_value length] > 0) {
    UILabel* minor_value_label =
        TextLabel(minor_value, [UIColor colorNamed:kTextPrimaryColor],
                  /*bold=*/YES, /*is_title=*/NO);
    [views addObject:SplitLabel(minor_value_label, is_credit_card)];
  }

  if ([display_description length] > 0) {
    UILabel* description =
        TextLabel(display_description, [UIColor colorNamed:kTextSecondaryColor],
                  /*bold=*/NO, /*is_title=*/NO);
    [views addObject:SplitLabel(description, is_credit_card)];
  }
  return views;
}

// Returns whether the provided `suggestion` is a password suggestion from the
// user's saved data.
bool IsPasswordSuggestion(FormSuggestion* suggestion) {
  switch (suggestion.type) {
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kBackupPasswordEntry:
      return true;
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kOneTimePasswordEntry:
      return false;
  }
  NOTREACHED();
}

// Returns the text to display for a password suggestion.
NSString* PasswordSuggestionDisplayText(NSString* suggestion_value) {
  if ([suggestion_value length] == 0) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_PASSWORD_NO_USERNAME);
  }

  return suggestion_value;
}

// Returns the string to set as the view's accessibility label.
NSString* AccessibilityLabel(NSString* suggestion_text,
                             NSString* suggestion_description,
                             BOOL is_backup_password_suggestion) {
  std::u16string accessibility_label = l10n_util::GetStringFUTF16(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
      base::SysNSStringToUTF16(suggestion_text),
      base::SysNSStringToUTF16(suggestion_description));

  if (is_backup_password_suggestion) {
    // Append an additional mention to the accessibility label.
    accessibility_label = l10n_util::GetStringFUTF16(
        IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION, accessibility_label,
        l10n_util::GetStringUTF16(
            IDS_IOS_KEYBOARD_ACCESSORY_RECOVERY_PASSWORD_ACCESSIBILITY_LABEL));
  }

  return base::SysUTF16ToNSString(accessibility_label);
}

}  // namespace

@implementation FormSuggestionLabel {
  // Client of this view.
  __weak id<FormSuggestionLabelDelegate> _delegate;

  // The suggestion presented by this view.
  FormSuggestion* _suggestion;

  // The index of the suggestion presented by this view.
  NSUInteger _suggestionIndex;

  // The total number of suggestion labels in the FormSuggestionView parent.
  NSUInteger _numberOfSuggestions;

  // The maximum width constraint for this view.
  // (applies to address and credit card suggestions only).
  NSLayoutConstraint* _widthConstraint;

  // The accessory trailing view of the FormSuggestionView parent view.
  UIView* _accessoryTrailingView;
}

#pragma mark - Public

- (id)initWithSuggestion:(FormSuggestion*)suggestion
                    index:(NSUInteger)index
      numberOfSuggestions:(NSUInteger)numberOfSuggestions
    accessoryTrailingView:(UIView*)accessoryTrailingView
                 delegate:(id<FormSuggestionLabelDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _suggestion = suggestion;
    _suggestionIndex = index;
    _numberOfSuggestions = numberOfSuggestions;
    _accessoryTrailingView = accessoryTrailingView;
    _delegate = delegate;

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
    stackView.axis = UILayoutConstraintAxisHorizontal;
    stackView.alignment = UIStackViewAlignmentCenter;
    stackView.layoutMarginsRelativeArrangement = YES;
    stackView.layoutMargins =
        UIEdgeInsetsMake(0, kBorderWidth, 0, kBorderWidth);
    stackView.spacing = kSpacing;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stackView];
    if (IsLiquidGlassEffectEnabled()) {
      AddSameConstraintsToSides(
          stackView, self,
          LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
      [stackView.heightAnchor constraintEqualToAnchor:self.heightAnchor]
          .active = YES;
    } else {
      AddSameConstraints(stackView, self);
    }

    if (suggestion.icon) {
      UIImageView* iconView = [[UIImageView alloc]
          initWithImage:[self resizeIconIfNecessary:suggestion.icon]];
      // If we have an icon, we want to see the icon and let the text be
      // truncated rather than expanding the text area and hiding the icon.
      [iconView
          setContentCompressionResistancePriority:UILayoutPriorityRequired
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
      [stackView addArrangedSubview:iconView];
    }

    NSString* suggestionText =
        IsPasswordSuggestion(suggestion)
            ? PasswordSuggestionDisplayText(suggestion.value)
            : suggestion.value;

    BOOL isTablet = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;

    // On phones, store the suggestion information in a stack view so that it
    // can be selectively truncated if necessary.
    if (!isTablet) {
      UIStackView* verticalStackView =
          [[UIStackView alloc] initWithArrangedSubviews:@[]];
      verticalStackView.axis = UILayoutConstraintAxisVertical;
      verticalStackView.alignment = UIStackViewAlignmentLeading;
      verticalStackView.layoutMarginsRelativeArrangement = YES;
      verticalStackView.layoutMargins =
          UIEdgeInsetsMake(0, suggestion.icon ? kSpacing : 0, 0, 0);
      verticalStackView.spacing = kVerticalSpacing;
      [stackView addArrangedSubview:verticalStackView];

      // Insert the next subviews vertically instead of horizontally.
      stackView = verticalStackView;
    }

    if (isTablet) {
      // On tablets, the stage manager causes an issue where an infinite loop
      // happens if we add stack views here, so we can't use more stack views
      // until the stage manager issue is fixed. As a workaround, on tablets,
      // since we don't need to truncate the suggestion text, the stack views
      // can be replaced by a single attributed string to present the data the
      // same way without having to rely on a stack of UILabel objects, which,
      // on the plus side, might actually be more light weight in the end.
      [stackView addArrangedSubview:AttributedTextLabel(
                                        suggestionText, suggestion.minorValue,
                                        suggestion.displayDescription,
                                        suggestion.icon)];
    } else {
      // Format the suggestion information using a stack view so that each piece
      // of information can be truncated individually when truncation is needed.
      NSArray<UIView*>* views = TextViews(suggestionText, suggestion.minorValue,
                                          suggestion.displayDescription,
                                          [self isCreditCardSuggestion]);
      for (UIView* view in views) {
        [stackView addArrangedSubview:view];
      }
    }

    [self setBackgroundColor:[self customBackgroundColor]];
    if (IsLiquidGlassEffectEnabled()) {
      [self setOpaque:NO];
    }

    [self setClipsToBounds:YES];
    [self setUserInteractionEnabled:YES];
    [self setIsAccessibilityElement:YES];
    [self
        setAccessibilityLabel:AccessibilityLabel(
                                  suggestionText, suggestion.displayDescription,
                                  suggestion.type ==
                                      SuggestionType::kBackupPasswordEntry)];
    [self
        setAccessibilityValue:l10n_util::GetNSStringF(
                                  IDS_IOS_AUTOFILL_SUGGESTION_INDEX_VALUE,
                                  base::NumberToString16(index + 1),
                                  base::NumberToString16(numberOfSuggestions))];
    [self
        setAccessibilityIdentifier:kFormSuggestionLabelAccessibilityIdentifier];

    // On phones, set a maximum width to save space on the keyboard accessory.
    if (!isTablet) {
      CGFloat maximumWidth = [self maximumWidth];
      if (maximumWidth < CGFLOAT_MAX) {
        _widthConstraint =
            [self.widthAnchor constraintLessThanOrEqualToConstant:maximumWidth];
        _widthConstraint.active = YES;
      }
    }
  }

  return self;
}

- (FormSuggestion*)suggestion {
  return _suggestion;
}

- (NSUInteger)suggestionIndex {
  return _suggestionIndex;
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  [self setCornerRadius:[self cornerRadius]];
  if (!IsLiquidGlassEffectEnabled()) {
    self.layer.shadowRadius = kShadowRadius;
    self.layer.shadowOffset = CGSizeMake(0, kShadowVerticalOffset);
    self.layer.shadowOpacity = kShadowOpacity;
    self.layer.shadowColor =
        [UIColor colorNamed:kBackgroundShadowColor].CGColor;
    self.layer.masksToBounds = NO;
  }
  if (_widthConstraint) {
    _widthConstraint.constant = [self maximumWidth];
  }
}

#pragma mark - UIResponder

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  UIColor* highlightColor = [UIColor colorNamed:kGrey300Color];
  if (IsLiquidGlassEffectEnabled()) {
    highlightColor =
        [highlightColor colorWithAlphaComponent:kHighlightColorAlpha];
  }
  [self setBackgroundColor:highlightColor];
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  // No op. This method implementation is needed per the UIResponder docs.
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[self customBackgroundColor]];
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  [self setBackgroundColor:[self customBackgroundColor]];

  // Don't count touches ending outside the view as as taps.
  CGPoint locationInView = [touches.anyObject locationInView:self];
  if (CGRectContainsPoint(self.bounds, locationInView)) {
    [_delegate didTapFormSuggestionLabel:self];
  }
}

#pragma mark - Private

// Sets the corner radius. Can be dymamic if the liquid glass effect is enabled.
- (void)setCornerRadius:(CGFloat)cornerRadius {
  if (IsLiquidGlassEffectEnabled()) {
    if (@available(iOS 26, *)) {
      self.cornerConfiguration = [UICornerConfiguration
          configurationWithRadius:
              [UICornerRadius
                  containerConcentricRadiusWithMinimum:[self cornerRadius]]];
      return;
    }
  }
  self.layer.cornerRadius = [self cornerRadius];
}

// Color of the suggestion chips.
- (UIColor*)customBackgroundColor {
  return IsLiquidGlassEffectEnabled() ? UIColor.clearColor
                                      : [UIColor colorNamed:kBackgroundColor];
}

// Corner radius of the suggestion chips.
- (CGFloat)cornerRadius {
  return kCornerRadius;
}

- (CGFloat)borderWidth {
  return kBorderWidth;
}

// Returns whether this label is for a credit card suggestion.
- (BOOL)isCreditCardSuggestion {
  return (_suggestion.type == SuggestionType::kCreditCardEntry) ||
         (_suggestion.type == SuggestionType::kVirtualCreditCardEntry);
}

// Resize the icon if it's a credit card icon which requires an upscaling.
- (UIImage*)resizeIconIfNecessary:(UIImage*)icon {
  if ([self isCreditCardSuggestion] && icon && icon.size.width > 0 &&
      icon.size.width < kSuggestionIconWidth) {
    // For a simple image resize, we can keep the same underlying image
    // and only adjust the ratio.
    CGFloat ratio = icon.size.width / kSuggestionIconWidth;
    icon = [UIImage imageWithCGImage:[icon CGImage]
                               scale:icon.scale * ratio
                         orientation:icon.imageOrientation];
  }
  return icon;
}

// Computes the suggestion label's maximum width.
// Returns CGFLOAT_MAX if there's no maximum width.
- (CGFloat)maximumWidth {
  CGFloat maxWidth = CGFLOAT_MAX;
  // Using the screen width because the `window` member is nil at the moment of
  // setting up the label's width anchor.
  CGSize windowSize = [[UIScreen mainScreen] bounds].size;
  CGFloat portraitScreenWidth = MIN(windowSize.width, windowSize.height);
  switch (_suggestion.type) {
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry: {
      // Max width is just enough to show half of the credit card icon on the
      // 2nd suggestion, in portrait mode.
      CGFloat staticButtonsWidth = _accessoryTrailingView.frame.size.width;
      maxWidth = (portraitScreenWidth - staticButtonsWidth) -
                 kHalfCreditCardIconOffset;
    } break;
    case SuggestionType::kAddressEntry:
      if (_numberOfSuggestions > 1) {
        // Max width is half width, in portrait mode.
        maxWidth = portraitScreenWidth * 0.5;
      }
      break;
    default:
      break;
  }
  return maxWidth;
}

@end
