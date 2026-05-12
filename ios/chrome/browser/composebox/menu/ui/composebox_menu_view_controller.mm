// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import <optional>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_cell.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_header_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_list_cell.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_separator_footer.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_strings.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The estimated height of the attachments group.
const CGFloat kAttachmentGroupEstimatedHeight = 80.0f;

// The width of the sheet in landscape.
const CGFloat kLandscapeSheetWidth = 400.0f;

// The top padding for the collection view.
const CGFloat kCollectionViewTopPadding = 20.0f;

// Spacing between attachment items.
const CGFloat kAttachmentItemSpacing = 6.0f;

// Insets for the model and tools sections.
const NSDirectionalEdgeInsets kListSectionInsets = {0, 16.0, 20.0, 16.0};

// Insets for the attachments section.
const NSDirectionalEdgeInsets kAttachmentSectionInsets = {6.0, 16.0, 8.0, 16.0};

// Vertical padding for the separator.
const CGFloat kSeparatorVerticalPadding = 10.0f;

// Height of the separator line.
const CGFloat kSeparatorHeight = 1.0f;

// Maps a menu item type to its corresponding attachment option.
std::optional<ComposeboxAttachmentOption> AttachmentOptionForMenuItemType(
    ComposeboxMenuItemType type) {
  switch (type) {
    case ComposeboxMenuItemType::kCurrentTab:
      return ComposeboxAttachmentOption::kCurrentTab;
    case ComposeboxMenuItemType::kAttachmentTabs:
      return ComposeboxAttachmentOption::kTab;
    case ComposeboxMenuItemType::kAttachmentCamera:
      return ComposeboxAttachmentOption::kCamera;
    case ComposeboxMenuItemType::kAttachmentGallery:
      return ComposeboxAttachmentOption::kGallery;
    case ComposeboxMenuItemType::kAttachmentFiles:
      return ComposeboxAttachmentOption::kFile;
    default:
      return std::nullopt;
  }
}

// Maps a tool mode to its corresponding menu item type.
ComposeboxMenuItemType MenuItemTypeForTool(ComposeboxMode mode) {
  switch (mode) {
    case ComposeboxMode::kAIM:
      return ComposeboxMenuItemType::kAIM;
    case ComposeboxMode::kImageGeneration:
      return ComposeboxMenuItemType::kCreateImage;
    case ComposeboxMode::kDeepSearch:
      return ComposeboxMenuItemType::kDeepSearch;
    case ComposeboxMode::kCanvas:
      return ComposeboxMenuItemType::kCanvas;
    case ComposeboxMode::kRegularSearch:
      return ComposeboxMenuItemType::kUnknown;
  }
}

// Maps a tool mode to its corresponding icon.
UIImage* IconForTool(ComposeboxMode mode) {
  switch (mode) {
    case ComposeboxMode::kAIM:
      return CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                       kSymbolActionPointSize);
    case ComposeboxMode::kImageGeneration:
      return GetBananaIcon(kSymbolActionPointSize);
    case ComposeboxMode::kDeepSearch:
      return CustomSymbolWithPointSize(kDeepSearchSymbol,
                                       kSymbolActionPointSize);
    case ComposeboxMode::kCanvas:
      return CustomSymbolWithPointSize(kDocumentBadgeSpark,
                                       kSymbolActionPointSize);
    case ComposeboxMode::kRegularSearch:
      return nil;
  }
}

// Maps a model option to its corresponding menu item type.
ComposeboxMenuItemType MenuItemTypeForModel(ComposeboxModelOption option) {
  switch (option) {
    case ComposeboxModelOption::kRegular:
      return ComposeboxMenuItemType::kModelRegular;
    case ComposeboxModelOption::kAuto:
      return ComposeboxMenuItemType::kModelAuto;
    case ComposeboxModelOption::kThinking:
      return ComposeboxMenuItemType::kModelThinking;
    case ComposeboxModelOption::kThinkingNoGenUI:
      return ComposeboxMenuItemType::kModelThinkingNoGenUI;
    case ComposeboxModelOption::kNone:
      return ComposeboxMenuItemType::kUnknown;
  }
}

