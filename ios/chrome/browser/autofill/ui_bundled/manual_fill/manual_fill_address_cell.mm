// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_address_cell.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface ManualFillAddressItem ()

// The content delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

// The address/profile for this item.
@property(nonatomic, readonly) ManualFillAddress* address;

// The UIActions that should be available from the cell's overflow menu button.
@property(nonatomic, strong) NSArray<UIAction*>* menuActions;

// The cell's accessibility label. Indicates the index at which the address
// represented by this item is positioned in the list of addresses to show.
@property(nonatomic, strong) NSString* cellIndexAccessibilityLabel;

@end

@implementation ManualFillAddressItem {
  // The 0-based index at which the address is in the list of addresses to show.
  NSInteger _cellIndex;

  // If `YES`, autofill button is shown for the item.
  BOOL _showAutofillFormButton;
}

- (instancetype)initWithAddress:(ManualFillAddress*)address
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _contentInjector = contentInjector;
    _address = address;
    _menuActions = menuActions;
    _cellIndex = cellIndex;
    _cellIndexAccessibilityLabel = cellIndexAccessibilityLabel;
    _showAutofillFormButton = showAutofillFormButton;
    self.cellClass = [ManualFillAddressCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillAddressCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithAddress:self.address
                  contentInjector:self.contentInjector
                      menuActions:self.menuActions
                        cellIndex:_cellIndex
      cellIndexAccessibilityLabel:self.cellIndexAccessibilityLabel
           showAutofillFormButton:_showAutofillFormButton];
}
@end

@interface ManualFillAddressCell ()

// The label with the line1 -- line2.
// TODO(crbug.com/326398845): Remove property once the Keyboard Accessory
// Upgrade feature has launched both on iPhone and iPad.
@property(nonatomic, strong) UILabel* addressLabel;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// A button showing the address associated first name.
@property(nonatomic, strong) UIButton* firstNameButton;

// A button showing the address associated middle name or initial.
@property(nonatomic, strong) UIButton* middleNameButton;

// A button showing the address associated last name.
@property(nonatomic, strong) UIButton* lastNameButton;

// A button showing the company name.
@property(nonatomic, strong) UIButton* companyButton;

// A button showing the address line 1.
@property(nonatomic, strong) UIButton* line1Button;

// An optional button showing the address line 2.
@property(nonatomic, strong) UIButton* line2Button;

// A button showing zip code.
@property(nonatomic, strong) UIButton* zipButton;

// A button showing city.
@property(nonatomic, strong) UIButton* cityButton;

// A button showing state/province.
@property(nonatomic, strong) UIButton* stateButton;

// A button showing country.
@property(nonatomic, strong) UIButton* countryButton;

// A button showing a phone number.
@property(nonatomic, strong) UIButton* phoneNumberButton;

// A button showing an email address.
@property(nonatomic, strong) UIButton* emailAddressButton;

// The content delegate for this item.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// The menu button displayed in the top right corner of the cell.
@property(nonatomic, strong) UIButton* overflowMenuButton;

// Button to autofill the current form with the address' data.
@property(nonatomic, strong) UIButton* autofillFormButton;

// The address data for this cell.
@property(nonatomic, weak) ManualFillAddress* address;

@end

namespace {

// Vertical spacing between the top of the cell and the top of the overflow
// menu.
constexpr CGFloat kOverflowMenuButtonTopSpacing = 14;

}  // namespace

@implementation ManualFillAddressCell {
  // The 0-based index at which the address is in the list of addresses to show.
  NSInteger _cellIndex;

  // If `YES`, autofill button is shown for the cell.
  BOOL _showAutofillFormButton;

  // If `YES`, the first row of chip buttons was laid out horizontally. This is
  // useful to know whether or not the overflow menu needs to be considered when
  // laying out a group of chip buttons horizontally as this menu is presented
  // on the trailing side of the first row.
  BOOL _firstChipRowHasBeenLaidOut;

  // Current width of the cell's layout guide. Used to know whenever it changes
  // and when the views need to be re-arranged.
  CGFloat _layoutGuideWidth;
}

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [self resetDynamicContraints];

  self.addressLabel.text = @"";
  [self.firstNameButton setTitle:@"" forState:UIControlStateNormal];
  [self.middleNameButton setTitle:@"" forState:UIControlStateNormal];
  [self.lastNameButton setTitle:@"" forState:UIControlStateNormal];
  [self.companyButton setTitle:@"" forState:UIControlStateNormal];
  [self.line1Button setTitle:@"" forState:UIControlStateNormal];
  [self.line2Button setTitle:@"" forState:UIControlStateNormal];
  [self.zipButton setTitle:@"" forState:UIControlStateNormal];
  [self.cityButton setTitle:@"" forState:UIControlStateNormal];
  [self.stateButton setTitle:@"" forState:UIControlStateNormal];
  [self.countryButton setTitle:@"" forState:UIControlStateNormal];
  [self.phoneNumberButton setTitle:@"" forState:UIControlStateNormal];
  [self.emailAddressButton setTitle:@"" forState:UIControlStateNormal];
  self.contentInjector = nil;
  self.address = nil;
  _showAutofillFormButton = NO;
}

