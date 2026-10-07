// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actuation_header_view_controller.h"

#import <iterator>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kLayoutSpacing = 16.0;
const CGFloat kControlSpacing = 8.0;
const CGFloat kIconPointSize = 14.0;

// Upper bound of the secondary items stepper, and number of style pickers.
const NSUInteger kMaxSecondaryItems = 4;
// Initial number of secondary items.
const NSUInteger kDefaultSecondaryItems = 1;

// Segment indexes of the secondary item style pickers.
const NSInteger kSecondarySegmentIcon = 0;
const NSInteger kSecondarySegmentCallToAction = 1;

// Segment indexes of the primary item control.
const NSInteger kPrimarySegmentNone = 0;
const NSInteger kPrimarySegmentIcon = 1;
const NSInteger kPrimarySegmentCallToAction = 2;

// Default header texts.
NSString* const kDefaultTitle = @"Input needed";
NSString* const kDefaultSubtitle = @"Buy 3 tickets to Project Hail Mary";
NSString* const kDefaultCallToActionTitle = @"View";
NSString* const kPrimaryCallToActionTitle = @"Done";

// Returns an action doing nothing.
UIAction* NoOpAction() {
  return [UIAction actionWithHandler:^(UIAction*){
  }];
}

// Returns a label displaying `text` in `style`, colored with `colorName`.
UILabel* Label(NSString* text, UIFontTextStyle style, NSString* colorName) {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:style];
  label.textColor = [UIColor colorNamed:colorName];
  return label;
}

// Returns a rounded text field displaying `text`.
UITextField* TextField(NSString* text) {
  UITextField* textField = [[UITextField alloc] init];
  textField.borderStyle = UITextBorderStyleRoundedRect;
  textField.text = text;
  return textField;
}

// Returns a segmented control with `items`.
UISegmentedControl* SegmentedControl(NSArray<NSString*>* items) {
  return [[UISegmentedControl alloc] initWithItems:items];
}

// Returns an icon item displaying `symbol`.
ActuationHeaderItem* IconItem(Symbol symbol, NSString* title) {
  return [ActuationHeaderItem
                 itemWithIcon:SymbolWithPointSize(symbol, kIconPointSize)
                        title:title
      accessibilityIdentifier:nil
                       action:NoOpAction()];
}

// Returns a call-to-action item displaying `title`.
ActuationHeaderItem* CallToActionItem(NSString* title) {
  return [ActuationHeaderItem callToActionWithTitle:title
                            accessibilityIdentifier:nil
                                             action:NoOpAction()];
}

}  // namespace

// Container keeping fully rounded ends regardless of its height.
@interface AIPrototypingCapsuleView : UIView
@end

@implementation AIPrototypingCapsuleView

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.cornerRadius = CGRectGetHeight(self.bounds) / 2.0;
}

@end

@implementation AIPrototypingActuationHeaderViewController {
  UIScrollView* _scrollView;
  UIStackView* _mainStack;

  ActuationHeaderView* _headerView;

  UITextField* _titleField;
  UITextField* _subtitleField;
  UITextField* _callToActionTitleField;
  UISwitch* _actuatingSwitch;
  UISegmentedControl* _primarySegment;
  UIStepper* _secondaryCountStepper;
  UILabel* _secondaryCountLabel;

  // One style picker per possible secondary item, created once.
  NSArray<UISegmentedControl*>* _secondaryStyleSegments;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actuation Header";
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
  // Allows dragging, hence dismissing the keyboard, even when content fits.
  _scrollView.alwaysBounceVertical = YES;
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
  [self updateHeader];
}

#pragma mark - Private

// Pins `_scrollView` to the safe area and `_mainStack` inside its content.
- (void)setupConstraints {
  AddSameConstraints(_scrollView, self.view.safeAreaLayoutGuide);
  AddSameConstraintsWithInset(_mainStack, _scrollView.contentLayoutGuide,
                              kLayoutSpacing);
  [_mainStack.widthAnchor
      constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor
                     constant:-2.0 * kLayoutSpacing]
      .active = YES;
}

// Adds `_headerView`, wrapped in a capsule, to `_mainStack`.
- (void)createShowcaseSection {
  UIView* showcaseBox = [[AIPrototypingCapsuleView alloc] init];
  showcaseBox.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  [_mainStack addArrangedSubview:showcaseBox];

  _headerView = [[ActuationHeaderView alloc] initWithFrame:CGRectZero];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  [showcaseBox addSubview:_headerView];
  AddSameConstraintsWithInset(_headerView, showcaseBox, kControlSpacing);
}

