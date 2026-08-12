// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/actuation_worklog_debug_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;
using intelligence::actor::kSpacingTiny;

const CGFloat kIconSize = 16.0;

}  // namespace

@interface ActuationWorklogDebugViewController () <UITextFieldDelegate> {
  UIScrollView* _scrollView;
  ActuationWorklogView* _worklogView;
  NSMutableArray<ActuationWorklogItem*>* _worklogItems;

  UITextField* _titleField;
  UITextField* _subtitleField;
  UISegmentedControl* _stepStyleSelector;
  UISwitch* _activeSwitch;

  UITextField* _chipField;
  UISegmentedControl* _chipIconSelector;
}
@end

@implementation ActuationWorklogDebugViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actuation Worklog Sandbox";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.view.keyboardLayoutGuide.usesBottomSafeArea = NO;

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  _worklogView = [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
  _worklogView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_worklogView];

  UIView* panelContainer = [[UIView alloc] init];
  panelContainer.translatesAutoresizingMaskIntoConstraints = NO;
  panelContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  [self.view addSubview:panelContainer];

  UIView* separator = [self createSeparatorView];
  [panelContainer addSubview:separator];

  UIStackView* inputStack = [[UIStackView alloc] init];
  inputStack.axis = UILayoutConstraintAxisVertical;
  inputStack.spacing = kSpacingSmall;
  inputStack.translatesAutoresizingMaskIntoConstraints = NO;
  [panelContainer addSubview:inputStack];

  UILabel* stepHeader = [self headerLabel:@"STEP CONFIGURATION"];
  [inputStack addArrangedSubview:stepHeader];

  UIStackView* stepContainer = [[UIStackView alloc] init];
  stepContainer.spacing = kSpacingMedium;
  stepContainer.alignment = UIStackViewAlignmentTop;
  [inputStack addArrangedSubview:stepContainer];

  UIStackView* stepInputsStack = [[UIStackView alloc] init];
  stepInputsStack.axis = UILayoutConstraintAxisVertical;
  stepInputsStack.spacing = kSpacingSmall;
  [stepContainer addArrangedSubview:stepInputsStack];

  UIStackView* stepTitleRow = [self rowWithSpacing:kSpacingSmall];
  [stepInputsStack addArrangedSubview:stepTitleRow];

  _titleField = [self textField:@"Step Title"];
  [stepTitleRow addArrangedSubview:_titleField];

  _activeSwitch = [[UISwitch alloc] init];
  _activeSwitch.on = YES;
  [stepTitleRow addArrangedSubview:[self labelWithText:@"Active"]];
  [stepTitleRow addArrangedSubview:_activeSwitch];

  _subtitleField = [self textField:@"Step Subtitle (optional)"];
  [stepInputsStack addArrangedSubview:_subtitleField];

  _stepStyleSelector = [self segmentedControl:@[ @"Dot", @"Label", @"Card" ]];
  [stepInputsStack addArrangedSubview:_stepStyleSelector];

  UIStackView* stepButtonsStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        [self circleButton:SymbolPlus
                      tint:[UIColor colorNamed:kBlueColor]
                    action:@selector(add:)],
        [self circleButton:SymbolArrowClockWise
                      tint:[UIColor colorNamed:kGreenColor]
                    action:@selector(update:)],
        [self circleButton:SymbolTrash
                      tint:[UIColor colorNamed:kOrange500Color]
                    action:@selector(pop:)],
      ]];
  stepButtonsStack.axis = UILayoutConstraintAxisVertical;
  stepButtonsStack.spacing = kSpacingSmall;
  stepButtonsStack.distribution = UIStackViewDistributionFillEqually;
  [stepContainer addArrangedSubview:stepButtonsStack];

  UIView* sectionSeparator = [self createSeparatorView];
  [inputStack addArrangedSubview:sectionSeparator];

  UILabel* chipHeader = [self headerLabel:@"CHIP CONFIGURATION"];
  [inputStack addArrangedSubview:chipHeader];

  UIStackView* chipContainer = [[UIStackView alloc] init];
  chipContainer.spacing = kSpacingMedium;
  chipContainer.alignment = UIStackViewAlignmentTop;
  [inputStack addArrangedSubview:chipContainer];

  UIStackView* chipInputsStack = [[UIStackView alloc] init];
  chipInputsStack.axis = UILayoutConstraintAxisVertical;
  chipInputsStack.spacing = kSpacingSmall;
  [chipContainer addArrangedSubview:chipInputsStack];

  _chipField = [self textField:@"Active Chip Text"];
  [chipInputsStack addArrangedSubview:_chipField];

  UIStackView* chipIconRow = [self rowWithSpacing:kSpacingSmall];
  [chipInputsStack addArrangedSubview:chipIconRow];

  _chipIconSelector = [self segmentedControl:@[ @"None", @"Search", @"Globe" ]];
  [chipIconRow addArrangedSubview:_chipIconSelector];

  UIView* chipSpacer = [[UIView alloc] init];
  [chipSpacer setContentHuggingPriority:UILayoutPriorityDefaultLow
                                forAxis:UILayoutConstraintAxisHorizontal];
  [chipIconRow addArrangedSubview:chipSpacer];

  UIButton* setChipButton = [self circleButton:SymbolPencil
                                          tint:[UIColor colorNamed:kBlueColor]
                                        action:@selector(setChip:)];
  [chipContainer addArrangedSubview:setChipButton];

  UIView* resetSeparator = [self createSeparatorView];
  [inputStack addArrangedSubview:resetSeparator];

  UIButtonConfiguration* resetConfig =
      [UIButtonConfiguration plainButtonConfiguration];
  resetConfig.title = @"Reset Sandbox";
  resetConfig.baseForegroundColor = [UIColor colorNamed:kRedColor];
  resetConfig.buttonSize = UIButtonConfigurationSizeSmall;

  UIButton* resetButton = [UIButton buttonWithType:UIButtonTypeSystem];
  resetButton.configuration = resetConfig;
  [resetButton addTarget:self
                  action:@selector(reset:)
        forControlEvents:UIControlEventTouchUpInside];
  [inputStack addArrangedSubview:resetButton];

  [NSLayoutConstraint activateConstraints:@[
    [_scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_scrollView.bottomAnchor constraintEqualToAnchor:panelContainer.topAnchor],
    [_worklogView.widthAnchor
        constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor],
    [panelContainer.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor],
    [inputStack.bottomAnchor
        constraintEqualToAnchor:panelContainer.safeAreaLayoutGuide.bottomAnchor
                       constant:-kSpacingSmall],
    [stepButtonsStack.widthAnchor constraintEqualToConstant:kSpacingLarge * 2],
    [setChipButton.widthAnchor constraintEqualToConstant:kSpacingLarge * 2],
  ]];

  AddSameConstraintsToSidesWithInsets(
      _scrollView, self.view, LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0, kSpacingMedium, 0, kSpacingMedium));
  AddSameConstraints(_worklogView, _scrollView.contentLayoutGuide);
  AddSameConstraintsToSides(panelContainer, self.view,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      separator, panelContainer,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSidesWithInsets(
      inputStack, panelContainer,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(kSpacingLarge, kSpacingLarge, 0,
                                  kSpacingLarge));

  [self populateMockWorklog];
}

