// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actuation_task_card_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_task_card_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kLayoutSpacing = 16.0;
const CGFloat kControlSpacing = 8.0;
const CGFloat kHeaderIconSymbolSize = 20.0;

// Returns a UIColor with the specified name and alpha component.
UIColor* ColorWithAlpha(NSString* name, CGFloat alpha) {
  return [[UIColor colorNamed:name] colorWithAlphaComponent:alpha];
}

}  // namespace

@interface AIPrototypingActuationTaskCardViewController () <
    ActuationTaskCardViewDelegate,
    UITextFieldDelegate>
@end

@implementation AIPrototypingActuationTaskCardViewController {
  UIScrollView* _scrollView;
  UIStackView* _mainStack;

  ActuationTaskCardView* _showcaseCardView;

  UITextField* _titleField;
  UITextField* _subtitleField;
  UITextField* _buttonTitleField;
  UISegmentedControl* _iconSegment;
  UISegmentedControl* _gradientSegment;
  UISwitch* _enabledSwitch;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actuation Task Card";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Dismiss keyboard on tap outside.
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

#pragma mark - ActuationTaskCardViewDelegate

- (void)taskCardViewDidTapActionButton:(ActuationTaskCardView*)view {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:@"Action Button Tapped"
                       message:[NSString stringWithFormat:@"Button Title: %@",
                                                          view.buttonTitle]
                preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)taskCardView:(ActuationTaskCardView*)view
    didChangeCollapsedState:(BOOL)isCollapsed {
  [UIView animateWithDuration:0.45
                        delay:0.0
       usingSpringWithDamping:1.0
        initialSpringVelocity:0.0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     [self.view layoutIfNeeded];
                   }
                   completion:nil];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - Actions

// Dismisses the active keyboard when interacting outside text fields.
- (void)dismissKeyboard {
  [self.view endEditing:YES];
}

// Updates `_showcaseCardView` text properties when customizer text fields
// change.
- (void)textFieldDidChange:(UITextField*)sender {
  if (sender == _titleField) {
    _showcaseCardView.title = sender.text;
  } else if (sender == _subtitleField) {
    _showcaseCardView.subtitle = sender.text;
  } else if (sender == _buttonTitleField) {
    _showcaseCardView.buttonTitle = sender.text;
  }
}

// Updates `_showcaseCardView` header icon based on the selected segment index.
- (void)iconSegmentChanged:(UISegmentedControl*)sender {
  [self dismissKeyboard];
  UIImage* icon = nil;
  NSInteger index = sender.selectedSegmentIndex;
  if (index == 1) {
    icon = SymbolWithPointSize(SymbolSparkles, kHeaderIconSymbolSize);
  } else if (index == 2) {
    icon = SymbolWithPointSize(SymbolCart, kHeaderIconSymbolSize);
  } else if (index == 3) {
    icon = SymbolWithPointSize(SymbolSearch, kHeaderIconSymbolSize);
  }

  _showcaseCardView.headerIcon = icon;
}

// Updates `_showcaseCardView` background gradient colors based on the selected
// segment index.
- (void)gradientSegmentChanged:(UISegmentedControl*)sender {
  [self dismissKeyboard];
  UIColor* startColor = nil;
  UIColor* endColor = nil;
  NSInteger index = sender.selectedSegmentIndex;
  if (index == 1) {
    startColor = ColorWithAlpha(kBlueColor, 0.15);
    endColor = ColorWithAlpha(kBlueColor, 0.02);
  } else if (index == 2) {
    startColor = ColorWithAlpha(kRedColor, 0.15);
    endColor = ColorWithAlpha(kOrange500Color, 0.04);
  } else if (index == 3) {
    startColor = ColorWithAlpha(kGreenColor, 0.15);
    endColor = ColorWithAlpha(kGreenColor, 0.02);
  }

  [_showcaseCardView setBackgroundGradientStartColor:startColor
                                            endColor:endColor];
}

// Toggles `_showcaseCardView` action button enabled state.
- (void)enabledSwitchChanged:(UISwitch*)sender {
  [self dismissKeyboard];
  _showcaseCardView.enabled = sender.on;
}

#pragma mark - Private

// Configures Auto Layout constraints for `_scrollView` and `_mainStack`.
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
}