// Maps a model option to its corresponding icon.
UIImage* IconForModel(ComposeboxModelOption option) {
  switch (option) {
    case ComposeboxModelOption::kRegular:
      return DefaultSymbolWithPointSize(kBoltSymbol, kSymbolActionPointSize);
    case ComposeboxModelOption::kAuto:
      return DefaultSymbolWithPointSize(
          kArrowTrianglehead2ClockwiseRotate90Symbol, kSymbolActionPointSize);
    case ComposeboxModelOption::kThinking:
    case ComposeboxModelOption::kThinkingNoGenUI:
      return DefaultSymbolWithPointSize(kClockSymbol, kSymbolActionPointSize);
    case ComposeboxModelOption::kNone:
      return nil;
  }
}

}  // namespace



@interface ComposeboxMenuViewController () <UICollectionViewDelegate>
@end

@implementation ComposeboxMenuViewController {
  // The collection view displaying the composebox menu.
  UICollectionView* _collectionView;
  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<NSNumber*, ComposeboxMenuItem*>*
      _dataSource;
  // The sections to display in the collection view.
  NSArray<ComposeboxMenuSection*>* _sections;
  // The UI input state for the composebox.
  ComposeboxUIInputState* _inputState;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self setUpCollectionView];
  [self setUpDataSource];
  [self applySnapshot];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf focusFirstMenuItem];
  });
}

- (CGSize)preferredContentSize {
  CGSize size = super.preferredContentSize;
  [self.view layoutIfNeeded];
  size.height =
      _collectionView.contentSize.height + _collectionView.contentInset.top;

  if ([UIDevice currentDevice].userInterfaceIdiom !=
      UIUserInterfaceIdiomPhone) {
    return size;
  }
  // Width is controlled by preferredControlSize on phones (see.
  // widthFollowsPreferredContentSizeWhenEdgeAttached).

  BOOL isLandscape =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  if (isLandscape) {
    CGFloat baseWidth = size.width > 0 ? size.width : kLandscapeSheetWidth;
    size.width =
        baseWidth < kLandscapeSheetWidth ? baseWidth : kLandscapeSheetWidth;
  } else {
    // Portrait fallback: use full width if not specified.
    if (size.width == 0) {
      size.width = self.view.bounds.size.width;
    }
  }
  return size;
}

- (void)computeSections {
  CHECK(_inputState);
  NSMutableArray<ComposeboxMenuSection*>* sections =
      [[NSMutableArray alloc] init];

  // Attachments Section
  NSMutableArray<ComposeboxMenuItem*>* attachmentsItems =
      [[NSMutableArray alloc] init];
  NSArray<ComposeboxMenuItem*>* allAttachments =
      [self availableAttachmentItems];

  for (ComposeboxMenuItem* item in allAttachments) {
    std::optional<ComposeboxAttachmentOption> option =
        AttachmentOptionForMenuItemType(item.type);
    if (option && ![_inputState isAttachmentHidden:*option]) {
      [attachmentsItems addObject:item];
    }
  }

  if (attachmentsItems.count > 0) {
    ComposeboxMenuSection* attachmentsSection = [[ComposeboxMenuSection alloc]
        initWithTitle:nil
                items:attachmentsItems
           identifier:ComposeboxMenuSectionIdentifier::kAttachments];
    [sections addObject:attachmentsSection];
  }

  ComposeboxStrings* strings = _inputState.strings;

  // Tools Section
  NSMutableArray<ComposeboxMenuItem*>* toolsItems =
      [[NSMutableArray alloc] init];

  for (ComposeboxMode mode : ComposeboxModeSet::All()) {
    if (mode == ComposeboxMode::kRegularSearch) {
      continue;
    }
    if (![_inputState isToolHidden:mode]) {
      [toolsItems
          addObject:[[ComposeboxMenuItem alloc]
                        initWithTitle:[strings menuLabelForTool:mode]
                                image:IconForTool(mode)
                                 type:MenuItemTypeForTool(mode)
                             disabled:[_inputState isToolDisabled:mode]]];
    }
  }

  if (toolsItems.count > 0) {
    ComposeboxMenuSection* toolsSection = [[ComposeboxMenuSection alloc]
        initWithTitle:strings.toolsSectionHeader
                items:toolsItems
           identifier:ComposeboxMenuSectionIdentifier::kTools];
    [sections addObject:toolsSection];
  }

  // Models Section
  NSMutableArray<ComposeboxMenuItem*>* modelsItems =
      [[NSMutableArray alloc] init];

  for (ComposeboxModelOption option : ComposeboxModelOptionSet::All()) {
    if (option == ComposeboxModelOption::kNone) {
      continue;
    }

    if (![_inputState isModelHidden:option]) {
      [modelsItems
          addObject:[[ComposeboxMenuItem alloc]
                        initWithTitle:[strings menuLabelForModel:option]
                                image:IconForModel(option)
                                 type:MenuItemTypeForModel(option)
                             disabled:[_inputState isModelDisabled:option]]];
    }
  }

  if (modelsItems.count > 0) {
    ComposeboxMenuSection* modelsSection = [[ComposeboxMenuSection alloc]
        initWithTitle:strings.modelSectionHeader
                items:modelsItems
           identifier:ComposeboxMenuSectionIdentifier::kModels];
    [sections addObject:modelsSection];
  }

  _sections = sections;
}

