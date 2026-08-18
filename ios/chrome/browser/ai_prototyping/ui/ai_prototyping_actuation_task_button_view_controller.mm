// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actuation_task_button_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kLayoutSpacing = 16.0;
const CGFloat kCornerRadius = 12.0;
const CGFloat kControlSpacing = 8.0;

const CGFloat kIconPointSize = 28.0;

const CGFloat kThemeStartAlpha = 0.12;
const CGFloat kThemeEndAlphaDefault = 0.04;
const CGFloat kThemeEndAlphaLight = 0.02;

// Returns a UIColor with the specified name and alpha component.
UIColor* ColorWithAlpha(NSString* name, CGFloat alpha) {
  return [[UIColor colorNamed:name] colorWithAlphaComponent:alpha];
}

}  // namespace

@interface AIPrototypingActuationTaskButtonViewController () <
    UITextFieldDelegate>
@end

@implementation AIPrototypingActuationTaskButtonViewController {
  UIScrollView* _scrollView;
  UIStackView* _mainStack;

  UIView* _showcaseBox;
  ActuationTaskButton* _showcaseButton;

  UITextField* _titleField;
  UITextField* _subtitleField;
  UISegmentedControl* _iconSegment;
  UISegmentedControl* _themeSegment;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Custom Controls";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Dismiss keyboard on tap outside
  UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  [self.view addGestureRecognizer:tap];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_scrollView];

  _mainStack = [[UIStackView alloc] init];
  _mainStack.axis = UILayoutConstraintAxisVertical;
  _mainStack.spacing = kLayoutSpacing;
  _mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_mainStack];

  [self createShowcaseSection];
  [self createCustomizationSection];
  [self setupConstraints];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - Actions

- (void)dismissKeyboard {
  [self.view endEditing:YES];
}

- (void)textFieldDidChange:(UITextField*)sender {
  if (sender == _titleField) {
    _showcaseButton.title = sender.text;
  } else if (sender == _subtitleField) {
    _showcaseButton.subtitle = sender.text;
  }
}

- (void)iconSegmentChanged:(UISegmentedControl*)sender {
  [self dismissKeyboard];
  UIImage* icon = nil;
  NSInteger index = sender.selectedSegmentIndex;
  if (index == 1) {
    icon = SymbolWithPointSize(SymbolCart, kIconPointSize);
  } else if (index == 2) {
    icon = SymbolWithPointSize(SymbolSearch, kIconPointSize);
  } else if (index == 3) {
    icon = SymbolWithPointSize(SymbolCalendar, kIconPointSize);
  }

  _showcaseButton.icon = icon;
}

- (void)themeSegmentChanged:(UISegmentedControl*)sender {
  [self dismissKeyboard];
  UIColor* startColor = nil;
  UIColor* endColor = nil;
  NSInteger index = sender.selectedSegmentIndex;
  if (index == 1) {
    startColor = ColorWithAlpha(kRedColor, kThemeStartAlpha);
    endColor = ColorWithAlpha(kOrange500Color, kThemeEndAlphaDefault);
  } else if (index == 2) {
    startColor = ColorWithAlpha(kGreenColor, kThemeStartAlpha);
    endColor = ColorWithAlpha(kGreenColor, kThemeEndAlphaLight);
  } else if (index == 3) {
    startColor = ColorWithAlpha(kOrange500Color, kThemeStartAlpha);
    endColor = ColorWithAlpha(kOrange500Color, kThemeEndAlphaLight);
  }

  [_showcaseButton setBackgroundGradientStartColor:startColor
                                          endColor:endColor];
}

- (void)enabledSwitchChanged:(UISwitch*)sender {
  [self dismissKeyboard];
  _showcaseButton.enabled = sender.on;
}

- (void)buttonTapped:(ActuationTaskButton*)sender {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"Button Tapped"
                       message:[NSString
                                   stringWithFormat:@"Title: %@", sender.title]
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark - Private

- (void)setupConstraints {
  AddSameConstraintsWithInsets(
      _scrollView, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, 0.0, 0.0, 0.0));
  AddSameConstraintsWithInset(_mainStack, _scrollView.contentLayoutGuide,
                              kLayoutSpacing);
  [_mainStack.widthAnchor
      constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor
                     constant:-2.0 * kLayoutSpacing]
      .active = YES;

  AddSameConstraintsWithInset(_showcaseButton, _showcaseBox, kLayoutSpacing);
}