- (void)setUpWithAddress:(ManualFillAddress*)address
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton {
  _cellIndex = cellIndex;
  _showAutofillFormButton = showAutofillFormButton;
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentInjector = contentInjector;
  self.address = address;

  if (menuActions && menuActions.count) {
    self.overflowMenuButton.menu = [UIMenu menuWithChildren:menuActions];
    self.overflowMenuButton.hidden = NO;
  } else {
    self.overflowMenuButton.hidden = YES;
  }

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    GiveAccessibilityContextToCellAndButton(self, self.overflowMenuButton,
                                            self.autofillFormButton,
                                            cellIndexAccessibilityLabel);
  }

  [self populateViewsWithAddress:address];
  [self arrangeViewsWithAddress:address];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGFloat width = self.layoutGuide.layoutFrame.size.width;
  if (self.contentView.subviews.count == 0 || width == _layoutGuideWidth) {
    return;
  }

  // Re-arrange the views when the width of the layout guide's frame changed to
  // make sure that the usage of the horizontal space is optimized.
  _layoutGuideWidth = width;
  [self resetDynamicContraints];
  [self.contentView invalidateIntrinsicContentSize];
  [self arrangeViewsWithAddress:self.address];
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.layoutGuide = AddLayoutGuideToContentView(
      self.contentView,
      /*cell_has_header=*/!IsKeyboardAccessoryUpgradeEnabled());

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    CreateGraySeparatorForContainer(self.contentView);
  }

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    self.addressLabel = CreateLabel();
    [self.contentView addSubview:self.addressLabel];
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.addressLabel ], self.layoutGuide);
  } else {
    self.overflowMenuButton = CreateOverflowMenuButton();
    [self.contentView addSubview:self.overflowMenuButton];
    [staticConstraints
        addObject:[self.overflowMenuButton.topAnchor
                      constraintEqualToAnchor:self.contentView.topAnchor
                                     constant:kOverflowMenuButtonTopSpacing]];
  }

  self.firstNameButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.firstNameButton];

  self.middleNameButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.middleNameButton];

  self.lastNameButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.lastNameButton];

  self.companyButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.companyButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.companyButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.line1Button =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line1Button];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.line1Button ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.line2Button =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line2Button];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.line2Button ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.zipButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.zipButton];

  self.cityButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.cityButton];

  self.stateButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.stateButton];

  self.countryButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.countryButton];

  self.phoneNumberButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.phoneNumberButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.phoneNumberButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.emailAddressButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.emailAddressButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.emailAddressButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.autofillFormButton = CreateAutofillFormButton();
  [self.contentView addSubview:self.autofillFormButton];
  [self.autofillFormButton addTarget:self
                              action:@selector(onAutofillFormButtonTapped)
                    forControlEvents:UIControlEventTouchUpInside];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.autofillFormButton ], self.layoutGuide);

  // Without this set, Voice Over will read the content vertically instead of
  // horizontally.
  self.contentView.shouldGroupAccessibilityChildren = YES;

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