- (void)setUpCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:self.view.bounds
                         collectionViewLayout:[self createLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;
  _collectionView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _collectionView.showsVerticalScrollIndicator = NO;
  _collectionView.showsHorizontalScrollIndicator = NO;
  _collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentNever;
  _collectionView.contentInset =
      UIEdgeInsetsMake(kCollectionViewTopPadding, 0, 0, 0);

  [self.view addSubview:_collectionView];
  AddSameConstraints(_collectionView, self.view);
}

- (UICollectionViewLayout*)createLayout {
  UICollectionViewCompositionalLayoutConfiguration* config =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];

  __weak __typeof(self) weakSelf = self;
  return [[UICollectionViewCompositionalLayout alloc]
      initWithSectionProvider:^NSCollectionLayoutSection*(
          NSInteger sectionIndex,
          id<NSCollectionLayoutEnvironment> layoutEnvironment) {
        return [weakSelf layoutSectionForIndex:sectionIndex
                             layoutEnvironment:layoutEnvironment];
      }
                configuration:config];
}

- (NSCollectionLayoutSection*)
    layoutSectionForIndex:(NSInteger)sectionIndex
        layoutEnvironment:(id<NSCollectionLayoutEnvironment>)layoutEnvironment {
  ComposeboxMenuSectionIdentifier identifier =
      ComposeboxMenuSectionIdentifier::kAttachments;
  if (sectionIndex < (NSInteger)_sections.count) {
    identifier = _sections[sectionIndex].identifier;
  }

  if (identifier == ComposeboxMenuSectionIdentifier::kAttachments) {
    CGFloat itemsCount = 1.0;
    if (sectionIndex < (NSInteger)_sections.count) {
      itemsCount = MAX(1.0, (CGFloat)_sections[sectionIndex].items.count);
    }

    CGFloat containerWidth = layoutEnvironment.container.contentSize.width;
    CGFloat availableWidth = containerWidth - kAttachmentSectionInsets.leading -
                             kAttachmentSectionInsets.trailing;
    CGFloat totalSpacing = (itemsCount - 1) * kAttachmentItemSpacing;
    CGFloat itemWidth =
        AlignValueToPixel((availableWidth - totalSpacing) / itemsCount);

    NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   absoluteDimension:itemWidth]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.0]];

    NSCollectionLayoutItem* item =
        [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

    NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   absoluteDimension:itemWidth]
               heightDimension:
                   [NSCollectionLayoutDimension
                       estimatedDimension:kAttachmentGroupEstimatedHeight]];
    NSCollectionLayoutGroup* group =
        [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                      subitems:@[ item ]];

    NSCollectionLayoutSection* section =
        [NSCollectionLayoutSection sectionWithGroup:group];
    section.contentInsets = kAttachmentSectionInsets;
    section.orthogonalScrollingBehavior =
        UICollectionLayoutSectionOrthogonalScrollingBehaviorContinuous;
    section.interGroupSpacing = kAttachmentItemSpacing;

    if (_sections.count > 1) {
      NSCollectionLayoutSize* footerSize = [NSCollectionLayoutSize
          sizeWithWidthDimension:[NSCollectionLayoutDimension
                                     fractionalWidthDimension:1.0]
                 heightDimension:
                     [NSCollectionLayoutDimension
                         absoluteDimension:2 * kSeparatorVerticalPadding +
                                           kSeparatorHeight]];
      NSCollectionLayoutBoundarySupplementaryItem* footer =
          [NSCollectionLayoutBoundarySupplementaryItem
              boundarySupplementaryItemWithLayoutSize:footerSize
                                          elementKind:
                                              UICollectionElementKindSectionFooter
                                            alignment:NSRectAlignmentBottom];
      section.boundarySupplementaryItems = @[ footer ];
    }

    return section;
  } else {
    UICollectionLayoutListConfiguration* listConfig =
        [[UICollectionLayoutListConfiguration alloc]
            initWithAppearance:UICollectionLayoutListAppearanceInsetGrouped];
    listConfig.headerMode = UICollectionLayoutListHeaderModeSupplementary;
    listConfig.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    NSCollectionLayoutSection* section = [NSCollectionLayoutSection
        sectionWithListConfiguration:listConfig
                   layoutEnvironment:layoutEnvironment];
    section.contentInsets = kListSectionInsets;
    return section;
  }
}