// Instantiates and embeds `_showcaseCardView` displaying the sample task card.
- (void)createShowcaseSection {
  _showcaseCardView = [[ActuationTaskCardView alloc]
      initWithTitle:
          @"Buy 3 tickets for Project Hail Mary for this coming Friday."
        buttonTitle:@"Start task"
        collapsible:YES];
  _showcaseCardView.subtitle =
      @"Gemini chooses which sites to use to complete the task, and may share "
      @"your personal info.";
  _showcaseCardView.headerIcon =
      SymbolWithPointSize(SymbolSparkles, kHeaderIconSymbolSize);
  _showcaseCardView.delegate = self;
  _showcaseCardView.translatesAutoresizingMaskIntoConstraints = NO;
  [_mainStack addArrangedSubview:_showcaseCardView];
}

// Constructs the interactive customizer controls for modifying titles, icons,
// gradient colors, and button states.
- (void)createCustomizationSection {
  UILabel* sectionHeader = [[UILabel alloc] init];
  sectionHeader.text = @"Interactive Customizer";
  sectionHeader.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle3];
  sectionHeader.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [_mainStack addArrangedSubview:sectionHeader];

  _titleField = [self textField:@"Card Title" text:_showcaseCardView.title];
  _subtitleField = [self textField:@"Card Subtitle"
                              text:_showcaseCardView.subtitle];
  _buttonTitleField = [self textField:@"Action Button Title"
                                 text:_showcaseCardView.buttonTitle];

  _iconSegment =
      [self segmentedControl:@"Header Icon"
                       items:@[ @"None", @"Sparkle", @"Cart", @"Search" ]
                      action:@selector(iconSegmentChanged:)
               selectedIndex:1];

  _gradientSegment =
      [self segmentedControl:@"Background Gradient"
                       items:@[ @"None", @"Blue", @"Red", @"Green" ]
                      action:@selector(gradientSegmentChanged:)
               selectedIndex:0];

  [_mainStack addArrangedSubview:[self controlLabel:@"Action Button State"]];

  _enabledSwitch =
      [self addSwitchRowWithLabel:@"Button Enabled"
                           action:@selector(enabledSwitchChanged:)];
  _enabledSwitch.on = YES;
}

// Factory helper to construct section caption labels.
- (UILabel*)controlLabel:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

// Factory helper to construct labelled text fields registered with
// `textFieldDidChange:`.
- (UITextField*)textField:(NSString*)labelText text:(NSString*)text {
  [_mainStack addArrangedSubview:[self controlLabel:labelText]];
  UITextField* textField = [[UITextField alloc] init];
  textField.text = text;
  textField.borderStyle = UITextBorderStyleRoundedRect;
  textField.delegate = self;
  textField.translatesAutoresizingMaskIntoConstraints = NO;
  [textField addTarget:self
                action:@selector(textFieldDidChange:)
      forControlEvents:UIControlEventEditingChanged];
  [_mainStack addArrangedSubview:textField];
  return textField;
}

// Factory helper to construct labelled segmented controls with custom targets.
- (UISegmentedControl*)segmentedControl:(NSString*)labelText
                                  items:(NSArray<NSString*>*)items
                                 action:(SEL)action
                          selectedIndex:(NSInteger)selectedIndex {
  [_mainStack addArrangedSubview:[self controlLabel:labelText]];
  UISegmentedControl* segment =
      [[UISegmentedControl alloc] initWithItems:items];
  segment.selectedSegmentIndex = selectedIndex;
  segment.translatesAutoresizingMaskIntoConstraints = NO;
  [segment addTarget:self
                action:action
      forControlEvents:UIControlEventValueChanged];
  [_mainStack addArrangedSubview:segment];
  return segment;
}

// Factory helper to construct labelled switch rows for boolean properties.
- (UISwitch*)addSwitchRowWithLabel:(NSString*)labelText action:(SEL)action {
  UIStackView* row = [[UIStackView alloc] init];
  row.alignment = UIStackViewAlignmentCenter;
  row.spacing = kControlSpacing;
  row.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* label = [[UILabel alloc] init];
  label.text = labelText;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [row addArrangedSubview:label];

  UISwitch* switchControl = [[UISwitch alloc] init];
  [switchControl addTarget:self
                    action:action
          forControlEvents:UIControlEventValueChanged];
  [row addArrangedSubview:switchControl];

  [_mainStack addArrangedSubview:row];
  return switchControl;
}

@end
