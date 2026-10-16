// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actuation_intervention_view_controller.h"

#import <array>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kLayoutSpacing = 16.0;
const CGFloat kShowcaseCornerRadius = 24.0;

// Layouts of the layout picker, in segment order. Single source of truth for
// the segment index to `ActuationInterventionType` mapping.
constexpr std::array kLayouts = {
    ActuationInterventionType::kCard,
    ActuationInterventionType::kRow,
    ActuationInterventionType::kColumn,
};

// Identifiers of the showcased intervention actions.
constexpr NSInteger kPrimaryActionIdentifier = 0;
constexpr NSInteger kSecondaryActionIdentifier = 1;
constexpr NSInteger kTertiaryActionIdentifier = 2;

// Default intervention texts.
NSString* const kDefaultTitle = @"Confirm purchase";
NSString* const kDefaultSubtitle = @"3 tickets to Project Hail Mary, $45.00";
NSString* const kDefaultPrimaryButtonText = @"Continue";
NSString* const kDefaultSecondaryButtonText = @"Cancel";
NSString* const kDefaultTertiaryButtonText = @"Manually enter";

// Placeholder of the optional button fields.
NSString* const kOptionalButtonPlaceholder = @"Empty to hide the button";

// Action name displayed until an action is triggered.
NSString* const kNoActionName = @"none";

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

// Returns a human readable name for `action`, including its identifier.
NSString* ActionName(ActuationInterventionAction* action) {
  return [NSString stringWithFormat:@"%@ (#%ld)", action.title,
                                    static_cast<long>(action.identifier)];
}

// Returns the text reporting `actionName` as the last triggered action.
NSString* LastActionText(NSString* actionName) {
  return [NSString stringWithFormat:@"Last action: %@", actionName];
}

// Returns the layout picker title for `layout`.
NSString* LayoutName(ActuationInterventionType layout) {
  switch (layout) {
    case ActuationInterventionType::kCard:
      return @"Card";
    case ActuationInterventionType::kRow:
      return @"Row";
    case ActuationInterventionType::kColumn:
      return @"Column";
  }
}

}  // namespace

@interface AIPrototypingActuationInterventionViewController () <
    ActuationInterventionViewDelegate>
@end

@implementation AIPrototypingActuationInterventionViewController {
  UIStackView* _mainStack;

  ActuationInterventionView* _interventionView;
  UILabel* _lastActionLabel;

  UISegmentedControl* _layoutSegment;
  UITextField* _titleField;
  UITextField* _subtitleField;
  UITextField* _primaryButtonField;
  UITextField* _secondaryButtonField;
  UITextField* _tertiaryButtonField;

  // Caption + field containers, toggled based on the selected layout.
  UIStackView* _titleRow;
  UIStackView* _subtitleRow;
  UIStackView* _secondaryButtonRow;
  UIStackView* _tertiaryButtonRow;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actuation Intervention";
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
  scrollView.alwaysBounceVertical = YES;
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];

  _mainStack = [[UIStackView alloc] init];
  _mainStack.axis = UILayoutConstraintAxisVertical;
  _mainStack.spacing = kLayoutSpacing;
  _mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:_mainStack];

  AddSameConstraints(scrollView, self.view.safeAreaLayoutGuide);
  AddSameConstraintsWithInset(_mainStack, scrollView.contentLayoutGuide,
                              kLayoutSpacing);
  [_mainStack.widthAnchor
      constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor
                     constant:-2.0 * kLayoutSpacing]
      .active = YES;

  [self createShowcaseSection];
  [self createCustomizationSection];
  [self updateIntervention];
}

#pragma mark - ActuationInterventionViewDelegate

- (void)interventionView:(ActuationInterventionView*)view
        didTriggerAction:(ActuationInterventionAction*)action {
  _lastActionLabel.text = LastActionText(ActionName(action));
}

#pragma mark - Private

// Adds `_interventionView`, wrapped in a rounded box, to `_mainStack`.
- (void)createShowcaseSection {
  // Mimics the worklog sheet background the intervention is hosted in.
  UIView* showcaseBox = [[UIView alloc] init];
  showcaseBox.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  showcaseBox.layer.cornerRadius = kShowcaseCornerRadius;
  [_mainStack addArrangedSubview:showcaseBox];

  // The intervention view manages its own padding, so only edges are pinned.
  _interventionView = [[ActuationInterventionView alloc] init];
  _interventionView.delegate = self;
  _interventionView.translatesAutoresizingMaskIntoConstraints = NO;
  [showcaseBox addSubview:_interventionView];
  AddSameConstraints(_interventionView, showcaseBox);

  _lastActionLabel = Label(LastActionText(kNoActionName),
                           UIFontTextStyleFootnote, kTextSecondaryColor);
  [_mainStack addArrangedSubview:_lastActionLabel];
}

