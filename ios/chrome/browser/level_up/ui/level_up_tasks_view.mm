// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_tasks_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The card corner radius.
const CGFloat kCardCornerRadius = 24.0;
// Unified padding spacing constant.
const CGFloat kLayoutSpacing = 16.0;
// The point size of a task icon.
const CGFloat kIconSize = 24.0;
// The point size of status chevron indicators.
const CGFloat kChevronSize = 14.0;
// The spacing within the vertical text container stack.
const CGFloat kTextContainerSpacing = 2.0;
// The hairline height of separators between rows.
const CGFloat kSeparatorHeight = 1.0;
// The transparent alpha opacity of intermediate row separators.
const CGFloat kSeparatorAlpha = 0.4;

}  // namespace

@implementation LevelUpTasksView {
  // Vertical stack view arranging task rows and separators.
  UIStackView* _tasksStackView;
  // Stored copy of the task models list.
  NSArray<LevelUpTask*>* _tasks;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.layer.cornerRadius = kCardCornerRadius;
    self.layer.masksToBounds = YES;

    UIView* headerRow = [self createHeaderRow];
    UIView* headerSeparator = [self createHeaderSeparator];
    _tasksStackView = [self createTasksStackView];

    [self addSubview:headerRow];
    [self addSubview:headerSeparator];
    [self addSubview:_tasksStackView];

    [self setupLayoutConstraintsWithHeaderRow:headerRow
                              headerSeparator:headerSeparator];
  }
  return self;
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  _tasks = [tasks copy];

  // Clear previous dynamic task rows.
  for (UIView* subview in _tasksStackView.arrangedSubviews) {
    [subview removeFromSuperview];
  }

  for (NSUInteger i = 0; i < tasks.count; i++) {
    UIView* cell = [self createRowForTask:tasks[i]
                                   isLast:(i == tasks.count - 1)];
    cell.userInteractionEnabled = YES;

    UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(handleTaskTap:)];
    [cell addGestureRecognizer:tapGesture];

    [_tasksStackView addArrangedSubview:cell];
  }
}

// Handles tap events on task cells.
- (void)handleTaskTap:(UITapGestureRecognizer*)gesture {
  NSInteger index =
      [_tasksStackView.arrangedSubviews indexOfObject:gesture.view];
  LevelUpTask* task = _tasks[index];
  if (task.navigationAction) {
    task.navigationAction();
  }
}

#pragma mark - Private

// Creates and returns a vertical cell view arranging the task row and its
// separator.
- (UIView*)createRowForTask:(LevelUpTask*)task isLast:(BOOL)isLast {
  UIImageView* iconView = [self createIconViewForTask:task];
  UIStackView* textContainer = [self createTextContainerForTask:task];
  UIImageView* chevronView = [self createChevronView];
  UIStackView* rowStackView = [self createRowStackViewWithIcon:iconView
                                                 textContainer:textContainer
                                                       chevron:chevronView];

  if (isLast) {
    return rowStackView;
  }

  UIView* separator = [self createTaskSeparator];
  UIView* cellView = [[UIView alloc] init];
  cellView.translatesAutoresizingMaskIntoConstraints = NO;

  [cellView addSubview:rowStackView];
  [cellView addSubview:separator];

  [self setupCellConstraintsWithCellView:cellView
                            rowStackView:rowStackView
                               separator:separator
                           textContainer:textContainer];

  return cellView;
}

// Creates the task status icon.
- (UIImageView*)createIconViewForTask:(LevelUpTask*)task {
  UIImageView* iconView = [[UIImageView alloc] init];
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  iconView.contentMode = UIViewContentModeScaleAspectFit;

  if (task.completed) {
    iconView.tintColor = [UIColor colorNamed:kGreen600Color];
    iconView.image = DefaultSymbolWithPointSize(kCheckmarkSymbol, kIconSize);
  } else {
    iconView.tintColor = [UIColor colorNamed:kBlueColor];
    iconView.image = DefaultSymbolWithPointSize(task.iconSymbolName, kIconSize);
  }

  AddSquareConstraints(iconView, kIconSize);
  return iconView;
}

// Creates the vertical stack arranging task title and description.
- (UIStackView*)createTextContainerForTask:(LevelUpTask*)task {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.numberOfLines = 0;
  titleLabel.text = task.title;

  UILabel* descriptionLabel = [[UILabel alloc] init];
  descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descriptionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  descriptionLabel.numberOfLines = 0;
  descriptionLabel.textAlignment = NSTextAlignmentNatural;
  descriptionLabel.text = task.taskDescription;

  UIStackView* textContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, descriptionLabel ]];
  textContainer.axis = UILayoutConstraintAxisVertical;
  textContainer.spacing = kTextContainerSpacing;
  textContainer.translatesAutoresizingMaskIntoConstraints = NO;

  return textContainer;
}

// Creates the task row chevron indicator.
- (UIImageView*)createChevronView {
  UIImageView* chevronView = [[UIImageView alloc] init];
  chevronView.translatesAutoresizingMaskIntoConstraints = NO;
  chevronView.contentMode = UIViewContentModeScaleAspectFit;
  chevronView.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  chevronView.image =
      DefaultSymbolWithPointSize(kChevronForwardSymbol, kChevronSize);

  AddSquareConstraints(chevronView, kChevronSize);

  return chevronView;
}