#pragma mark - Private

// Focuses the first item in the menu for accessibility.
- (void)focusFirstMenuItem {
  if ([_collectionView numberOfItemsInSection:0] > 0) {
    NSIndexPath* firstIndexPath = [NSIndexPath indexPathForItem:0 inSection:0];
    UICollectionViewCell* cell =
        [_collectionView cellForItemAtIndexPath:firstIndexPath];
    if (cell) {
      UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                      cell);
    } else {
      UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                      _collectionView);
    }
  }
}

- (NSArray<ComposeboxMenuItem*>*)availableAttachmentItems {
  CHECK(_inputState);
  ComposeboxMenuItem* currentTabItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MENU_CURRENT_TAB_ACTION)
              image:DefaultSymbolWithPointSize(kGlobeSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kCurrentTab
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kCurrentTab]
            favicon:_inputState.currentTabFavicon];
  ComposeboxMenuItem* tabsItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
              image:DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentTabs
           disabled:[_inputState
                        isAttachmentDisabled:ComposeboxAttachmentOption::kTab]];
  ComposeboxMenuItem* cameraItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
              image:DefaultSymbolWithPointSize(kSystemCameraSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentCamera
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kCamera]];
  ComposeboxMenuItem* galleryItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
              image:DefaultSymbolWithPointSize(kPhotoOnRectangleAngled,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentGallery
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kGallery]];
  ComposeboxMenuItem* filesItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION)
              image:DefaultSymbolWithPointSize(kFolderSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentFiles
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kFile]];

  return @[ currentTabItem, tabsItem, cameraItem, galleryItem, filesItem ];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [collectionView deselectItemAtIndexPath:indexPath animated:YES];

  ComposeboxMenuItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  if (!item || item.disabled) {
    return;
  }

  // Ignore taps on already selected models.
  if (_inputState.activeModel != ComposeboxModelOption::kNone &&
      item.type == MenuItemTypeForModel(_inputState.activeModel)) {
    return;
  }

  [self.mutator handleItemPickedWithType:item.type];
}

#pragma mark - Data Source Helpers