// Adds each data item from the ManualFillAddress to its corresponding view when
// valid (when the data item is not empty). Otherwise, hides the corresponding
// view. Once populated, the views need to be layed out with the
// `arrangeViewsWithAddress` method.
- (void)populateViewsWithAddress:(ManualFillAddress*)address {
  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    NSString* blackText = nil;
    NSString* grayText = nil;

    // Top label is the address summary and fallbacks on city and email.
    if (address.line1.length) {
      blackText = address.line1;
      grayText = address.line2.length ? address.line2 : nil;
    } else if (address.line2.length) {
      blackText = address.line2;
    } else if (address.city.length) {
      blackText = address.city;
    } else if (address.emailAddress.length) {
      blackText = address.emailAddress;
    }

    NSMutableAttributedString* attributedString = nil;
    if (blackText.length) {
      attributedString = [[NSMutableAttributedString alloc]
          initWithString:blackText
              attributes:@{
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kTextPrimaryColor],
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
              }];
      if (grayText.length) {
        NSString* formattedGrayText =
            [NSString stringWithFormat:@" –– %@", grayText];
        NSDictionary* attributes = @{
          NSForegroundColorAttributeName :
              [UIColor colorNamed:kTextSecondaryColor],
          NSFontAttributeName :
              [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
        };
        NSAttributedString* grayAttributedString =
            [[NSAttributedString alloc] initWithString:formattedGrayText
                                            attributes:attributes];
        [attributedString appendAttributedString:grayAttributedString];
      }
    }

    if (attributedString) {
      self.addressLabel.attributedText = attributedString;
    }
  }

  bool showFirstName = address.firstName.length;
  bool showMiddleName = address.middleNameOrInitial.length;
  bool showLastName = address.lastName.length;

  // First name chip button.
  if (showFirstName) {
    [self setTitleAndAccessibilityLabelOfChip:self.firstNameButton
                                    withValue:address.firstName];
    self.firstNameButton.hidden = NO;
  } else {
    self.firstNameButton.hidden = YES;
  }

  // Middle name chip button.
  if (showMiddleName) {
    [self setTitleAndAccessibilityLabelOfChip:self.middleNameButton
                                    withValue:address.middleNameOrInitial];
    self.middleNameButton.hidden = NO;
  } else {
    self.middleNameButton.hidden = YES;
  }

  // Last name chip button.
  if (showLastName) {
    [self setTitleAndAccessibilityLabelOfChip:self.lastNameButton
                                    withValue:address.lastName];
    self.lastNameButton.hidden = NO;
  } else {
    self.lastNameButton.hidden = YES;
  }

  // Company line chip button.
  if (address.company.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.companyButton
                                    withValue:address.company];
    self.companyButton.hidden = NO;
  } else {
    self.companyButton.hidden = YES;
  }

  // Address line 1 chip button.
  if (address.line1.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.line1Button
                                    withValue:address.line1];
    self.line1Button.hidden = NO;
  } else {
    self.line1Button.hidden = YES;
  }

  // Address line 2 chip button.
  if (address.line2.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.line2Button
                                    withValue:address.line2];
    self.line2Button.hidden = NO;
  } else {
    self.line2Button.hidden = YES;
  }

  // City chip button.
  if (address.city.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.cityButton
                                    withValue:address.city];
    self.cityButton.hidden = NO;
  } else {
    self.cityButton.hidden = YES;
  }

  // State chip button.
  if (address.state.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.stateButton
                                    withValue:address.state];
    self.stateButton.hidden = NO;
  } else {
    self.stateButton.hidden = YES;
  }

  // ZIP code chip button.
  if (address.zip.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.zipButton
                                    withValue:address.zip];
    self.zipButton.hidden = NO;
  } else {
    self.zipButton.hidden = YES;
  }

  // Country chip button.
  if (address.country.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.countryButton
                                    withValue:address.country];
    self.countryButton.hidden = NO;
  } else {
    self.countryButton.hidden = YES;
  }

  // Phone number chip button.
  if (address.phoneNumber.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.phoneNumberButton
                                    withValue:address.phoneNumber];
    self.phoneNumberButton.hidden = NO;
  } else {
    self.phoneNumberButton.hidden = YES;
  }

  // Email address chip button.
  if (address.emailAddress.length) {
    [self setTitleAndAccessibilityLabelOfChip:self.emailAddressButton
                                    withValue:address.emailAddress];
    self.emailAddressButton.hidden = NO;
  } else {
    self.emailAddressButton.hidden = YES;
  }
}