- (void)createShowcaseSection {
  _showcaseBox = [[UIView alloc] init];
  _showcaseBox.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  _showcaseBox.layer.cornerRadius = kCornerRadius;
  _showcaseBox.translatesAutoresizingMaskIntoConstraints = NO;
  [_mainStack addArrangedSubview:_showcaseBox];

  _showcaseButton = [[ActuationTaskButton alloc]
      initWithTitle:@"Buy 3 tickets for Project Hailmary"
           subtitle:@"See task on amctheaters.com"
               icon:SymbolWithPointSize(SymbolCart, kIconPointSize)];
  _showcaseButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_showcaseButton addTarget:self
                      action:@selector(buttonTapped:)
            forControlEvents:UIControlEventTouchUpInside];
  [_showcaseBox addSubview:_showcaseButton];
}

- (void)createCustomizationSection {
  UILabel* sectionHeader = [[UILabel alloc] init];
  sectionHeader.text = @"Interactive Customizer";
  sectionHeader.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
  sectionHeader.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [_mainStack addArrangedSubview:sectionHeader];

  [_mainStack addArrangedSubview:[self controlLabel:@"Button Title"]];
  _titleField = [self createTextFieldWithText:_showcaseButton.title];
  [_mainStack addArrangedSubview:_titleField];

  [_mainStack addArrangedSubview:[self controlLabel:@"Button Subtitle"]];
  _subtitleField = [self createTextFieldWithText:_showcaseButton.subtitle];
  [_mainStack addArrangedSubview:_subtitleField];

  [_mainStack addArrangedSubview:[self controlLabel:@"Left Icon Options"]];
  _iconSegment =
      [self createSegmentedControlWithItems:@[
        @"None", @"Cart", @"Search", @"Calendar"
      ]
                                     action:@selector(iconSegmentChanged:)];
  _iconSegment.selectedSegmentIndex = 1;
  [_mainStack addArrangedSubview:_iconSegment];

  [_mainStack addArrangedSubview:[self controlLabel:@"Gradient Color Theme"]];
  _themeSegment =
      [self createSegmentedControlWithItems:@[
        @"Default", @"Red", @"Green", @"Orange"
      ]
                                     action:@selector(themeSegmentChanged:)];
  _themeSegment.selectedSegmentIndex = 0;
  [_mainStack addArrangedSubview:_themeSegment];

  [_mainStack addArrangedSubview:[self controlLabel:@"State Configuration"]];
  UIStackView* enabledRow = [[UIStackView alloc] init];
  enabledRow.alignment = UIStackViewAlignmentCenter;
  enabledRow.spacing = kControlSpacing;
  enabledRow.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* enabledLabel = [[UILabel alloc] init];
  enabledLabel.text = @"Enabled";
  enabledLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  enabledLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [enabledRow addArrangedSubview:enabledLabel];

  UISwitch* enabledSwitch = [[UISwitch alloc] init];
  enabledSwitch.on = YES;
  [enabledSwitch addTarget:self
                    action:@selector(enabledSwitchChanged:)
          forControlEvents:UIControlEventValueChanged];
  [enabledRow addArrangedSubview:enabledSwitch];
  [_mainStack addArrangedSubview:enabledRow];
}

- (UILabel*)controlLabel:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

- (UITextField*)createTextFieldWithText:(NSString*)text {
  UITextField* textField = [[UITextField alloc] init];
  textField.text = text;
  textField.borderStyle = UITextBorderStyleRoundedRect;
  textField.delegate = self;
  textField.translatesAutoresizingMaskIntoConstraints = NO;
  [textField addTarget:self
                action:@selector(textFieldDidChange:)
      forControlEvents:UIControlEventEditingChanged];
  return textField;
}

- (UISegmentedControl*)createSegmentedControlWithItems:
                           (NSArray<NSString*>*)items
                                                action:(SEL)action {
  UISegmentedControl* segment =
      [[UISegmentedControl alloc] initWithItems:items];
  segment.translatesAutoresizingMaskIntoConstraints = NO;
  [segment addTarget:self
                action:action
      forControlEvents:UIControlEventValueChanged];
  return segment;
}

@end
