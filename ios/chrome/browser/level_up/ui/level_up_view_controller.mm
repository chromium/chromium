// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"

#import "ios/chrome/browser/level_up/ui/level_up_progress_view.h"
#import "ios/chrome/browser/level_up/ui/level_up_table_view_controller.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing for layout margins and stacks.
const CGFloat kLayoutSpacing = 16.0;
// The size of the user avatar.
const CGFloat kAvatarSize = 56.0;
// The corner radius of the user avatar image view.
const CGFloat kAvatarCornerRadius = 28.0;
// Spacing within the welcome text container.
const CGFloat kWelcomeTextSpacing = 4.0;
// The line height multiple for the welcome title.
const CGFloat kWelcomeTextLineHeightMultiple = 1.2;
// The line height multiple for the user name.
const CGFloat kUserNameLineHeightMultiple = 1.08;

}  // namespace

@implementation LevelUpViewController {
  // Subview displaying the task progress indicator card.
  LevelUpProgressView* _progressView;
  // Child view controller managing the task table view cells.
  LevelUpTableViewController* _tableViewController;
  // Height constraint for the tasks table view.
  NSLayoutConstraint* _tableHeightConstraint;
  // View displaying the welcome user header profile.
  UIView* _welcomeHeaderView;
  // Image view containing user's sign-in avatar.
  UIImageView* _userAvatarImageView;
  // Label showing user's full name.
  UILabel* _userNameLabel;
  // User's full name.
  NSString* _userFullName;
  // User's avatar image.
  UIImage* _userAvatar;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _progressView = [[LevelUpProgressView alloc] init];
    _progressView.translatesAutoresizingMaskIntoConstraints = NO;

    _tableViewController = [[LevelUpTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* tableView = _tableViewController.tableView;

  [self addChildViewController:_tableViewController];
  [_tableViewController didMoveToParentViewController:self];

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

  _welcomeHeaderView = [self createWelcomeHeaderView];
  _userNameLabel.attributedText =
      [self attributedUserNameStringWithName:_userFullName];
  _userAvatarImageView.image = _userAvatar;

  UIStackView* mainContainer = [[UIStackView alloc] initWithArrangedSubviews:@[
    _welcomeHeaderView, _progressView, tableView
  ]];
  mainContainer.translatesAutoresizingMaskIntoConstraints = NO;
  mainContainer.axis = UILayoutConstraintAxisVertical;
  mainContainer.spacing = kLayoutSpacing;
  mainContainer.alignment = UIStackViewAlignmentFill;

  [self.view addSubview:mainContainer];

  _tableHeightConstraint =
      [tableView.heightAnchor constraintEqualToConstant:0.0];
  _tableHeightConstraint.active = YES;

  [self updateTableHeightConstraint];

  [NSLayoutConstraint activateConstraints:@[
    [mainContainer.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kLayoutSpacing],
    [mainContainer.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kLayoutSpacing],
    [mainContainer.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kLayoutSpacing],
  ]];
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  [_progressView setLevel:level tasksForLevel:tasks];
  [_tableViewController setLevel:level tasksForLevel:tasks];

  [self updateTableHeightConstraint];
}

#pragma mark - LevelUpProfileConsumer

- (void)setUserFullName:(NSString*)userFullName
             userAvatar:(UIImage*)userAvatar {
  _userFullName = userFullName;
  _userAvatar = userAvatar;

  if (self.isViewLoaded) {
    _userNameLabel.attributedText =
        [self attributedUserNameStringWithName:userFullName];
    _userAvatarImageView.image = userAvatar;
  }
}

#pragma mark - Private

// Updates the table height constraint constant to match the header and cells
// combined.
- (void)updateTableHeightConstraint {
  if (!_tableHeightConstraint) {
    return;
  }
  UITableView* tableView = _tableViewController.tableView;
  [tableView layoutIfNeeded];

  NSInteger taskCount = [_tableViewController tableView:tableView
                                  numberOfRowsInSection:0];
  CGFloat headerHeight = [tableView rectForHeaderInSection:0].size.height;
  CGFloat rowHeight = tableView.rowHeight;
  _tableHeightConstraint.constant = headerHeight + (taskCount * rowHeight);
}

- (void)dismiss {
  [self.handler dismissLevelUp];
}

// Creates the welcome user header view.
- (UIView*)createWelcomeHeaderView {
  _userAvatarImageView = [[UIImageView alloc] init];
  _userAvatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _userAvatarImageView.contentMode = UIViewContentModeScaleAspectFill;
  _userAvatarImageView.layer.cornerRadius = kAvatarCornerRadius;
  _userAvatarImageView.layer.masksToBounds = YES;
  _userAvatarImageView.backgroundColor =
      [UIColor colorNamed:kTextQuaternaryColor];
  [NSLayoutConstraint activateConstraints:@[
    [_userAvatarImageView.widthAnchor constraintEqualToConstant:kAvatarSize],
    [_userAvatarImageView.heightAnchor constraintEqualToConstant:kAvatarSize],
  ]];

  UILabel* welcomeLabel = [[UILabel alloc] init];
  welcomeLabel.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = kWelcomeTextLineHeightMultiple;

  NSDictionary<NSAttributedStringKey, id>* welcomeTextAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraphStyle
  };

  welcomeLabel.attributedText = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(IDS_IOS_LEVEL_UP_WELCOME)
          attributes:welcomeTextAttributes];

  _userNameLabel = [[UILabel alloc] init];
  _userNameLabel.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ welcomeLabel, _userNameLabel ]];
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.spacing = kWelcomeTextSpacing;

  UIStackView* headerStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _userAvatarImageView, textStack ]];
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;
  headerStack.axis = UILayoutConstraintAxisHorizontal;
  headerStack.spacing = kLayoutSpacing;
  headerStack.alignment = UIStackViewAlignmentCenter;

  [headerStack.heightAnchor constraintEqualToConstant:kAvatarSize].active = YES;

  return headerStack;
}

// Returns an attributed string for the user's full name.
- (NSAttributedString*)attributedUserNameStringWithName:(NSString*)name {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineHeightMultiple = kUserNameLineHeightMultiple;

  UIFontDescriptor* bodyDescriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
  UIFontDescriptor* boldDescriptor = [bodyDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  UIFont* userNameFont = [UIFont fontWithDescriptor:boldDescriptor size:0.0];

  NSDictionary<NSAttributedStringKey, id>* userNameTextAttributes = @{
    NSFontAttributeName : userNameFont,
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  return [[NSAttributedString alloc] initWithString:name
                                         attributes:userNameTextAttributes];
}

@end
