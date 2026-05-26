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
  // The table view controller.
  LevelUpTableViewController* _tableViewController;

  // The root scroll view enclosing all contents.
  UIScrollView* _scrollView;
  // The main vertical container.
  UIStackView* _mainContainer;

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

  // Stored copies of levels and tasks.
  NSArray<LevelUpTask*>* _tasks;
  NSInteger _level;
}

@synthesize tasksConsumer = _tableViewController;

- (instancetype)init {
  self = [super init];
  if (self) {
    _tableViewController = [[LevelUpTableViewController alloc]
        initWithHeaderTitle:l10n_util::GetNSString(IDS_IOS_LEVEL_UP_YOUR_TASKS)
          showsSeeAllButton:YES
                      style:UITableViewStylePlain];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setupChildControllers];
  [self setupNavigationItems];
  [self setupContentView];

  [_progressView setLevel:_level tasksForLevel:_tasks];
  [_tableViewController setLevel:_level tasksForLevel:_tasks];
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  _level = level;
  _tasks = [tasks copy];
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

// Configures and layouts the child task card controllers.
- (void)setupChildControllers {
  _progressView = [[LevelUpProgressView alloc] init];
  _progressView.translatesAutoresizingMaskIntoConstraints = NO;

  [self addChildViewController:_tableViewController];
  [_tableViewController didMoveToParentViewController:self];
}

// Configures the custom navigation bar button items.
- (void)setupNavigationItems {
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
}

// Configures the scroll view, welcome header view, and container stack layouts.
- (void)setupContentView {
  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.alwaysBounceVertical = YES;
  [self.view addSubview:_scrollView];
  AddSameConstraints(_scrollView, self.view);

  _welcomeHeaderView = [self createWelcomeHeaderView];
  _userNameLabel.attributedText =
      [self attributedUserNameStringWithName:_userFullName];
  _userAvatarImageView.image = _userAvatar;

  _mainContainer = [[UIStackView alloc] initWithArrangedSubviews:@[
    _welcomeHeaderView, _progressView, _tableViewController.view
  ]];
  _mainContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _mainContainer.axis = UILayoutConstraintAxisVertical;
  _mainContainer.spacing = kLayoutSpacing;
  _mainContainer.alignment = UIStackViewAlignmentFill;
  [_scrollView addSubview:_mainContainer];

  AddSameConstraintsWithInsets(
      _mainContainer, _scrollView.contentLayoutGuide,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                  kLayoutSpacing, kLayoutSpacing));
  [_mainContainer.widthAnchor constraintEqualToAnchor:_scrollView.widthAnchor
                                             constant:-2 * kLayoutSpacing]
      .active = YES;
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
  return [[NSAttributedString alloc] initWithString:name ?: @""
                                         attributes:userNameTextAttributes];
}

@end