- (void)populateMockWorklog {
  ActuationWorklogItem* step1 = [[ActuationWorklogItem alloc]
      initWithTitle:@"Task started"
           subtitle:@"Use Gemini carefully and take control if needed. You are "
                    @"responsible for Gemini's actions during tasks."
               icon:SymbolTemplateWithPointSize(SymbolPlayFill, kIconSize)
              style:ActuationWorklogItemStyle::kLabeled
             active:NO];
  ActuationWorklogItem* step2 = [self item:@"Opening a new Tab." active:NO];
  ActuationWorklogItem* step3 = [self item:@"Searching theaters." active:NO];
  ActuationWorklogItem* step4 = [self item:@"Searching showtimes." active:NO];
  ActuationWorklogItem* step5 = [self item:@"Finding MTL venues." active:YES];

  _worklogItems = [@[ step1, step2, step3, step4, step5 ] mutableCopy];
  [_worklogView setItems:_worklogItems];
}

#pragma mark - Actions

- (void)add:(id)sender {
  [self updateLastItemActive:NO];

  ActuationWorklogItem* newItem = [self stepFromInputs:@"New Step"];
  [_worklogItems addObject:newItem];
  [_worklogView addItem:newItem];
  [self resetStepConfigSection];
  [self scrollToBottomAnimated:YES];
}

- (void)pop:(id)sender {
  if (_worklogItems.count == 0) {
    return;
  }
  [_worklogItems removeLastObject];
  [_worklogView setLastItem:nil];
  [self updateLastItemActive:YES];
  [self scrollToBottomAnimated:YES];
}

- (void)update:(id)sender {
  if (_worklogItems.count == 0) {
    return;
  }

  ActuationWorklogItem* updatedItem = [self stepFromInputs:@"Updated Step"];
  _worklogItems[_worklogItems.count - 1] = updatedItem;
  [_worklogView setLastItem:updatedItem];
  [self resetStepConfigSection];
  [self scrollToBottomAnimated:YES];
}

- (void)setChip:(id)sender {
  NSString* chipText = _chipField.text;
  if (chipText.length == 0) {
    [_worklogView setChip:nil];
    return;
  }

  UIImage* icon = [self currentChipIcon];
  ActuationWorklogChip* chip =
      [[ActuationWorklogChip alloc] initWithText:chipText icon:icon];
  [_worklogView setChip:chip];
  [self scrollToBottomAnimated:YES];
}