// Adds the controls editing `_interventionView` to `_mainStack`.
- (void)createCustomizationSection {
  [_mainStack
      addArrangedSubview:Label(@"Interactive Customizer", UIFontTextStyleTitle3,
                               kTextPrimaryColor)];

  _layoutSegment = [[UISegmentedControl alloc] init];
  for (ActuationInterventionType layout : kLayouts) {
    [_layoutSegment insertSegmentWithTitle:LayoutName(layout)
                                   atIndex:_layoutSegment.numberOfSegments
                                  animated:NO];
  }
  _layoutSegment.selectedSegmentIndex = 0;
  [_mainStack addArrangedSubview:[self rowWithControl:_layoutSegment
                                               titled:@"Layout"]];

  _titleField = TextField(kDefaultTitle);
  _titleRow = [self rowWithControl:_titleField titled:@"Title"];
  [_mainStack addArrangedSubview:_titleRow];

  _subtitleField = TextField(kDefaultSubtitle);
  _subtitleRow = [self rowWithControl:_subtitleField titled:@"Subtitle"];
  [_mainStack addArrangedSubview:_subtitleRow];

  _primaryButtonField = TextField(kDefaultPrimaryButtonText);
  [_mainStack addArrangedSubview:[self rowWithControl:_primaryButtonField
                                               titled:@"Primary button"]];

  _secondaryButtonField = TextField(kDefaultSecondaryButtonText);
  _secondaryButtonField.placeholder = kOptionalButtonPlaceholder;
  _secondaryButtonRow = [self rowWithControl:_secondaryButtonField
                                      titled:@"Secondary button"];
  [_mainStack addArrangedSubview:_secondaryButtonRow];

  _tertiaryButtonField = TextField(kDefaultTertiaryButtonText);
  _tertiaryButtonField.placeholder = kOptionalButtonPlaceholder;
  _tertiaryButtonRow = [self rowWithControl:_tertiaryButtonField
                                     titled:@"Tertiary button"];
  [_mainStack addArrangedSubview:_tertiaryButtonRow];

  // Any control missing here would not refresh `_interventionView`.
  NSArray<UIControl*>* controls = @[
    _layoutSegment, _titleField, _subtitleField, _primaryButtonField,
    _secondaryButtonField, _tertiaryButtonField
  ];
  for (UIControl* control in controls) {
    [control addTarget:self
                  action:@selector(updateIntervention)
        forControlEvents:UIControlEventValueChanged |
                         UIControlEventEditingChanged];
  }
}

// Pushes the current control values to `_interventionView` and only shows the
// fields relevant to the selected layout.
- (void)updateIntervention {
  ActuationInterventionType layout =
      kLayouts[static_cast<size_t>(_layoutSegment.selectedSegmentIndex)];
  BOOL isCard = layout == ActuationInterventionType::kCard;
  _titleRow.hidden = !isCard;
  _subtitleRow.hidden = !isCard;
  _secondaryButtonRow.hidden = isCard;
  _tertiaryButtonRow.hidden = layout != ActuationInterventionType::kColumn;

  [_interventionView configureWithData:[self interventionDataForLayout:layout]];
}

// Returns the intervention data for `layout`, filled from the text fields.
- (ActuationInterventionData*)interventionDataForLayout:
    (ActuationInterventionType)layout {
  ActuationInterventionAction* primaryAction =
      [[ActuationInterventionAction alloc]
          initWithIdentifier:kPrimaryActionIdentifier
                       title:_primaryButtonField.text];
  switch (layout) {
    case ActuationInterventionType::kCard: {
      NSString* subtitle =
          _subtitleField.text.length > 0 ? _subtitleField.text : nil;
      return [ActuationInterventionData cardItemWithTitle:_titleField.text
                                                 subtitle:subtitle
                                                   action:primaryAction];
    }
    case ActuationInterventionType::kRow: {
      if (_secondaryButtonField.text.length == 0) {
        return
            [ActuationInterventionData rowItemWithActions:@[ primaryAction ]];
      }
      ActuationInterventionAction* secondaryAction =
          [[ActuationInterventionAction alloc]
              initWithIdentifier:kSecondaryActionIdentifier
                           title:_secondaryButtonField.text];
      return [ActuationInterventionData
          rowItemWithActions:@[ secondaryAction, primaryAction ]];
    }
    case ActuationInterventionType::kColumn: {
      NSMutableArray<ActuationInterventionAction*>* actions =
          [NSMutableArray arrayWithObject:primaryAction];
      [self appendActionWithIdentifier:kSecondaryActionIdentifier
                             fromField:_secondaryButtonField
                               toArray:actions];
      [self appendActionWithIdentifier:kTertiaryActionIdentifier
                             fromField:_tertiaryButtonField
                               toArray:actions];
      return [ActuationInterventionData columnItemWithActions:actions];
    }
  }
}

// Appends an action with `identifier` titled with the text of `field` to
// `actions`, unless `field` is empty.
- (void)appendActionWithIdentifier:(NSInteger)identifier
                         fromField:(UITextField*)field
                           toArray:
                               (NSMutableArray<ActuationInterventionAction*>*)
                                   actions {
  if (field.text.length == 0) {
    return;
  }
  [actions addObject:[[ActuationInterventionAction alloc]
                         initWithIdentifier:identifier
                                      title:field.text]];
}

// Returns a vertical stack with a caption displaying `title` above `control`.
- (UIStackView*)rowWithControl:(UIView*)control titled:(NSString*)title {
  UIStackView* row = [[UIStackView alloc] initWithArrangedSubviews:@[
    Label(title, UIFontTextStyleCaption1, kTextSecondaryColor), control
  ]];
  row.axis = UILayoutConstraintAxisVertical;
  row.spacing = kLayoutSpacing / 2.0;
  return row;
}

@end
