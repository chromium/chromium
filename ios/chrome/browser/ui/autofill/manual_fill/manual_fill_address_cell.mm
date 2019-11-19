// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_address_cell.h"

#include "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillAddressItem ()

// The content delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

// The address/profile for this item.
@property(nonatomic, readonly) ManualFillAddress* address;

@end

@implementation ManualFillAddressItem

- (instancetype)initWithAddress:(ManualFillAddress*)address
                contentInjector:(id<ManualFillContentInjector>)contentInjector {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _contentInjector = contentInjector;
    _address = address;
    self.cellClass = [ManualFillAddressCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillAddressCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithAddress:self.address contentInjector:self.contentInjector];
}

@end

@interface ManualFillAddressCell ()

// Separator line between cells, if needed.
@property(nonatomic, strong) UIView* grayLine;

// The label with the line1 -- line2.
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

@end

@implementation ManualFillAddressCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

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
}

- (void)setUpWithAddress:(ManualFillAddress*)address
         contentInjector:(id<ManualFillContentInjector>)contentInjector {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentInjector = contentInjector;

  NSMutableArray<UIView*>* verticalLeadViews = [[NSMutableArray alloc] init];
  UIView* guide = self.grayLine;

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
    [verticalLeadViews addObject:self.addressLabel];
  }

  self.dynamicConstraints = [[NSMutableArray alloc] init];

  BOOL largeTypes = UIContentSizeCategoryIsAccessibilityCategory(
      UIScreen.mainScreen.traitCollection.preferredContentSizeCategory);

  // Name line, first middle and last.
  NSMutableArray<UIView*>* nameLineViews = [[NSMutableArray alloc] init];

  bool showFirstName = address.firstName.length;
  bool showMiddleName = address.middleNameOrInitial.length;
  bool showLastName = address.lastName.length;

  if (showFirstName) {
    [self.firstNameButton setTitle:address.firstName
                          forState:UIControlStateNormal];
    [nameLineViews addObject:self.firstNameButton];
    self.firstNameButton.hidden = NO;
  } else {
    self.firstNameButton.hidden = YES;
  }

  if (showMiddleName) {
    [self.middleNameButton setTitle:address.middleNameOrInitial
                           forState:UIControlStateNormal];
    [nameLineViews addObject:self.middleNameButton];
    self.middleNameButton.hidden = NO;
  } else {
    self.middleNameButton.hidden = YES;
  }

  if (showLastName) {
    [self.lastNameButton setTitle:address.lastName
                         forState:UIControlStateNormal];
    [nameLineViews addObject:self.lastNameButton];
    self.lastNameButton.hidden = NO;
  } else {
    self.lastNameButton.hidden = YES;
  }

  [self layMultipleViews:nameLineViews
          withLargeTypes:largeTypes
                 onGuide:guide
      addFirstLineViewTo:verticalLeadViews];

  // Company line.
  if (address.company.length) {
    [self.companyButton setTitle:address.company forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.companyButton];
    self.companyButton.hidden = NO;
  } else {
    self.companyButton.hidden = YES;
  }

  // Address line 1.
  if (address.line1.length) {
    [self.line1Button setTitle:address.line1 forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.line1Button];
    self.line1Button.hidden = NO;
  } else {
    self.line1Button.hidden = YES;
  }

  // Address line 2.
  if (address.line2.length) {
    [self.line2Button setTitle:address.line2 forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.line2Button];
    self.line2Button.hidden = NO;
  } else {
    self.line2Button.hidden = YES;
  }

  // Zip and city line.
  NSMutableArray<UIView*>* zipCityLineViews = [[NSMutableArray alloc] init];

  if (address.zip.length) {
    [self.zipButton setTitle:address.zip forState:UIControlStateNormal];
    [zipCityLineViews addObject:self.zipButton];
    self.zipButton.hidden = NO;
  } else {
    self.zipButton.hidden = YES;
  }

  if (address.city.length) {
    [self.cityButton setTitle:address.city forState:UIControlStateNormal];
    [zipCityLineViews addObject:self.cityButton];
    self.cityButton.hidden = NO;
  } else {
    self.cityButton.hidden = YES;
  }

  [self layMultipleViews:zipCityLineViews
          withLargeTypes:largeTypes
                 onGuide:guide
      addFirstLineViewTo:verticalLeadViews];

  // State and country line.
  NSMutableArray<UIView*>* stateCountryLineViews =
      [[NSMutableArray alloc] init];

  if (address.state.length) {
    [self.stateButton setTitle:address.state forState:UIControlStateNormal];
    [stateCountryLineViews addObject:self.stateButton];
    self.stateButton.hidden = NO;
  } else {
    self.stateButton.hidden = YES;
  }

  if (address.country.length) {
    [self.countryButton setTitle:address.country forState:UIControlStateNormal];
    [stateCountryLineViews addObject:self.countryButton];
    self.countryButton.hidden = NO;
  } else {
    self.countryButton.hidden = YES;
  }

  [self layMultipleViews:stateCountryLineViews
          withLargeTypes:largeTypes
                 onGuide:guide
      addFirstLineViewTo:verticalLeadViews];

  if (address.phoneNumber.length) {
    [self.phoneNumberButton setTitle:address.phoneNumber
                            forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.phoneNumberButton];
    self.phoneNumberButton.hidden = NO;
  } else {
    self.phoneNumberButton.hidden = YES;
  }

  if (address.emailAddress.length) {
    [self.emailAddressButton setTitle:address.emailAddress
                             forState:UIControlStateNormal];
    [verticalLeadViews addObject:self.emailAddressButton];
    self.emailAddressButton.hidden = NO;
  } else {
    self.emailAddressButton.hidden = YES;
  }

  AppendVerticalConstraintsSpacingForViews(self.dynamicConstraints,
                                           verticalLeadViews, self.contentView);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

