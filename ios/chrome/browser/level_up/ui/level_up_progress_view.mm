// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_progress_view.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/multi_color_gradient_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The card corner radius.
const CGFloat kCardCornerRadius = 24.0;
// Unified spacing constant for padding and vertical stacks.
const CGFloat kLayoutSpacing = 16.0;
// The spacing inside the tasks horizontal stack view.
const CGFloat kTasksStackSpacing = 2.0;
// The spacing inside the text vertical stack view.
const CGFloat kTextStackSpacing = 12.0;
// The thickness height of a progress task.
const CGFloat kTaskHeight = 10.0;
// The font size of the card header.
const CGFloat kHeaderFontSize = 15.0;
// The size of the task completion badge icon.
const CGFloat kCompletionBadgeSize = 40.0;
// The size of the checkered flag icon inside the badge.
const CGFloat kCheckeredFlagIconSize = 16.0;
// The spacing inside the completion row.
const CGFloat kCompletionRowSpacing = 16.0;

}  // namespace

@implementation LevelUpProgressView {
  // Label displaying the user's active level title.
  UILabel* _levelLabel;
  // View containing the progress tasks.
  UIStackView* _tasksView;
  // Gradient container view displaying the completed task badge.
  MultiColorGradientView* _completionBadgeContainer;
  // Label displaying remaining tasks progress instructions.
  UILabel* _subtitleLabel;
  // Switch toggling task notifications.
  UISwitch* _notificationToggleSwitch;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    self.layer.cornerRadius = kCardCornerRadius;
    self.layer.masksToBounds = YES;

    UIStackView* headerView = [self createHeaderView];
    _tasksView = [self createTasksView];
    _completionBadgeContainer = [self createCompletionBadgeContainer];
    _subtitleLabel = [self createSubtitleLabel];
    _notificationToggleSwitch = [self createNotificationToggleSwitch];
    UIStackView* subtitleContainer = [self createSubtitleContainer];
    UIStackView* mainStackView =
        [[UIStackView alloc] initWithArrangedSubviews:@[
          headerView, _tasksView, subtitleContainer
        ]];
    mainStackView.translatesAutoresizingMaskIntoConstraints = NO;
    mainStackView.axis = UILayoutConstraintAxisVertical;
    mainStackView.spacing = kLayoutSpacing;

    [self addSubview:mainStackView];

    AddSameConstraintsWithInset(mainStackView, self, kLayoutSpacing);
  }
  return self;
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  _levelLabel.text = l10n_util::GetNSStringF(IDS_IOS_LEVEL_UP_LEVEL_TITLE,
                                             base::NumberToString16(level));

  NSInteger completedTasksForLevel = 0;
  for (LevelUpTask* task in tasks) {
    if (task.completed) {
      completedTasksForLevel++;
    }
  }
  NSInteger totalTasksForLevel = tasks.count;
  NSInteger tasksRemaining = totalTasksForLevel - completedTasksForLevel;
  NSString* progressMessage = nil;
  if (tasksRemaining > 0) {
    _completionBadgeContainer.hidden = YES;
    _notificationToggleSwitch.hidden = YES;
    _tasksView.hidden = NO;
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    progressMessage = l10n_util::GetPluralNSStringF(
        IDS_IOS_LEVEL_UP_TASKS_REMAINING, tasksRemaining);
  } else {
    _completionBadgeContainer.hidden = NO;
    _notificationToggleSwitch.hidden = NO;
    _tasksView.hidden = YES;
    _subtitleLabel.textAlignment = NSTextAlignmentLeft;
    progressMessage = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_MAXIMUM_LEVEL);
  }
  _subtitleLabel.text = progressMessage;

  // Clear previous tasks.
  [_tasksView.arrangedSubviews
      makeObjectsPerformSelector:@selector(removeFromSuperview)];

  // Create tasks dynamically.
  for (NSInteger i = 0; i < totalTasksForLevel; i++) {
    [_tasksView
        addArrangedSubview:[self createTaskWithIndex:i
                               completedTasksForLevel:completedTasksForLevel
                                   totalTasksForLevel:totalTasksForLevel]];
  }
}

#pragma mark - Private

// Creates the header view.
- (UIStackView*)createHeaderView {
  UILabel* headerLabel = [[UILabel alloc] init];
  headerLabel.translatesAutoresizingMaskIntoConstraints = NO;
  headerLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  headerLabel.font = [UIFont systemFontOfSize:kHeaderFontSize
                                       weight:UIFontWeightRegular];
  headerLabel.text = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_HEADER_TITLE);

  _levelLabel = [[UILabel alloc] init];
  _levelLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _levelLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  UIFontDescriptor* baseDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleTitle3];
  UIFontDescriptor* boldDescriptor = [baseDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  _levelLabel.font = [UIFont fontWithDescriptor:boldDescriptor
                                           size:[boldDescriptor pointSize]];

  UIStackView* headerView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ headerLabel, _levelLabel ]];
  headerView.axis = UILayoutConstraintAxisVertical;
  headerView.spacing = kTextStackSpacing;
  return headerView;
}