// Adds the controls editing `_headerView` to `_mainStack`.
- (void)createCustomizationSection {
  [_mainStack
      addArrangedSubview:Label(@"Interactive Customizer", UIFontTextStyleTitle3,
                               kTextPrimaryColor)];

  _titleField = TextField(kDefaultTitle);
  [self addControl:_titleField titled:@"Title"];

  _subtitleField = TextField(kDefaultSubtitle);
  [self addControl:_subtitleField titled:@"Subtitle"];

  _callToActionTitleField = TextField(kDefaultCallToActionTitle);
  [self addControl:_callToActionTitleField titled:@"Call-to-action title"];

  _actuatingSwitch = [[UISwitch alloc] init];
  [self addRowWithLabel:Label(@"Actuating", UIFontTextStyleBody,
                              kTextPrimaryColor)
                control:_actuatingSwitch];

  _primarySegment = SegmentedControl(@[ @"None", @"Icon", @"CTA" ]);
  _primarySegment.selectedSegmentIndex = kPrimarySegmentNone;
  [self addControl:_primarySegment titled:@"Primary Item"];

  _secondaryCountLabel = Label(nil, UIFontTextStyleBody, kTextPrimaryColor);
  _secondaryCountStepper = [[UIStepper alloc] init];
  _secondaryCountStepper.minimumValue = 0.0;
  _secondaryCountStepper.maximumValue = kMaxSecondaryItems;
  _secondaryCountStepper.value = kDefaultSecondaryItems;
  [self addRowWithLabel:_secondaryCountLabel control:_secondaryCountStepper];

  NSMutableArray<UISegmentedControl*>* segments = [NSMutableArray array];
  for (NSUInteger index = 0; index < kMaxSecondaryItems; ++index) {
    UISegmentedControl* segment = SegmentedControl(@[ @"Icon", @"CTA" ]);
    segment.selectedSegmentIndex = kSecondarySegmentIcon;
    [segments addObject:segment];
  }
  segments.firstObject.selectedSegmentIndex = kSecondarySegmentCallToAction;
  _secondaryStyleSegments = segments;
  UIStackView* secondaryStylesStack =
      [[UIStackView alloc] initWithArrangedSubviews:segments];
  secondaryStylesStack.axis = UILayoutConstraintAxisVertical;
  secondaryStylesStack.spacing = kControlSpacing;
  [_mainStack addArrangedSubview:secondaryStylesStack];

  // Any control missing here would not refresh `_headerView`.
  NSArray<UIControl*>* controls = @[
    _titleField, _subtitleField, _callToActionTitleField, _actuatingSwitch,
    _primarySegment, _secondaryCountStepper
  ];
  for (UIControl* control in
       [controls arrayByAddingObjectsFromArray:_secondaryStyleSegments]) {
    [control addTarget:self
                  action:@selector(updateHeader)
        forControlEvents:UIControlEventValueChanged |
                         UIControlEventEditingChanged];
  }
}

// Pushes the current control values to `_headerView`.
- (void)updateHeader {
  _headerView.title = _titleField.text;
  _headerView.subtitle = _subtitleField.text;
  _headerView.actuating = _actuatingSwitch.on;

  NSUInteger count = static_cast<NSUInteger>(_secondaryCountStepper.value);
  _secondaryCountLabel.text =
      [NSString stringWithFormat:@"Secondary items: %@", @(count)];

  const Symbol icons[] = {SymbolCart, SymbolSearch, SymbolCalendar};
  NSString* callToActionTitle = _callToActionTitleField.text;
  NSUInteger callToActionCount = 0;
  NSMutableArray<ActuationHeaderItem*>* items = [NSMutableArray array];
  for (NSUInteger index = 0; index < _secondaryStyleSegments.count; ++index) {
    UISegmentedControl* segment = _secondaryStyleSegments[index];
    segment.hidden = index >= count;
    if (segment.hidden) {
      continue;
    }
    if (segment.selectedSegmentIndex == kSecondarySegmentIcon) {
      Symbol symbol = icons[index % std::size(icons)];
      NSString* title = [NSString stringWithFormat:@"Icon %@", @(index + 1)];
      [items addObject:IconItem(symbol, title)];
      continue;
    }
    // Number CTAs after the first one so they remain distinguishable.
    ++callToActionCount;
    NSString* title =
        callToActionCount == 1
            ? callToActionTitle
            : [NSString stringWithFormat:@"%@ %@", callToActionTitle,
                                         @(callToActionCount)];
    [items addObject:CallToActionItem(title)];
  }
  _headerView.secondaryItems = items;
  _headerView.primaryItem = [self selectedPrimaryItem];
}

// Returns the primary item matching `_primarySegment`, or nil for none.
- (ActuationHeaderItem*)selectedPrimaryItem {
  switch (_primarySegment.selectedSegmentIndex) {
    case kPrimarySegmentIcon:
      return IconItem(SymbolXMark, @"Close");
    case kPrimarySegmentCallToAction:
      return CallToActionItem(kPrimaryCallToActionTitle);
    default:
      return nil;
  }
}

// Adds `control` to `_mainStack`, below a caption displaying `title`.
- (void)addControl:(UIView*)control titled:(NSString*)title {
  [_mainStack addArrangedSubview:Label(title, UIFontTextStyleCaption1,
                                       kTextSecondaryColor)];
  [_mainStack addArrangedSubview:control];
}

// Adds a horizontal row to `_mainStack` with `label` followed by `control`.
- (void)addRowWithLabel:(UILabel*)label control:(UIView*)control {
  UIStackView* row =
      [[UIStackView alloc] initWithArrangedSubviews:@[ label, control ]];
  row.alignment = UIStackViewAlignmentCenter;
  row.spacing = kControlSpacing;
  [_mainStack addArrangedSubview:row];
}

@end