#pragma mark - Private

// Dynamically lay givens |views| on |guide|, adding first view of every
// generated line to |addFirstLineViewTo|. If |largeTypes| is true, fields are
// laid out vertically one per line, otherwise horizontally on one line.
// Constraints are added to |self.dynamicConstraints| property.
- (void)layMultipleViews:(NSArray<UIView*>*)views
          withLargeTypes:(BOOL)largeTypes
                 onGuide:(UIView*)guide
      addFirstLineViewTo:(NSMutableArray<UIView*>*)verticalLeadViews {
  if (views.count == 0)
    return;
  if (largeTypes) {
    for (UIView* view in views) {
      AppendHorizontalConstraintsForViews(
          self.dynamicConstraints, @[ view ], guide, kChipsHorizontalMargin,
          AppendConstraintsHorizontalEqualOrSmallerThanGuide);
      [verticalLeadViews addObject:view];
    }
  } else {
    AppendHorizontalConstraintsForViews(
        self.dynamicConstraints, views, guide, kChipsHorizontalMargin,
        AppendConstraintsHorizontalSyncBaselines |
            AppendConstraintsHorizontalEqualOrSmallerThanGuide);
    [verticalLeadViews addObject:views.firstObject];
  }
}

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  self.grayLine = CreateGraySeparatorForContainer(self.contentView);

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.addressLabel = CreateLabel();
  [self.contentView addSubview:self.addressLabel];
  AppendHorizontalConstraintsForViews(staticConstraints, @[ self.addressLabel ],
                                      self.contentView,
                                      kButtonHorizontalMargin);

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
      staticConstraints, @[ self.companyButton ], self.grayLine,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.line1Button =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line1Button];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.line1Button ], self.grayLine,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.line2Button =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.line2Button];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.line2Button ], self.grayLine,
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
      staticConstraints, @[ self.phoneNumberButton ], self.grayLine,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.emailAddressButton =
      CreateChipWithSelectorAndTarget(@selector(userDidTapAddressInfo:), self);
  [self.contentView addSubview:self.emailAddressButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.emailAddressButton ], self.grayLine,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  // Without this set, Voice Over will read the content vertically instead of
  // horizontally.
  self.contentView.shouldGroupAccessibilityChildren = YES;

  [NSLayoutConstraint activateConstraints:staticConstraints];
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

@end
