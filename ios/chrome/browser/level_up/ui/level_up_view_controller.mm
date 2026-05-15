// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"

#import "ios/chrome/browser/level_up/ui/level_up_progress_view.h"
#import "ios/chrome/browser/level_up/ui/level_up_tasks_view.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Margin for the progress card inside the view.
const CGFloat kCardMargin = 16.0;

}  // namespace

@implementation LevelUpViewController {
  // Subview displaying the task progress indicator card.
  LevelUpProgressView* _progressView;
  // Subview displaying the card list of level-up tasks.
  LevelUpTasksView* _tasksCardView;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _progressView = [[LevelUpProgressView alloc] init];
    _progressView.translatesAutoresizingMaskIntoConstraints = NO;

    _tasksCardView = [[LevelUpTasksView alloc] init];
    _tasksCardView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_LEVEL_UP);

  UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
  menuButton.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  menuButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [menuButton setImage:DefaultSymbolTemplateWithPointSize(
                           kEllipsisSymbol, kSymbolAccessoryPointSize)
              forState:UIControlStateNormal];

  menuButton.menu = [UIMenu menuWithTitle:@"" children:@[]];
  menuButton.showsMenuAsPrimaryAction = YES;
  self.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:menuButton];

  UIButton* dismissButton = [UIButton buttonWithType:UIButtonTypeSystem];
  dismissButton.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  dismissButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [dismissButton setImage:DefaultSymbolTemplateWithPointSize(
                              kXMarkSymbol, kSymbolAccessoryPointSize)
                 forState:UIControlStateNormal];

  [dismissButton addTarget:self
                    action:@selector(dismiss)
          forControlEvents:UIControlEventTouchUpInside];
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:dismissButton];

  UIStackView* mainContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _progressView, _tasksCardView ]];
  mainContainer.translatesAutoresizingMaskIntoConstraints = NO;
  mainContainer.axis = UILayoutConstraintAxisVertical;
  mainContainer.spacing = kCardMargin;
  mainContainer.alignment = UIStackViewAlignmentFill;

  [self.view addSubview:mainContainer];

  [NSLayoutConstraint activateConstraints:@[
    [mainContainer.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kCardMargin],
    [mainContainer.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kCardMargin],
    [mainContainer.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kCardMargin],
  ]];
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  [_progressView setLevel:level tasksForLevel:tasks];
  [_tasksCardView setLevel:level tasksForLevel:tasks];
}

#pragma mark - Private

- (void)dismiss {
  [self.handler dismissLevelUp];
}

@end
