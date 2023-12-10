// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/autocomplete_match_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_type.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

NSString* const kAutocompleteMatchCellReuseIdentifier =
    @"AutocompleteMatchCell";

namespace {

/// Size of the default match icon.
const CGFloat kDefaultMatchIconSize = 17.0;
/// Leading and trailing padding for the content view.
const CGFloat kContentViewHorizontalPadding = 10.0;
/// Top and bottom padding for the content view.
const CGFloat kContentViewVerticalPadding = 00.0;

}  // namespace

@implementation AutocompleteMatchCell {
  /// Label for AutocompleteMatch.provider.type.
  UILabel* _providerTypeLabel;
  /// Label for AutocompleteMatch.type.
  UILabel* _matchTypeLabel;

  /// Label for AutocompleteMatchFormatter.text.
  UILabel* _textLabel;
  /// Label for AutocompleteMatch.relevance.
  UILabel* _relevanceLabel;
  /// Icon for AutocompleteMatch.allowed_to_be_default_match.
  UIImageView* _defaultMatchImageView;

  /// Label for AutocompleteMatch.description.
  UILabel* _descriptionLabel;
  /// Label for AutocompleteMatch.destination_url.
  UILabel* _URLLabel;
  /// Label for AutocompleteMatch.additional_info.
  UILabel* _additionalInfoLabel;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // First row for types.
    _matchTypeLabel = [[UILabel alloc] init];
    _matchTypeLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _matchTypeLabel.textColor = UIColor.systemGrayColor;

    _providerTypeLabel = [[UILabel alloc] init];
    _providerTypeLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _providerTypeLabel.textAlignment = NSTextAlignmentRight;
    _providerTypeLabel.textColor = UIColor.systemGreenColor;

    UIStackView* firstRowStackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _matchTypeLabel, _providerTypeLabel ]];
    firstRowStackView.translatesAutoresizingMaskIntoConstraints = NO;

    // Second row for text, relevance and canBeDefaultMatch.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;

    _relevanceLabel = [[UILabel alloc] init];
    _relevanceLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _relevanceLabel.textAlignment = NSTextAlignmentRight;

    _defaultMatchImageView = [[UIImageView alloc] init];
    _defaultMatchImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_defaultMatchImageView.widthAnchor
          constraintEqualToConstant:kDefaultMatchIconSize],
      [_defaultMatchImageView.heightAnchor
          constraintEqualToConstant:kDefaultMatchIconSize]
    ]];

    UIStackView* secondRowStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          _textLabel, _relevanceLabel, _defaultMatchImageView
        ]];
    secondRowStackView.translatesAutoresizingMaskIntoConstraints = NO;

    // Additional rows
    _descriptionLabel = [[UILabel alloc] init];
    _descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;

    _URLLabel = [[UILabel alloc] init];
    _URLLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _URLLabel.textColor = UIColor.darkGrayColor;

    _additionalInfoLabel = [[UILabel alloc] init];
    _additionalInfoLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _additionalInfoLabel.numberOfLines = 0;

    // Content view.
    UIStackView* contentStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          firstRowStackView, secondRowStackView, _descriptionLabel, _URLLabel,
          _additionalInfoLabel
        ]];
    contentStackView.translatesAutoresizingMaskIntoConstraints = NO;
    contentStackView.axis = UILayoutConstraintAxisVertical;

    [self.contentView addSubview:contentStackView];
    AddSameConstraintsWithInsets(
        contentStackView, self.contentView,
        NSDirectionalEdgeInsetsMake(
            kContentViewVerticalPadding, kContentViewHorizontalPadding,
            kContentViewVerticalPadding, kContentViewHorizontalPadding));
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _providerTypeLabel.text = nil;
  _matchTypeLabel.text = nil;
  _textLabel.text = nil;
  _relevanceLabel.text = nil;
  _defaultMatchImageView.image = nil;
  _descriptionLabel.text = nil;
  _URLLabel.text = nil;
  _additionalInfoLabel.text = nil;
}

- (void)setupWithAutocompleteMatchFormatter:
            (AutocompleteMatchFormatter*)matchFormatter
                           showProviderType:(BOOL)shouldShowProviderType {
  const AutocompleteMatch& match = matchFormatter.autocompleteMatch;

  _providerTypeLabel.text = base::SysUTF8ToNSString(
      AutocompleteProvider::TypeToString(match.provider->type()));
  _providerTypeLabel.hidden = !shouldShowProviderType;

  _matchTypeLabel.text =
      base::SysUTF8ToNSString(AutocompleteMatchType::ToString(match.type));
  _textLabel.attributedText = matchFormatter.text;
  _relevanceLabel.text = [NSString stringWithFormat:@"%d", match.relevance];
  _descriptionLabel.text = base::SysUTF16ToNSString(match.description);
  _URLLabel.text = base::SysUTF8ToNSString(match.destination_url.spec());

  if (match.allowed_to_be_default_match) {
    UIImage* checkmarkSymbol =
        SymbolWithPalette(DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                                     kDefaultMatchIconSize),
                          @[ UIColor.systemGreenColor ]);
    _defaultMatchImageView.image = checkmarkSymbol;
  } else {
    UIImage* xmarkSymbol =
        SymbolWithPalette(DefaultSymbolWithPointSize(kXMarkCircleFillSymbol,
                                                     kDefaultMatchIconSize),
                          @[ UIColor.redColor ]);
    _defaultMatchImageView.image = xmarkSymbol;
  }

  NSMutableAttributedString* additionalInfoString =
      [[NSMutableAttributedString alloc] init];
  for (const auto& info : match.additional_info) {
    NSString* infoName = base::SysUTF8ToNSString(info.first + ": ");
    NSString* infoValue = base::SysUTF8ToNSString(info.second + "\n");
    NSAttributedString* attributedName = [[NSAttributedString alloc]
        initWithString:infoName
            attributes:@{
              NSForegroundColorAttributeName : UIColor.systemRedColor,
            }];
    NSAttributedString* attributedValue = [[NSAttributedString alloc]
        initWithString:infoValue
            attributes:@{
              NSForegroundColorAttributeName : UIColor.systemIndigoColor,
            }];

    [additionalInfoString appendAttributedString:attributedName];
    [additionalInfoString appendAttributedString:attributedValue];
  }
  _additionalInfoLabel.attributedText = additionalInfoString;
}

@end
