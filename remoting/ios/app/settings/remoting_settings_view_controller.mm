// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/settings/remoting_settings_view_controller.h"

#import <MaterialComponents/MaterialAppBar.h>
#import <MaterialComponents/MaterialButtons.h>

#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/settings/setting_option.h"
#import "remoting/ios/app/settings/settings_view_cell.h"
#import "remoting/ios/app/view_utils.h"
#include "base/check_op.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static NSString* const kReusableIdentifierItem = @"remotingSettingsVCItem";

static const CGFloat kSectionSeparatorHeight = 1.f;

@interface RemotingSettingsViewController () {
  MDCAppBarViewController* _appBarViewController;
  NSArray* _sections;
  NSMutableArray* _content;
}
@end

@implementation RemotingSettingsViewController

@synthesize delegate = _delegate;
@synthesize inputMode = _inputMode;
@synthesize shouldResizeHostToFit = _shouldResizeHostToFit;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  _appBarViewController = [[MDCAppBarViewController alloc] init];
  [self addChildViewController:_appBarViewController];

  self.view.backgroundColor = RemotingTheme.menuBlueColor;
  _appBarViewController.headerView.backgroundColor =
      RemotingTheme.menuBlueColor;
  MDCNavigationBarTextColorAccessibilityMutator* mutator =
      [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
  [mutator mutate:_appBarViewController.navigationBar];

  _appBarViewController.headerView.trackingScrollView = self.collectionView;
  [self.view addSubview:_appBarViewController.view];
  [_appBarViewController didMoveToParentViewController:self];

  self.collectionView.backgroundColor = RemotingTheme.menuBlueColor;

  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithImage:RemotingTheme.closeIcon
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(didTapClose:)];
  remoting::SetAccessibilityInfoFromImage(closeButton);
  self.navigationItem.leftBarButtonItem = nil;
  self.navigationItem.rightBarButtonItem = closeButton;

  [self.collectionView registerClass:[SettingsViewCell class]
          forCellWithReuseIdentifier:kReusableIdentifierItem];

  [self.collectionView registerClass:[MDCCollectionViewTextCell class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
                 withReuseIdentifier:UICollectionElementKindSectionHeader];

  // A 1px height cell acting as the separator. Not being shown on the last
  // section. See also:
  // -collectionView:layout:referenceSizeForFooterInSection:
  [self.collectionView registerClass:[UICollectionViewCell class]
          forSupplementaryViewOfKind:UICollectionElementKindSectionFooter
                 withReuseIdentifier:UICollectionElementKindSectionFooter];

  _sections = @[
    l10n_util::GetNSString(IDS_DISPLAY_OPTIONS),
    l10n_util::GetNSString(IDS_MOUSE_OPTIONS),
    l10n_util::GetNSString(IDS_KEYBOARD_OPTIONS),
    l10n_util::GetNSString(IDS_SUPPORT_MENU),
  ];
  self.styler.cellStyle = MDCCollectionViewCellStyleDefault;
  self.styler.cellBackgroundColor = UIColor.clearColor;
  self.styler.shouldHideSeparators = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationController setNavigationBarHidden:YES animated:animated];
  [self loadContent];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return (NSInteger)[_content count];
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return (NSInteger)[_content[(NSUInteger)section] count];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  SettingOption* setting = _content[indexPath.section][indexPath.item];
  SettingsViewCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kReusableIdentifierItem
                                forIndexPath:indexPath];
  [cell setSettingOption:setting];
  return cell;
}

- (CGFloat)collectionView:(UICollectionView*)collectionView
    cellHeightAtIndexPath:(NSIndexPath*)indexPath {
  SettingOption* setting = _content[indexPath.section][indexPath.item];
  switch (setting.style) {
    case OptionCheckbox:  // Fall-through
    case OptionSelector:
      return MDCCellDefaultTwoLineHeight;
    case FlatButton:  // Fall-through.
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [super collectionView:collectionView didSelectItemAtIndexPath:indexPath];

  SettingOption* setting = _content[indexPath.section][indexPath.item];

  NSMutableArray* updatedIndexPaths = [NSMutableArray arrayWithCapacity:1];
  int i = 0;
  switch (setting.style) {
    case OptionCheckbox:
      setting.checked = !setting.checked;
      [updatedIndexPaths
          addObject:[NSIndexPath indexPathForItem:indexPath.item
                                        inSection:indexPath.section]];
      break;
    case OptionSelector:
      for (SettingOption* s in _content[indexPath.section]) {
        s.checked = NO;
        [updatedIndexPaths
            addObject:[NSIndexPath indexPathForItem:i
                                          inSection:indexPath.section]];
        i++;
      }
      setting.checked = YES;
      break;
    case FlatButton:
      break;
  }
  [self.collectionView reloadItemsAtIndexPaths:updatedIndexPaths];
  if (setting.action) {
    setting.action();
  }
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    MDCCollectionViewTextCell* supplementaryView =
        [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                           withReuseIdentifier:kind
                                                  forIndexPath:indexPath];
    supplementaryView.contentView.backgroundColor = RemotingTheme.menuBlueColor;
    supplementaryView.textLabel.text = _sections[(NSUInteger)indexPath.section];
    supplementaryView.textLabel.textColor = RemotingTheme.menuTextColor;
    supplementaryView.isAccessibilityElement = YES;
    supplementaryView.accessibilityLabel = supplementaryView.textLabel.text;
    return supplementaryView;
  }
  DCHECK([kind isEqualToString:UICollectionElementKindSectionFooter]);
  UICollectionViewCell* view =
      [collectionView dequeueReusableSupplementaryViewOfKind:kind
                                         withReuseIdentifier:kind
                                                forIndexPath:indexPath];
  view.contentView.backgroundColor = RemotingTheme.menuSeparatorColor;
  return view;
}