- (void)reset:(id)sender {
  [self populateMockWorklog];
  [self resetStepConfigSection];
  [self resetChipSection];
  [self scrollToBottomAnimated:YES];
}

#pragma mark - Helpers

- (void)updateLastItemActive:(BOOL)active {
  ActuationWorklogItem* lastItem = _worklogItems.lastObject;
  if (lastItem) {
    NSUInteger lastIndex = _worklogItems.count - 1;
    ActuationWorklogItem* updatedItem = [lastItem withActive:active];
    _worklogItems[lastIndex] = updatedItem;
    [_worklogView setLastItem:updatedItem];
  }
}

- (void)scrollToBottomAnimated:(BOOL)animated {
  [self.view layoutIfNeeded];
  CGPoint bottomOffset = CGPointMake(
      0, _scrollView.contentSize.height - _scrollView.bounds.size.height +
             _scrollView.adjustedContentInset.bottom);
  if (bottomOffset.y > 0) {
    [_scrollView setContentOffset:bottomOffset animated:animated];
  }
}

- (ActuationWorklogItem*)item:(NSString*)title active:(BOOL)active {
  return [[ActuationWorklogItem alloc]
      initWithTitle:title
           subtitle:nil
               icon:nil
              style:ActuationWorklogItemStyle::kSimple
             active:active];
}

- (ActuationWorklogItem*)stepFromInputs:(NSString*)defaultTitle {
  NSString* title = _titleField.text;
  if (title.length == 0) {
    title = defaultTitle;
  }
  NSString* subtitle = _subtitleField.text;
  UIImage* icon = SymbolTemplateWithPointSize(SymbolPlayFill, kIconSize);
  ActuationWorklogItemStyle style = [self currentItemStyle];

  return [[ActuationWorklogItem alloc] initWithTitle:title
                                            subtitle:subtitle
                                                icon:icon
                                               style:style
                                              active:_activeSwitch.on];
}

- (UILabel*)headerLabel:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

- (UIView*)createSeparatorView {
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [separator.heightAnchor constraintEqualToConstant:1.0].active = YES;
  return separator;
}

- (UILabel*)labelWithText:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

- (UITextField*)textField:(NSString*)placeholder {
  UITextField* textField = [[UITextField alloc] init];
  textField.placeholder = placeholder;
  textField.borderStyle = UITextBorderStyleRoundedRect;
  textField.delegate = self;
  return textField;
}

- (UIStackView*)rowWithSpacing:(CGFloat)spacing {
  UIStackView* row = [[UIStackView alloc] init];
  row.spacing = spacing;
  row.alignment = UIStackViewAlignmentCenter;
  return row;
}

- (UISegmentedControl*)segmentedControl:(NSArray<NSString*>*)items {
  UISegmentedControl* view = [[UISegmentedControl alloc] initWithItems:items];
  view.selectedSegmentIndex = 0;
  return view;
}

- (UIButton*)circleButton:(Symbol)symbol
                     tint:(UIColor*)tintColor
                   action:(SEL)action {
  UIButtonConfiguration* config =
      [UIButtonConfiguration tintedButtonConfiguration];
  config.image = SymbolTemplateWithPointSize(symbol, kIconSize);
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.baseForegroundColor = tintColor;
  config.baseBackgroundColor = [tintColor colorWithAlphaComponent:0.45];

  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button.heightAnchor constraintEqualToAnchor:button.widthAnchor].active = YES;
  [button addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

- (void)resetStepConfigSection {
  _titleField.text = @"";
  _subtitleField.text = @"";
  _stepStyleSelector.selectedSegmentIndex = 0;
  _activeSwitch.on = YES;
  [_titleField resignFirstResponder];
  [_subtitleField resignFirstResponder];
}

- (void)resetChipSection {
  _chipField.text = @"";
  _chipIconSelector.selectedSegmentIndex = 0;
  [_chipField resignFirstResponder];
  [_worklogView setChip:nil];
}

- (UIImage*)currentChipIcon {
  if (_chipIconSelector.selectedSegmentIndex == 1) {
    return SymbolTemplateWithPointSize(SymbolMagnifyingglass, kIconSize);
  } else if (_chipIconSelector.selectedSegmentIndex == 2) {
    return SymbolTemplateWithPointSize(SymbolGlobe, kIconSize);
  }
  return nil;
}

- (ActuationWorklogItemStyle)currentItemStyle {
  if (_stepStyleSelector.selectedSegmentIndex == 0) {
    return ActuationWorklogItemStyle::kSimple;
  } else if (_stepStyleSelector.selectedSegmentIndex == 1) {
    return ActuationWorklogItemStyle::kLabeled;
  }
  return ActuationWorklogItemStyle::kCard;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

@end