- (void)setUpDataSource {
  __weak __typeof(self) weakSelf = self;

  UICollectionViewCellRegistration* listCellRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[ComposeboxMenuListCell class]
               configurationHandler:^(ComposeboxMenuListCell* cell,
                                      NSIndexPath* indexPath,
                                      ComposeboxMenuItem* item) {
                 [weakSelf configureListCell:cell
                                 atIndexPath:indexPath
                                    withItem:item];
               }];

  UICollectionViewCellRegistration* attachmentCellRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[ComposeboxMenuAttachmentCell class]
               configurationHandler:^(ComposeboxMenuAttachmentCell* cell,
                                      NSIndexPath* indexPath,
                                      ComposeboxMenuItem* item) {
                 [cell configureWithItem:item];
               }];

  UICollectionViewSupplementaryRegistration* headerRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[ComposeboxMenuHeaderView class]
                                 elementKind:
                                     UICollectionElementKindSectionHeader
                        configurationHandler:^(ComposeboxMenuHeaderView* view,
                                               NSString* elementKind,
                                               NSIndexPath* indexPath) {
                          __strong __typeof(weakSelf) strongSelf = weakSelf;
                          if (strongSelf) {
                            view.label.text =
                                strongSelf->_sections[indexPath.section].title;
                          }
                        }];

  UICollectionViewSupplementaryRegistration* footerRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[ComposeboxMenuSeparatorFooter
                                                 class]
                                 elementKind:
                                     UICollectionElementKindSectionFooter
                        configurationHandler:^(
                            ComposeboxMenuSeparatorFooter* view,
                            NSString* elementKind, NSIndexPath* indexPath){
                            // The view handles its own configuration in
                            // initWithFrame:.
                        }];

  _dataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    ComposeboxMenuItem* item) {
                  if ([item isAttachmentType]) {
                    return [collectionView
                        dequeueConfiguredReusableCellWithRegistration:
                            attachmentCellRegistration
                                                         forIndexPath:indexPath
                                                                 item:item];
                  } else {
                    return [collectionView
                        dequeueConfiguredReusableCellWithRegistration:
                            listCellRegistration
                                                         forIndexPath:indexPath
                                                                 item:item];
                  }
                }];

  _dataSource.supplementaryViewProvider = ^UICollectionReusableView*(
      UICollectionView* collectionView, NSString* elementKind,
      NSIndexPath* indexPath) {
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    if (strongSelf &&
        [elementKind isEqualToString:UICollectionElementKindSectionHeader]) {
      return [collectionView
          dequeueConfiguredReusableSupplementaryViewWithRegistration:
              headerRegistration
                                                        forIndexPath:indexPath];
    }
    if ([elementKind isEqualToString:UICollectionElementKindSectionFooter]) {
      return [collectionView
          dequeueConfiguredReusableSupplementaryViewWithRegistration:
              footerRegistration
                                                        forIndexPath:indexPath];
    }
    return nil;
  };
}

- (void)applySnapshot {
  NSDiffableDataSourceSnapshot<NSNumber*, ComposeboxMenuItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  for (ComposeboxMenuSection* section in _sections) {
    NSNumber* identifier = @(static_cast<NSInteger>(section.identifier));
    [snapshot appendSectionsWithIdentifiers:@[ identifier ]];
    [snapshot appendItemsWithIdentifiers:section.items
               intoSectionWithIdentifier:identifier];
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
  [_collectionView.collectionViewLayout invalidateLayout];
}

#pragma mark - ComposeboxMenuConsumer

- (void)setUIInputState:(ComposeboxUIInputState*)state {
  _inputState = state;
  [self computeSections];
  [self applySnapshot];
}

#pragma mark - Private Configuration Helpers

- (void)configureListCell:(ComposeboxMenuListCell*)cell
              atIndexPath:(NSIndexPath*)indexPath
                 withItem:(ComposeboxMenuItem*)item {
  UIListContentConfiguration* configuration =
      [cell defaultContentConfiguration];
  configuration.text = item.title;
  configuration.image = item.image;

  if (item.disabled) {
    configuration.textProperties.color =
        [UIColor colorNamed:kTextSecondaryColor];
    configuration.imageProperties.tintColor =
        [UIColor colorNamed:kTextSecondaryColor];
    cell.userInteractionEnabled = NO;
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
    cell.isAccessibilityElement = YES;
  } else {
    configuration.textProperties.color = [UIColor colorNamed:kTextPrimaryColor];
    configuration.imageProperties.tintColor =
        [UIColor colorNamed:kTextPrimaryColor];
    cell.userInteractionEnabled = YES;
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
    cell.isAccessibilityElement = YES;
  }

  cell.contentConfiguration = configuration;

  UIBackgroundConfiguration* backgroundConfiguration =
      [UIBackgroundConfiguration listCellConfiguration];
  backgroundConfiguration.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.backgroundConfiguration = backgroundConfiguration;

  BOOL isSelected = NO;
  if (_inputState.activeTool != ComposeboxMode::kRegularSearch &&
      item.type == MenuItemTypeForTool(_inputState.activeTool)) {
    isSelected = YES;
  } else if (_inputState.activeModel != ComposeboxModelOption::kNone &&
             item.type == MenuItemTypeForModel(_inputState.activeModel)) {
    isSelected = YES;
  }

  if (isSelected) {
    UICellAccessoryCheckmark* checkmark =
        [[UICellAccessoryCheckmark alloc] init];
    cell.accessories = @[ checkmark ];
  } else {
    cell.accessories = @[];
  }
  cell.accessibilityIdentifier =
      AccessibilityIdentifierForMenuItemType(item.type);
}
#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self.delegate composeboxMenuViewControllerDidRequestClose:self];
}

@end
