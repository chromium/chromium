// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_view_controller.h"

#import <vector>

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_constants.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_mutator.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_conversion_view_controller_delegate.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_type_cell.h"
#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_type_value_field_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/unit_conversion/unit_conversion_api.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The size of the `Report an issue` button icon.
const CGFloat kReportAnIssueIconSize = 15;

// The trailing and leading offsets of the `Report an issue` button.
const CGFloat kReportAnIssueButtonTrailingOffset = 8;
const CGFloat kReportAnIssueButtonLeadingOffset = 8;

// Table's top inset and header/footer heights of its sections.
const CGFloat kSectionHeaderHeight = 8;
const CGFloat kSectionFooterHeight = 8;

// Size constraint of the nav bar close button.
const CGFloat kCloseButtonIconSize = 30;

// Size constraint of the title button.
const CGFloat kTitleIconSize = 15;

// The padding between the icon and the text of the `Report an issue` button.
const CGFloat kReportAnIssueButtonPadding = 4;

// The padding between the icon and the text of the unit title button.
const CGFloat kUnitTitlePadding = 4;

// Cells identifiers.
NSString* kUnitTypeCellIdentifier = @"UnitTypeCell";
NSString* kUnitTypeFieldCellIdentifier = @"UnitTypeValueFieldCell";

// Source and target sections indexes.
const NSInteger kSourceSection = 0;
const NSInteger kTargetSection = 1;

// Unit type and unit value field rows indexes.
const NSInteger kUnitTypeRow = 0;
const NSInteger kUnitValueFieldRow = 1;

// The height offset to add to the computed preferredContentSize's height.
const CGFloat kTableViewHeightOffset = 16;

// Returns the `UnitType` group for the given `unit`.
ios::provider::UnitType TypeByUnit(NSUnit* unit) {
  if ([unit isKindOfClass:[NSUnitArea class]]) {
    return ios::provider::kUnitTypeArea;
  }
  if ([unit isKindOfClass:[NSUnitInformationStorage class]]) {
    return ios::provider::kUnitTypeInformationStorage;
  }
  if ([unit isKindOfClass:[NSUnitLength class]]) {
    return ios::provider::kUnitTypeLength;
  }
  if ([unit isKindOfClass:[NSUnitMass class]]) {
    return ios::provider::kUnitTypeMass;
  }
  if ([unit isKindOfClass:[NSUnitSpeed class]]) {
    return ios::provider::kUnitTypeSpeed;
  }
  if ([unit isKindOfClass:[NSUnitTemperature class]]) {
    return ios::provider::kUnitTypeTemperature;
  }
  if ([unit isKindOfClass:[NSUnitVolume class]]) {
    return ios::provider::kUnitTypeVolume;
  }
  return ios::provider::kUnitTypeUnknown;
}

}  // namespace

@interface UnitConversionViewController () <UITableViewDelegate,
                                            UITableViewDataSource,
                                            UITextFieldDelegate> {
  // The title button to change the unit type.
  UIButton* _unitTypeTitleButton;

  // The names of the source/target unit as they should be displayed.
  NSString* _formattedSourceUnit;
  NSString* _formattedTargetUnit;

  // The string to be displayed as UnitTypeValueFieldCell's text for source and
  // target values.
  NSString* _sourceUnitValueField;
  NSString* _targetUnitValueField;

  // The view to assign to the tableView's footer, where the `Report an issue`
  // button is added.
  UIView* _tableViewFooterView;

  // The `Report an issue` button.
  UIButton* _reportAnIssueButton;

  // A copy of `self.view.bounds.size.height` before its change, made during
  // calls to viewDidLayoutSubviews, and used to reduce the number of calls to
  // calculatePreferredContentHeight.
  CGFloat _previousHeight;
}

@property(nonatomic, strong) NSUnit* targetUnit;

@property(nonatomic, strong) NSUnit* sourceUnit;

// The text to be displayed as the title of the presenting view controller.
@property(nonatomic, copy) NSString* unitTypeTitle;
// The unit value.
@property(nonatomic, assign) double unitValue;

// The list of supported the unit types.
@property(nonatomic, assign) std::vector<ios::provider::UnitType> unitTypes;

// The unit type used to compute the title.
@property(nonatomic, assign) ios::provider::UnitType unitType;

@end

@implementation UnitConversionViewController