// Creates and activates the dynamic constraints that are depending on the
// address data.
- (void)arrangeViewsWithAddress:(ManualFillAddress*)address {
  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;

  if (!IsKeyboardAccessoryUpgradeEnabled() &&
      self.addressLabel.attributedText.length) {
    AddViewToVerticalLeadViews(self.addressLabel,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
  }

  self.dynamicConstraints = [[NSMutableArray alloc] init];

  _firstChipRowHasBeenLaidOut = NO;

  // First, middle and last names are presented on the same line when possible.
  NSMutableArray<UIView*>* nameLineViews = [[NSMutableArray alloc] init];

  // Holds the chip buttons related to the name that are vertical leads.
  NSMutableArray<UIView*>* nameGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // First name chip button.
  if (address.firstName.length) {
    [nameLineViews addObject:self.firstNameButton];
  }

  // Middle name chip button.
  if (address.middleNameOrInitial.length) {
    [nameLineViews addObject:self.middleNameButton];
  }

  // Last name chip button.
  if (address.lastName.length) {
    [nameLineViews addObject:self.lastNameButton];
  }

  [self layViewsHorizontally:nameLineViews
           verticalLeadViews:nameGroupVerticalLeadChips];

  // Holds the chip buttons related to the company name that are vertical leads.
  NSMutableArray<UIView*>* companyGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Company line chip button.
  if (address.company.length) {
    [companyGroupVerticalLeadChips addObject:self.companyButton];
  }

  // Holds the chip buttons related to the address that are vertical leads.
  NSMutableArray<UIView*>* addressGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Address line 1 chip button.
  if (address.line1.length) {
    [addressGroupVerticalLeadChips addObject:self.line1Button];
  }

  // Address line 2 chip button.
  if (address.line2.length) {
    [addressGroupVerticalLeadChips addObject:self.line2Button];
  }

  // City, state, ZIP code and country are presented on the same line when
  // possible. Used when the Keyboard Accessory Upgrade feature is enabled.
  NSMutableArray<UIView*>* cityStateZipCountryLineViews =
      [[NSMutableArray alloc] init];

  // ZIP code and city are presented on the same line when possible. Used when
  // the Keyboard Accessory Upgrade feature is disabled.
  NSMutableArray<UIView*>* zipCityLineViews = [[NSMutableArray alloc] init];
  // State and country are presented on the same line when possible. Used when
  // the Keyboard Accessory Upgrade feature is disabled.
  NSMutableArray<UIView*>* stateCountryLineViews =
      [[NSMutableArray alloc] init];

  // City chip button.
  if (address.city.length) {
    [IsKeyboardAccessoryUpgradeEnabled()
            ? cityStateZipCountryLineViews
            : zipCityLineViews addObject:self.cityButton];
  }

  // State chip button.
  if (address.state.length) {
    [IsKeyboardAccessoryUpgradeEnabled()
            ? cityStateZipCountryLineViews
            : stateCountryLineViews addObject:self.stateButton];
  }

  // ZIP code chip button.
  if (address.zip.length) {
    IsKeyboardAccessoryUpgradeEnabled()
        ? [cityStateZipCountryLineViews addObject:self.zipButton]
        : [zipCityLineViews insertObject:self.zipButton atIndex:0];
  }

  // Country chip button.
  if (address.country.length) {
    [IsKeyboardAccessoryUpgradeEnabled()
            ? cityStateZipCountryLineViews
            : stateCountryLineViews addObject:self.countryButton];
  }

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    [self layViewsHorizontally:cityStateZipCountryLineViews
             verticalLeadViews:addressGroupVerticalLeadChips];
  } else {
    [self layViewsHorizontally:zipCityLineViews
             verticalLeadViews:addressGroupVerticalLeadChips];
    [self layViewsHorizontally:stateCountryLineViews
             verticalLeadViews:addressGroupVerticalLeadChips];
  }

  // Holds the chip buttons related to the contact info that are vertical leads.
  NSMutableArray<UIView*>* contactInfoGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Phone number chip button.
  if (address.phoneNumber.length) {
    [contactInfoGroupVerticalLeadChips addObject:self.phoneNumberButton];
  }

  // Email address chip button.
  if (address.emailAddress.length) {
    [contactInfoGroupVerticalLeadChips addObject:self.emailAddressButton];
  }

  AddChipGroupsToVerticalLeadViews(
      @[
        nameGroupVerticalLeadChips, companyGroupVerticalLeadChips,
        addressGroupVerticalLeadChips, contactInfoGroupVerticalLeadChips
      ],
      verticalLeadViews);

  if (_showAutofillFormButton) {
    CHECK(IsKeyboardAccessoryUpgradeEnabled());
    AddViewToVerticalLeadViews(self.autofillFormButton,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
    self.autofillFormButton.hidden = NO;
  } else {
    self.autofillFormButton.hidden = YES;
  }

  // Set and activate constraints.
  AppendVerticalConstraintsSpacingForViews(self.dynamicConstraints,
                                           verticalLeadViews, self.layoutGuide);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

// Lays the given `views` horizontally and evaluates whether the overflow menu
// should be considered or not as part of the row.
- (void)layViewsHorizontally:(NSArray<UIView*>*)views
           verticalLeadViews:(NSMutableArray<UIView*>*)verticalLeadViews {
  if (views.count <= 0) {
    return;
  }

  UIView* trailingView;
  if (!_firstChipRowHasBeenLaidOut) {
    trailingView =
        IsKeyboardAccessoryUpgradeEnabled() ? self.overflowMenuButton : nil;
    _firstChipRowHasBeenLaidOut = YES;
  }

  LayViewsHorizontallyWhenPossible(views, self.layoutGuide,
                                   self.dynamicConstraints, verticalLeadViews,
                                   trailingView);
}

// Deactivates and removes the dynamic constraints.
- (void)resetDynamicContraints {
  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];
}

- (void)userDidTapAddressInfo:(UIButton*)sender {
  const char* metricsAction = nullptr;
  if (sender == self.firstNameButton) {
    metricsAction = "ManualFallback_Profiles_SelectFirstName";
  } else if (sender == self.middleNameButton) {
    metricsAction = "ManualFallback_Profiles_SelectMiddleName";
  } else if (sender == self.lastNameButton) {
    metricsAction = "ManualFallback_Profiles_SelectLastName";
  } else if (sender == self.companyButton) {
    metricsAction = "ManualFallback_Profiles_Company";
  } else if (sender == self.line1Button) {
    metricsAction = "ManualFallback_Profiles_Address1";
  } else if (sender == self.line2Button) {
    metricsAction = "ManualFallback_Profiles_Address2";
  } else if (sender == self.zipButton) {
    metricsAction = "ManualFallback_Profiles_Zip";
  } else if (sender == self.cityButton) {
    metricsAction = "ManualFallback_Profiles_City";
  } else if (sender == self.stateButton) {
    metricsAction = "ManualFallback_Profiles_State";
  } else if (sender == self.countryButton) {
    metricsAction = "ManualFallback_Profiles_Country";
  } else if (sender == self.phoneNumberButton) {
    metricsAction = "ManualFallback_Profiles_PhoneNumber";
  } else if (sender == self.emailAddressButton) {
    metricsAction = "ManualFallback_Profiles_EmailAddress";
  }
  DCHECK(metricsAction);
  base::RecordAction(base::UserMetricsAction(metricsAction));

  [self.contentInjector userDidPickContent:sender.titleLabel.text
                             passwordField:NO
                             requiresHTTPS:NO];
}

// Called when the "Autofill Form" button is tapped. Fills the current form with
// the address' data.
- (void)onAutofillFormButtonTapped {
  base::UmaHistogramSparse(
      "Autofill.UserAcceptedSuggestionAtIndex.Address.ManualFallback",
      _cellIndex);
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Profiles_SuggestionAccepted"));

  FormSuggestion* suggestion = [FormSuggestion
              suggestionWithValue:nil
                       minorValue:nil
               displayDescription:nil
                             icon:nil
                             type:autofill::SuggestionType::kAddressEntry
                backendIdentifier:[self.address GUID]
      fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                   requiresReauth:NO
       acceptanceA11yAnnouncement:
           base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
               IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM))];

  [self.contentInjector autofillFormWithSuggestion:suggestion
                                           atIndex:_cellIndex];
}

// Sets the title and accessbility label of the given `chipButton` using the
// given `value`.
- (void)setTitleAndAccessibilityLabelOfChip:(UIButton*)chipButton
                                  withValue:(NSString*)value {
  [chipButton setTitle:value forState:UIControlStateNormal];
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    chipButton.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(value));
  }
}

@end
