// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_shared_tabs_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/unguessable_token.h"
#import "components/contextual_tasks/public/features.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_shared_tab.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Button size for the header close button.
const CGFloat kCloseButtonSize = 40.0;
const CGFloat kCloseButtonSymbolPointSize = 15.0;

// Navigation bar padding.
const CGFloat kNavigationBarTopPadding = 8.0;
const CGFloat kNavigationBarTrailingPadding = 4.0;

// Top padding for elements relative to parent views.
const CGFloat kDisclaimerTopPadding = 8.0;
const CGFloat kCollectionViewTopPadding = 8.0;

// Content padding and dimensions.
const CGFloat kHorizontalMargin = 16.0;
const CGFloat kFaviconSize = 24.0;
const CGFloat kTrashSymbolPointSize = 18.0;

// Section identifier for the collection view diffable data source.
NSString* const kSharedTabsSectionIdentifier = @"kSharedTabsSectionIdentifier";

// Creates a button configuration for a header button matching Assistant AIM.
UIButtonConfiguration* CreateHeaderButtonConfiguration(UIImage* image) {
  UIButtonConfiguration* config;
  if (@available(iOS 26, *)) {
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      config = [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      config = [UIButtonConfiguration glassButtonConfiguration];
    }
  } else {
    config = [UIButtonConfiguration plainButtonConfiguration];
  }

  config.image = image;
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  config.background.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;

  return config;
}

}  // namespace

@interface ComposeboxMenuSharedTabsViewController () <UICollectionViewDelegate,
                                                      UITextViewDelegate>
@end

@implementation ComposeboxMenuSharedTabsViewController {
  NSArray<ComposeboxMenuSharedTab*>* _sharedTabs;
  UINavigationBar* _navigationBar;
  UIButton* _closeButton;
  UITextView* _disclaimerTextView;
  UICollectionView* _collectionView;
  UICollectionViewDiffableDataSource<NSString*, ComposeboxMenuSharedTab*>*
      _dataSource;
}

- (instancetype)initWithSharedTabs:
    (NSArray<ComposeboxMenuSharedTab*>*)sharedTabs {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _sharedTabs = [sharedTabs copy];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  [self setUpHeaderView];
  [self setUpDisclaimerTextView];
  [self setUpCollectionView];
  [self applySnapshot];
}

#pragma mark - Private Setup Helpers

- (void)setUpHeaderView {
  _navigationBar = [[UINavigationBar alloc] init];
  _navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_navigationBar];

  UINavigationItem* navigationItem = [[UINavigationItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MENU_SHARED_TABS)];

  UIImage* image = DefaultSymbolTemplateWithPointSize(
      kXMarkSymbol, kCloseButtonSymbolPointSize);
  UIButtonConfiguration* buttonConfiguration =
      CreateHeaderButtonConfiguration(image);

  _closeButton =
      [ExtendedTouchTargetButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:nil];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_closeButton addTarget:self
                   action:@selector(didTapCloseButton)
         forControlEvents:UIControlEventTouchUpInside];
  AddSquareConstraints(_closeButton, kCloseButtonSize);

  UIBarButtonItem* closeBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:_closeButton];
  navigationItem.rightBarButtonItem = closeBarButtonItem;

  [_navigationBar pushNavigationItem:navigationItem animated:NO];

  [NSLayoutConstraint activateConstraints:@[
    [_navigationBar.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                             constant:kNavigationBarTopPadding],
    [_navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kNavigationBarTrailingPadding],
  ]];
}

- (void)setUpDisclaimerTextView {
  _disclaimerTextView = CreateUITextViewWithTextKit1();
  _disclaimerTextView.translatesAutoresizingMaskIntoConstraints = NO;
  _disclaimerTextView.editable = NO;
  _disclaimerTextView.scrollEnabled = NO;
  _disclaimerTextView.textContainerInset = UIEdgeInsetsZero;
  _disclaimerTextView.textContainer.lineFragmentPadding = 0;
  _disclaimerTextView.backgroundColor = [UIColor clearColor];
  _disclaimerTextView.delegate = self;

  NSString* description = l10n_util::GetNSString(
      IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_DESCRIPTION);
  NSString* learnMore = l10n_util::GetNSString(
      IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_LEARN_MORE);
  NSString* fullText = [NSString
      stringWithFormat:@"%@ BEGIN_LINK%@END_LINK", description, learnMore];

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightRegular),
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };

  NSString* helpURLString = base::SysUTF8ToNSString(
      contextual_tasks::GetContextualTasksOnboardingTooltipHelpUrl());
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSLinkAttributeName : helpURLString,
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
  };

  _disclaimerTextView.attributedText = AttributedStringFromStringWithLink(
      fullText, textAttributes, linkAttributes);

  [self.view addSubview:_disclaimerTextView];

  AddSameConstraintsToSidesWithInsets(
      _disclaimerTextView, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0, kHorizontalMargin, 0, kHorizontalMargin));
  [_disclaimerTextView.topAnchor
      constraintEqualToAnchor:_navigationBar.bottomAnchor
                     constant:kDisclaimerTopPadding]
      .active = YES;
}