- (instancetype)initWithSourceUnit:(NSUnit*)sourceUnit
                        targetUnit:(NSUnit*)targetUnit
                         unitValue:(double)unitValue {
  self = [super initWithStyle:UITableViewStyleInsetGrouped];

  if (self) {
    _unitValue = unitValue;
    _sourceUnit = sourceUnit;
    _targetUnit = targetUnit;
    _previousHeight = 0;
  }
  return self;
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // Capture the tableView's width and use it to calculate the footer's size.
  CGFloat width = self.tableView.bounds.size.width;
  CGSize size = [_reportAnIssueButton
      systemLayoutSizeFittingSize:CGSizeMake(
                                      width,
                                      UILayoutFittingCompressedSize.height)];

  // Check the height change to avoid triggering a new layout cycle every time
  // the height is set.
  if (!AreCGFloatsEqual(_reportAnIssueButton.frame.size.height, size.height)) {
    CGRect frame = _reportAnIssueButton.frame;
    frame.size.height = size.height;
    _tableViewFooterView.frame = frame;
    self.tableView.tableFooterView = _tableViewFooterView;
  }

  // Check for height change before computing the new height.
  if (!AreCGFloatsEqual(_previousHeight, self.view.bounds.size.height)) {
    self.preferredContentSize =
        CGSizeMake(self.preferredContentSize.width,
                   [self calculatePreferredContentHeight]);
    _previousHeight = self.view.bounds.size.height;
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];
  _unitType = TypeByUnit(_sourceUnit);
  _sourceUnitValueField =
      [NSString localizedStringWithFormat:@"%g", _unitValue];
  _unitTypes = ios::provider::GetSupportedUnitTypes();
  _unitTypeTitle = [self titleForUnitType:_unitType];
  _formattedSourceUnit = ios::provider::GetFormattedUnit(_sourceUnit);
  _formattedTargetUnit = ios::provider::GetFormattedUnit(_targetUnit);
  [self computeTargetUnitValueField];

  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
  UIImage* closeIcon =
      DefaultSymbolWithPointSize(kXMarkCircleFillSymbol, kCloseButtonIconSize);
  [closeButton
      setImage:SymbolWithPalette(closeIcon,
                                 @[
                                   [UIColor colorNamed:kTextSecondaryColor],
                                   [UIColor colorNamed:kGrey200Color]
                                 ])
      forState:UIControlStateNormal];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped:)
        forControlEvents:UIControlEventTouchUpInside];
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:closeButton];

  _unitTypeTitleButton = [self createUnitTypeTitleButton];
  self.navigationItem.titleView = _unitTypeTitleButton;

  _tableViewFooterView = [[UIView alloc] init];

  _reportAnIssueButton = [self createReportIssueButton];
  _reportAnIssueButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_tableViewFooterView addSubview:_reportAnIssueButton];

  self.tableView.sectionHeaderHeight = kSectionHeaderHeight;
  self.tableView.sectionFooterHeight = kSectionFooterHeight;

  // With no header on first appearance, UITableView adds a 35 points space at
  // the beginning of the table view. This space remains after this table view
  // reloads with headers. Setting a small tableHeaderView avoids this.
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  UITapGestureRecognizer* dismissKeyBoardTap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  [self.view addGestureRecognizer:dismissKeyBoardTap];

  [self.tableView registerClass:[UnitTypeValueFieldCell class]
         forCellReuseIdentifier:kUnitTypeFieldCellIdentifier];
  [self.tableView registerClass:[UnitTypeCell class]
         forCellReuseIdentifier:kUnitTypeCellIdentifier];
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.tableView.allowsSelection = NO;

  [NSLayoutConstraint activateConstraints:@[
    [_reportAnIssueButton.trailingAnchor
        constraintLessThanOrEqualToAnchor:_tableViewFooterView.trailingAnchor
                                 constant:-kReportAnIssueButtonTrailingOffset],
    [_reportAnIssueButton.centerYAnchor
        constraintEqualToAnchor:_tableViewFooterView.centerYAnchor],
    [_reportAnIssueButton.leadingAnchor
        constraintEqualToAnchor:_tableViewFooterView.leadingAnchor
                       constant:kReportAnIssueButtonLeadingOffset],
  ]];

  self.tableView.accessibilityIdentifier = kUnitConversionTableViewIdentifier;
}

#pragma mark - Private

// Computes the new height based on the height `tableView`.
- (CGFloat)calculatePreferredContentHeight {
  return self.tableView.contentSize.height + kTableViewHeightOffset;
}