// Creates the tasks view.
- (UIStackView*)createTasksView {
  UIStackView* tasksView = [[UIStackView alloc] init];
  tasksView.axis = UILayoutConstraintAxisHorizontal;
  tasksView.spacing = kTasksStackSpacing;
  tasksView.distribution = UIStackViewDistributionFillEqually;
  return tasksView;
}

// Creates the completion badge view.
- (MultiColorGradientView*)createCompletionBadgeContainer {
  MultiColorGradientView* badgeContainer =
      [[MultiColorGradientView alloc] initWithColors:@[
        [UIColor colorNamed:kBlueColor], [UIColor colorNamed:kBlue400Color]
      ]
                                           locations:nil
                                          startPoint:CGPointMake(0.0, 0.5)
                                            endPoint:CGPointMake(1.0, 0.5)];
  badgeContainer.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageView* completionBadgeView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kSealFillSymbol,
                                               kCompletionBadgeSize)];
  completionBadgeView.contentMode = UIViewContentModeScaleAspectFit;
  completionBadgeView.frame =
      CGRectMake(0, 0, kCompletionBadgeSize, kCompletionBadgeSize);
  badgeContainer.maskView = completionBadgeView;

  [NSLayoutConstraint activateConstraints:@[
    [badgeContainer.widthAnchor constraintEqualToConstant:kCompletionBadgeSize],
    [badgeContainer.heightAnchor
        constraintEqualToConstant:kCompletionBadgeSize],
  ]];

  UIImageView* completionBadgeIconView = [[UIImageView alloc] init];
  completionBadgeIconView.translatesAutoresizingMaskIntoConstraints = NO;
  completionBadgeIconView.contentMode = UIViewContentModeScaleAspectFit;
  completionBadgeIconView.tintColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  completionBadgeIconView.image =
      DefaultSymbolWithPointSize(@"flag.checkered", kCheckeredFlagIconSize);
  [badgeContainer addSubview:completionBadgeIconView];

  AddSameCenterConstraints(completionBadgeIconView, badgeContainer);

  return badgeContainer;
}

// Creates the subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  subtitleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  subtitleLabel.numberOfLines = 0;
  return subtitleLabel;
}

// Creates the notification toggle switch.
- (UISwitch*)createNotificationToggleSwitch {
  UISwitch* toggleSwitch = [[UISwitch alloc] init];
  toggleSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  toggleSwitch.onTintColor = [UIColor colorNamed:kBlueColor];
  [toggleSwitch
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  return toggleSwitch;
}

// Creates the subtitle container.
- (UIStackView*)createSubtitleContainer {
  UIStackView* subtitleContainer =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _completionBadgeContainer, _subtitleLabel, _notificationToggleSwitch
      ]];
  subtitleContainer.spacing = kCompletionRowSpacing;
  subtitleContainer.alignment = UIStackViewAlignmentCenter;
  return subtitleContainer;
}

// Creates a progress capsule task subview with the given completion state.
- (UIView*)createTaskWithIndex:(NSInteger)index
        completedTasksForLevel:(NSInteger)completedTasksForLevel
            totalTasksForLevel:(NSInteger)totalTasksForLevel {
  UIView* taskView = nil;
  if (index < completedTasksForLevel) {
    NSArray<UIColor*>* colors = @[
      [UIColor colorNamed:kBlueColor], [UIColor colorNamed:kBlue300Color]
    ];

    // Each task shows a slice of one continuous gradient spanning
    // global positions [globalStartPoint → globalEndPoint]
    // ([0 → completedTasksForLevel]).
    //
    // Task `index` occupies global range [index → index + 1],
    // but draws in its own local space [0 → 1].
    //
    // To map global gradient positions into this task’s local space:
    // localPoint = globalPoint - index
    NSInteger globalStartPoint = 0;
    NSInteger globalEndPoint = completedTasksForLevel;
    CGPoint localStartPoint = CGPointMake(globalStartPoint - index, 0.5);
    CGPoint localEndPoint = CGPointMake(globalEndPoint - index, 0.5);

    taskView = [[MultiColorGradientView alloc] initWithColors:colors
                                                    locations:nil
                                                   startPoint:localStartPoint
                                                     endPoint:localEndPoint];
  } else {
    taskView = [[UIView alloc] init];
    taskView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
  }

  taskView.translatesAutoresizingMaskIntoConstraints = NO;
  taskView.layer.cornerRadius = kTaskHeight / 2;
  taskView.layer.masksToBounds = YES;
  [taskView.heightAnchor constraintEqualToConstant:kTaskHeight].active = YES;

  return taskView;
}

@end