#pragma mark - <UICollectionViewDelegateFlowLayout>

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForHeaderInSection:(NSInteger)section {
  return CGSizeMake(collectionView.bounds.size.width,
                    MDCCellDefaultOneLineHeight);
}

- (CGSize)collectionView:(UICollectionView*)collectionView
                             layout:
                                 (UICollectionViewLayout*)collectionViewLayout
    referenceSizeForFooterInSection:(NSInteger)section {
  if (section == (NSInteger)(_sections.count - 1)) {
    // No separator for last section. Note that the footer cell will not be
    // created if 0 is returned.
    return CGSizeZero;
  }
  return CGSizeMake(collectionView.bounds.size.width, kSectionSeparatorHeight);
}

#pragma mark - Private

- (void)didTapClose:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)loadContent {
  _content = [NSMutableArray array];

  __weak RemotingSettingsViewController* weakSelf = self;

  SettingOption* resizeOption = [[SettingOption alloc] init];
  resizeOption.title = l10n_util::GetNSString(IDS_RESIZE_TO_CLIENT);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  resizeOption.subtext = l10n_util::GetNSString(IDS_RESIZE_TO_CLIENT_SUBTITLE);
  resizeOption.style = OptionCheckbox;
  resizeOption.checked = self.shouldResizeHostToFit;
  __weak SettingOption* weakResizeOption = resizeOption;
  resizeOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(setResizeToFit:)]) {
      [weakSelf.delegate setResizeToFit:weakResizeOption.checked];
    }
  };

  [_content addObject:@[ resizeOption ]];

  SettingOption* directMode = [[SettingOption alloc] init];
  directMode.title = l10n_util::GetNSString(IDS_SELECT_TOUCH_MODE);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  directMode.subtext = l10n_util::GetNSString(IDS_TOUCH_MODE_DESCRIPTION);
  directMode.style = OptionSelector;
  directMode.checked =
      self.inputMode == remoting::GestureInterpreter::DIRECT_INPUT_MODE;
  directMode.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(useDirectInputMode)]) {
      [weakSelf.delegate useDirectInputMode];
    }
  };

  SettingOption* trackpadMode = [[SettingOption alloc] init];
  trackpadMode.title = l10n_util::GetNSString(IDS_SELECT_TRACKPAD_MODE);
  // TODO(nicholss): I think this text changes based on value. Confirm.
  trackpadMode.subtext = l10n_util::GetNSString(IDS_TRACKPAD_MODE_DESCRIPTION);
  trackpadMode.style = OptionSelector;
  trackpadMode.checked =
      self.inputMode == remoting::GestureInterpreter::TRACKPAD_INPUT_MODE;
  trackpadMode.action = ^{
    if ([weakSelf.delegate
            respondsToSelector:@selector(useTrackpadInputMode)]) {
      [weakSelf.delegate useTrackpadInputMode];
    }
  };

  [_content addObject:@[ directMode, trackpadMode ]];

  SettingOption* ctrlAltDelOption = [[SettingOption alloc] init];
  ctrlAltDelOption.title = l10n_util::GetNSString(IDS_SEND_CTRL_ALT_DEL);
  ctrlAltDelOption.style = FlatButton;
  ctrlAltDelOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(sendCtrAltDel)]) {
      [weakSelf.delegate sendCtrAltDel];
      [weakSelf dismissViewControllerAnimated:YES completion:nil];
    }
  };

  SettingOption* printScreenOption = [[SettingOption alloc] init];
  printScreenOption.title = l10n_util::GetNSString(IDS_SEND_PRINT_SCREEN);
  printScreenOption.style = FlatButton;
  printScreenOption.action = ^{
    if ([weakSelf.delegate respondsToSelector:@selector(sendPrintScreen)]) {
      [weakSelf.delegate sendPrintScreen];
      [weakSelf dismissViewControllerAnimated:YES completion:nil];
    }
  };

  [_content addObject:@[ ctrlAltDelOption, printScreenOption ]];

  SettingOption* helpCenterOption = [[SettingOption alloc] init];
  helpCenterOption.title = l10n_util::GetNSString(IDS_HELP_CENTER);
  helpCenterOption.style = FlatButton;
  helpCenterOption.action = ^{
    [AppDelegate.instance navigateToHelpCenter:weakSelf.navigationController];
    [weakSelf.navigationController setNavigationBarHidden:NO animated:YES];
  };

  // TODO(yuweih): Currently the EAGLView is not captured by the feedback tool.
  // To get it working we need to override renderInContext in CAEAGLLayer.
  SettingOption* sendFeedbackOption = [[SettingOption alloc] init];
  sendFeedbackOption.title =
      l10n_util::GetNSString(IDS_ACTIONBAR_SEND_FEEDBACK);
  sendFeedbackOption.style = FlatButton;
  sendFeedbackOption.action = ^{
    // Dismiss self so that it can capture the screenshot of HostView.
    [weakSelf dismissViewControllerAnimated:YES
                                 completion:^{
                                   [weakSelf.delegate sendFeedback];
                                 }];
  };

  [_content addObject:@[ helpCenterOption, sendFeedbackOption ]];

  DCHECK_EQ(_content.count, _sections.count);
}

@end