- (void)closeButtonTapped:(UIButton*)sender {
  [self.delegate didTapCloseUnitConversionController:self];
}

- (void)reportIssueButtonTapped:(UIButton*)sender {
  [self.delegate didTapReportIssueUnitConversionController:self];
}

- (UIButton*)createReportIssueButton {
  UIButton* button = [[UIButton alloc] init];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIImage* reportAnIssueIcon = DefaultSymbolWithPointSize(
      kExclamationMarkBubbleSymbol, kReportAnIssueIconSize);
  [buttonConfiguration setImage:reportAnIssueIcon];
  buttonConfiguration.imagePadding = kReportAnIssueButtonPadding;
  buttonConfiguration.attributedTitle = [self
      createAttributedStringWithTitle:
          l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_REPORT_AN_ISSUE)
                                 font:[UIFont preferredFontForTextStyle:
                                                  UIFontTextStyleFootnote]];
  button.configuration = buttonConfiguration;
  button.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
  [button setTintColor:[UIColor colorNamed:kBlueColor]];
  [button addTarget:self
                action:@selector(reportIssueButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Creates an attributed string object for a given title and font.
- (NSAttributedString*)createAttributedStringWithTitle:(NSString*)title
                                                  font:(UIFont*)font {
  NSDictionary* attributes = @{NSFontAttributeName : font};
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:title
                                             attributes:attributes];
  return attributedString;
}

// Creates a button configuration that will be attached to
// UnitTypeTitleButton.
- (UIButtonConfiguration*)createUnitTypeTitleConfiguration {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.imagePlacement = NSDirectionalRectEdgeTrailing;
  buttonConfiguration.imagePadding = kUnitTitlePadding;

  buttonConfiguration.attributedTitle = [self
      createAttributedStringWithTitle:[self unitTypeTitle]
                                 font:[UIFont preferredFontForTextStyle:
                                                  UIFontTextStyleHeadline]];
  return buttonConfiguration;
}

// Returns the UnitTypeTitleButton.
- (UIButton*)createUnitTypeTitleButton {
  UIButtonConfiguration* buttonConfiguration =
      [self createUnitTypeTitleConfiguration];
  UIButton* unitTypeTitleButton = [UIButton buttonWithType:UIButtonTypeCustom];
  unitTypeTitleButton.translatesAutoresizingMaskIntoConstraints = NO;
  unitTypeTitleButton.configuration = buttonConfiguration;

  UIImage* titleIcon =
      DefaultSymbolWithPointSize(kChevronDownCircleFill, kTitleIconSize);
  [unitTypeTitleButton
      setImage:SymbolWithPalette(titleIcon,
                                 @[
                                   [UIColor colorNamed:kTextPrimaryColor],
                                   [UIColor colorNamed:kGrey200Color]
                                 ])
      forState:UIControlStateNormal];
  [unitTypeTitleButton setTintColor:[UIColor colorNamed:kTextPrimaryColor]];
  [unitTypeTitleButton setTitleColor:[UIColor colorNamed:kSolidBlackColor]
                            forState:UIControlStateNormal];
  NSMutableArray* menuItemsArray = [NSMutableArray array];

  __weak UnitConversionViewController* weakSelf = self;
  std::vector<ios::provider::UnitType> unitTypes = [self unitTypes];
  for (ios::provider::UnitType unitType : unitTypes) {
    UIAction* action = [UIAction
        actionWithTitle:[self titleForUnitType:unitType]
                  image:nil
             identifier:nil
                handler:^(UIAction* ac) {
                  weakSelf.unitType = unitType;
                  [weakSelf.mutator unitTypeDidChange:unitType
                                            unitValue:[weakSelf unitValue]];
                }];

    [menuItemsArray addObject:action];
  }

  UIMenu* menu = [UIMenu menuWithChildren:menuItemsArray];
  unitTypeTitleButton.menu = menu;
  unitTypeTitleButton.accessibilityHint = l10n_util::GetNSString(
      IDS_IOS_UNITS_MEASUREMENTS_ACCESSIBILITY_HINT_UNIT_TYPE_SELECTOR);
  unitTypeTitleButton.showsMenuAsPrimaryAction = YES;
  [unitTypeTitleButton sizeToFit];
  return unitTypeTitleButton;
}

// Dismisses the keyboard, if it exist.
- (void)dismissKeyboard {
  [self.view endEditing:NO];
}

// Computes the _targetUnitValueField's value by converting the unit value from
// sourceUnit to targetUnit.
- (void)computeTargetUnitValueField {
  NSMeasurement* sourceUnitMeasurement =
      [[NSMeasurement alloc] initWithDoubleValue:self.unitValue
                                            unit:_sourceUnit];
  DCHECK([sourceUnitMeasurement canBeConvertedToUnit:_targetUnit]);
  NSMeasurement* targetUnitMeasurement =
      [sourceUnitMeasurement measurementByConvertingToUnit:_targetUnit];
  _targetUnitValueField = [NSString
      localizedStringWithFormat:@"%g", targetUnitMeasurement.doubleValue];
}

// Returns the title string based on the unit type.
- (NSString*)titleForUnitType:(ios::provider::UnitType)unitType {
  switch (unitType) {
    case ios::provider::kUnitTypeArea:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_AREA);
    case ios::provider::kUnitTypeInformationStorage:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_INFORMATION_STORAGE);
    case ios::provider::kUnitTypeLength:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_LENGTH);
    case ios::provider::kUnitTypeMass:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_MASS);
    case ios::provider::kUnitTypeSpeed:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_SPEED);
    case ios::provider::kUnitTypeTemperature:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_TEMPERATURE);
    case ios::provider::kUnitTypeVolume:
      return l10n_util::GetNSString(IDS_UNITS_MEASUREMENTS_VOLUME);
    case ios::provider::kUnitTypeUnknown:
      NOTREACHED();
  }
  return nil;
}