- (void)setUpCollectionView {
  UICollectionLayoutListConfiguration* config =
      [[UICollectionLayoutListConfiguration alloc]
          initWithAppearance:UICollectionLayoutListAppearanceInsetGrouped];
  config.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  UICollectionViewCompositionalLayout* layout =
      [UICollectionViewCompositionalLayout layoutWithListConfiguration:config];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;
  _collectionView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  [self.view addSubview:_collectionView];

  __weak __typeof(self) weakSelf = self;
  UICollectionViewCellRegistration* cellRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[UICollectionViewListCell class]
               configurationHandler:^(UICollectionViewListCell* cell,
                                      NSIndexPath* indexPath,
                                      ComposeboxMenuSharedTab* tab) {
                 [weakSelf configureListCell:cell
                                 atIndexPath:indexPath
                                     withTab:tab];
               }];

  _dataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    ComposeboxMenuSharedTab* tab) {
                  return [collectionView
                      dequeueConfiguredReusableCellWithRegistration:
                          cellRegistration
                                                       forIndexPath:indexPath
                                                               item:tab];
                }];

  AddSameConstraintsToSides(
      _collectionView, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);
  [_collectionView.topAnchor
      constraintEqualToAnchor:_disclaimerTextView.bottomAnchor
                     constant:kCollectionViewTopPadding]
      .active = YES;
}

- (void)configureListCell:(UICollectionViewListCell*)cell
              atIndexPath:(NSIndexPath*)indexPath
                  withTab:(ComposeboxMenuSharedTab*)tab {
  UIListContentConfiguration* content = [cell defaultContentConfiguration];
  content.text = tab.title.length ? tab.title : @"";
  content.textProperties.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  content.textProperties.color = [UIColor colorNamed:kTextPrimaryColor];
  content.textProperties.numberOfLines = 1;
  content.textProperties.lineBreakMode = NSLineBreakByTruncatingTail;

  NSString* domain = @"";
  if (tab.URL.is_valid() && !tab.URL.host().empty()) {
    std::u16string elidedHost = url_formatter::
        FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
            tab.URL);
    domain = base::SysUTF16ToNSString(elidedHost);
  }

  content.secondaryText = domain;
  content.secondaryTextProperties.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  content.secondaryTextProperties.color =
      [UIColor colorNamed:kTextSecondaryColor];
  content.secondaryTextProperties.numberOfLines = 1;
  content.secondaryTextProperties.lineBreakMode = NSLineBreakByTruncatingTail;

  if (tab.favicon) {
    content.image = tab.favicon;
  } else {
    content.image = DefaultSymbolWithPointSize(kGlobeSymbol, kFaviconSize);
  }
  content.imageProperties.maximumSize = CGSizeMake(kFaviconSize, kFaviconSize);
  content.imageProperties.cornerRadius = 4.0;

  cell.contentConfiguration = content;
}

- (void)applySnapshot {
  NSDiffableDataSourceSnapshot<NSString*, ComposeboxMenuSharedTab*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kSharedTabsSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:_sharedTabs];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UICollectionViewDelegate

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  ComposeboxMenuSharedTab* tab =
      [_dataSource itemIdentifierForIndexPath:indexPath];
  if (!tab) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     return [weakSelf contextMenuForTab:tab];
                   }];
}

// Creates and returns the context menu for a given shared `tab`.
- (UIMenu*)contextMenuForTab:(ComposeboxMenuSharedTab*)tab {
  __weak __typeof(self) weakSelf = self;

  UIAction* removeAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_REMOVE_TAB)
                image:DefaultSymbolWithPointSize(kTrashSymbol,
                                                 kTrashSymbolPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf removeTab:tab];
              }];
  removeAction.attributes = UIMenuElementAttributesDestructive;

  return [UIMenu menuWithTitle:@"" children:@[ removeAction ]];
}

// Removes `tab` from the collection view and notifies the delegate.
- (void)removeTab:(ComposeboxMenuSharedTab*)tab {
  NSMutableArray<ComposeboxMenuSharedTab*>* updatedTabs =
      [_sharedTabs mutableCopy];
  [updatedTabs removeObject:tab];
  _sharedTabs = [updatedTabs copy];

  NSDiffableDataSourceSnapshot<NSString*, ComposeboxMenuSharedTab*>* snapshot =
      [_dataSource snapshot];
  [snapshot deleteItemsWithIdentifiers:@[ tab ]];
  [_dataSource applySnapshot:snapshot animatingDifferences:YES];

  [self.delegate composeboxMenuSharedTabsViewController:self
                            didRemoveTabWithServerToken:tab.serverToken];
}

#pragma mark - User Actions

- (void)didTapCloseButton {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange {
  GURL gurl(base::SysNSStringToUTF8(URL.absoluteString));
  [self.delegate composeboxMenuSharedTabsViewController:self didTapURL:gurl];
  return NO;
}

@end