// Creates the hairline separator.
- (UIView*)createTaskSeparator {
  UIView* separator = [[UIView alloc] init];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = [[UIColor colorNamed:kSeparatorColor]
      colorWithAlphaComponent:kSeparatorAlpha];
  [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight].active =
      YES;
  return separator;
}

// Creates the tasks card header row view.
- (UIView*)createHeaderRow {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  UIFont* subheadFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* titleBoldDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  titleLabel.font = [UIFont fontWithDescriptor:titleBoldDescriptor size:0.0];
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_YOUR_TASKS);

  UIButton* seeAllButton = [UIButton buttonWithType:UIButtonTypeSystem];
  seeAllButton.translatesAutoresizingMaskIntoConstraints = NO;
  [seeAllButton setTitle:l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SEE_ALL)
                forState:UIControlStateNormal];
  [seeAllButton setTitleColor:[UIColor colorNamed:kBlueColor]
                     forState:UIControlStateNormal];

  UIFontDescriptor* seeAllBoldDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  seeAllButton.titleLabel.font = [UIFont fontWithDescriptor:seeAllBoldDescriptor
                                                       size:0.0];

  UIView* spacer = [[UIView alloc] init];
  [spacer setContentHuggingPriority:UILayoutPriorityDefaultLow
                            forAxis:UILayoutConstraintAxisHorizontal];

  UIStackView* headerStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, spacer, seeAllButton ]];
  headerStack.axis = UILayoutConstraintAxisHorizontal;
  headerStack.alignment = UIStackViewAlignmentCenter;
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;

  return headerStack;
}

// Creates the card header separator line.
- (UIView*)createHeaderSeparator {
  UIView* headerSeparator = [[UIView alloc] init];
  headerSeparator.translatesAutoresizingMaskIntoConstraints = NO;
  headerSeparator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [headerSeparator.heightAnchor constraintEqualToConstant:kSeparatorHeight]
      .active = YES;
  return headerSeparator;
}

// Creates the dynamic tasks arranged stack view.
- (UIStackView*)createTasksStackView {
  UIStackView* tasksStackView = [[UIStackView alloc] init];
  tasksStackView.translatesAutoresizingMaskIntoConstraints = NO;
  tasksStackView.axis = UILayoutConstraintAxisVertical;
  tasksStackView.spacing = kLayoutSpacing;
  return tasksStackView;
}

// Setup of layout constraints between header elements and task container.
- (void)setupLayoutConstraintsWithHeaderRow:(UIView*)headerRow
                            headerSeparator:(UIView*)headerSeparator {
  AddSameConstraintsToSidesWithInsets(
      headerRow, self,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing, 0,
                                  kLayoutSpacing));
  AddSameConstraintsToSides(headerSeparator, self,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(_tasksStackView, self,
                            LayoutSides::kLeading | LayoutSides::kTrailing);

  [NSLayoutConstraint activateConstraints:@[
    [headerSeparator.topAnchor constraintEqualToAnchor:headerRow.bottomAnchor
                                              constant:kLayoutSpacing],

    [_tasksStackView.topAnchor
        constraintEqualToAnchor:headerSeparator.bottomAnchor
                       constant:kLayoutSpacing],
    [_tasksStackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor
                                                 constant:-kLayoutSpacing],
  ]];
}

// Creates the horizontal stack view arranging the task row subviews.
- (UIStackView*)createRowStackViewWithIcon:(UIView*)iconView
                             textContainer:(UIView*)textContainer
                                   chevron:(UIView*)chevron {
  UIStackView* rowStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ iconView, textContainer, chevron ]];
  rowStackView.axis = UILayoutConstraintAxisHorizontal;
  rowStackView.spacing = kLayoutSpacing;
  rowStackView.alignment = UIStackViewAlignmentCenter;
  rowStackView.layoutMarginsRelativeArrangement = YES;
  rowStackView.layoutMargins =
      UIEdgeInsetsMake(0, kLayoutSpacing, 0, kLayoutSpacing);
  rowStackView.translatesAutoresizingMaskIntoConstraints = NO;
  return rowStackView;
}

// Setup of layout constraints between task cell subviews.
- (void)setupCellConstraintsWithCellView:(UIView*)cellView
                            rowStackView:(UIView*)rowStackView
                               separator:(UIView*)separator
                           textContainer:(UIView*)textContainer {
  AddSameConstraintsToSides(
      rowStackView, cellView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(separator, cellView, LayoutSides::kTrailing);

  [NSLayoutConstraint activateConstraints:@[
    [separator.topAnchor constraintEqualToAnchor:rowStackView.bottomAnchor
                                        constant:kLayoutSpacing],
    [separator.leadingAnchor
        constraintEqualToAnchor:textContainer.leadingAnchor],
    [separator.bottomAnchor constraintEqualToAnchor:cellView.bottomAnchor],
  ]];
}

@end