// Returns a UIMenu to the unitMenuButton of a given section.
- (UIMenu*)unitsMenuAtSection:(NSInteger)section {
  const NSArray<NSArray<NSUnit*>*>* unitList = GetUnitsForType(_unitType);
  NSMutableArray* menuItemsArray = [NSMutableArray array];
  __weak UnitConversionViewController* weakSelf = self;
  for (NSArray<NSUnit*>* units in unitList) {
    NSMutableArray* unitsSubMenu = [NSMutableArray array];
    for (NSUnit* unit in units) {
      UIAction* unitAction = [UIAction
          actionWithTitle:ios::provider::GetFormattedUnit(unit)
                    image:nil
               identifier:nil
                  handler:^(UIAction* action) {
                    if (section == kSourceSection) {
                      [weakSelf.mutator sourceUnitDidChange:unit
                                                 targetUnit:weakSelf.targetUnit
                                                  unitValue:weakSelf.unitValue
                                                   unitType:weakSelf.unitType];

                    } else if (section == kTargetSection) {
                      [weakSelf.mutator targetUnitDidChange:unit
                                                 sourceUnit:weakSelf.sourceUnit
                                                  unitValue:weakSelf.unitValue
                                                   unitType:weakSelf.unitType];
                    }
                  }];
      [unitsSubMenu addObject:unitAction];
    }
    [menuItemsArray addObject:[UIMenu menuWithTitle:@""
                                              image:nil
                                         identifier:nil
                                            options:UIMenuOptionsDisplayInline
                                           children:unitsSubMenu]];
  }
  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:menuItemsArray];
}

// Invoked when the value of the source unit textfield is changed.
- (void)sourceUnitFieldDidChange:(UITextField*)textField {
  _sourceUnitValueField = [textField.text copy];
  self.unitValue = [_sourceUnitValueField doubleValue];
  [self.mutator sourceUnitValueFieldDidChange:_sourceUnitValueField
                                   sourceUnit:_sourceUnit
                                   targetUnit:_targetUnit];
}

// Invoked when the value of the target unit textfield is changed.
- (void)targetUnitFieldDidChange:(UITextField*)textField {
  _targetUnitValueField = [textField.text copy];
  [self.mutator targetUnitValueFieldDidChange:_targetUnitValueField
                                   sourceUnit:_sourceUnit
                                   targetUnit:_targetUnit];
}

// Reloads the tableView's row based on its section and row indentifiers.
- (void)reloadRow:(NSInteger)row
          section:(NSInteger)section
           reload:(BOOL)reload {
  if (reload) {
    NSIndexPath* path = [NSIndexPath indexPathForRow:row inSection:section];
    [self.tableView reloadRowsAtIndexPaths:@[ path ]
                          withRowAnimation:UITableViewRowAnimationNone];
  }
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 2;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 2;
}

#pragma mark - UITableViewDelegate

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.row == kUnitTypeRow) {
    UnitTypeCell* cell =
        [tableView dequeueReusableCellWithIdentifier:kUnitTypeCellIdentifier];
    cell.unitMenuButton.menu = [self unitsMenuAtSection:indexPath.section];
    cell.unitMenuButton.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_UNITS_MEASUREMENTS_ACCESSIBILITY_HINT_UNIT_SELECTOR);
    cell.unitMenuButton.showsMenuAsPrimaryAction = YES;
    NSString* unitMenuButtonTitle;
    if (indexPath.section == kSourceSection) {
      unitMenuButtonTitle = _formattedSourceUnit;
      cell.unitMenuButton.accessibilityIdentifier =
          kSourceUnitMenuButtonIdentifier;
    } else if (indexPath.section == kTargetSection) {
      unitMenuButtonTitle = _formattedTargetUnit;
      cell.unitMenuButton.accessibilityIdentifier =
          kTargetUnitMenuButtonIdentifier;
    } else {
      NOTREACHED();
    }

    UIButtonConfiguration* unitMenuButtonConfiguration =
        cell.unitMenuButton.configuration;
    unitMenuButtonConfiguration.attributedTitle = [self
        createAttributedStringWithTitle:unitMenuButtonTitle
                                   font:[UIFont
                                            preferredFontForTextStyle:
                                                UIFontTextStyleSubheadline]];
    cell.unitMenuButton.configuration = unitMenuButtonConfiguration;
    return cell;
  }

  if (indexPath.row == kUnitValueFieldRow) {
    UnitTypeValueFieldCell* cell = [tableView
        dequeueReusableCellWithIdentifier:kUnitTypeFieldCellIdentifier];
    cell.unitValueTextField.delegate = self;
    if (indexPath.section == kSourceSection) {
      cell.unitValueTextField.text = _sourceUnitValueField;
      cell.unitValueTextField.accessibilityIdentifier =
          kSourceUnitFieldIdentifier;
      [cell.unitValueTextField addTarget:self
                                  action:@selector(sourceUnitFieldDidChange:)
                        forControlEvents:UIControlEventEditingChanged];
    } else if (indexPath.section == kTargetSection) {
      cell.unitValueTextField.text = _targetUnitValueField;
      cell.unitValueTextField.accessibilityIdentifier =
          kTargetUnitFieldIdentifier;
      [cell.unitValueTextField addTarget:self
                                  action:@selector(targetUnitFieldDidChange:)
                        forControlEvents:UIControlEventEditingChanged];
    } else {
      NOTREACHED();
    }
    return cell;
  }
  NOTREACHED();
}

#pragma mark - UnitConversionConsumer

- (void)updateSourceUnit:(NSUnit*)sourceUnit reload:(BOOL)reload {
  _sourceUnit = sourceUnit;
  _formattedSourceUnit = ios::provider::GetFormattedUnit(_sourceUnit);
  [self reloadRow:kUnitTypeRow section:kSourceSection reload:reload];
}

- (void)updateTargetUnit:(NSUnit*)targetUnit reload:(BOOL)reload {
  _targetUnit = targetUnit;
  _formattedTargetUnit = ios::provider::GetFormattedUnit(_targetUnit);
  [self reloadRow:kUnitTypeRow section:kTargetSection reload:reload];
}

- (void)updateSourceUnitValue:(double)sourceUnitValue reload:(BOOL)reload {
  _sourceUnitValueField =
      [NSString localizedStringWithFormat:@"%g", sourceUnitValue];
  self.unitValue = sourceUnitValue;
  [self reloadRow:kUnitValueFieldRow section:kSourceSection reload:reload];
}

- (void)updateTargetUnitValue:(double)targetUnitValue reload:(BOOL)reload {
  _targetUnitValueField =
      [NSString localizedStringWithFormat:@"%g", targetUnitValue];
  [self reloadRow:kUnitValueFieldRow section:kTargetSection reload:reload];
}

- (void)updateUnitTypeTitle:(ios::provider::UnitType)unitType {
  self.unitTypeTitle = [self titleForUnitType:unitType];
  UIButtonConfiguration* buttonConfiguration =
      _unitTypeTitleButton.configuration;
  buttonConfiguration.attributedTitle = [self
      createAttributedStringWithTitle:[self unitTypeTitle]
                                 font:[UIFont preferredFontForTextStyle:
                                                  UIFontTextStyleHeadline]];
  _unitTypeTitleButton.configuration = buttonConfiguration;
  [self.tableView reloadData];
}

@end
