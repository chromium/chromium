// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_page_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_shared_tab.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_cell.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_header_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_list_cell.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_separator_footer.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/composebox/ui/composebox_favicons_accordion_view.h"
#import "ios/chrome/browser/composebox/ui/composebox_model_item.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_config.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The estimated height of the attachments group.
const CGFloat kAttachmentGroupEstimatedHeight = 80.0f;
const CGFloat kAttachmentGroupEstimatedLargeHeight = 100.0f;

// The top padding for the collection view.
const CGFloat kCollectionViewTopPadding = 20.0f;
const CGFloat kCollectionViewSectionVerticalInset = 20.0f;

// Spacing between attachment items.
const CGFloat kAttachmentItemSpacing = 6.0f;

// Minimum width for attachment items to prevent them from becoming taller than
// wide. When items would be narrower than this, the section scrolls
// horizontally.
const CGFloat kAttachmentItemMinimumWidth = 60.0f;
const CGFloat kAttachmentItemLargeMinimumWidth = 80.0f;

// Insets for the model and tools sections.
const NSDirectionalEdgeInsets kListSectionInsets = {
    0, 16.0, kCollectionViewSectionVerticalInset, 16.0};

// Insets for the attachments section.
const NSDirectionalEdgeInsets kAttachmentSectionInsets = {6.0, 16.0, 8.0, 16.0};

// Vertical padding for the separator.
const CGFloat kSeparatorVerticalPadding = 10.0f;

// Height of the separator line.
const CGFloat kSeparatorHeight = 1.0f;

// Size of the fallback globe symbol for shared tabs without a favicon.
constexpr CGFloat kSharedTabFaviconSymbolSize = 24.0f;

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
    case ComposeboxMenuItemType::kAttachmentDrive:
      CHECK(IsComposeboxDriveOptionEnabled());
      return ComposeboxAttachmentOption::kDrive;
    default:
      return std::nullopt;
  }
}

}  // namespace

// Container view with safe area insets overridden.
@interface ComposeboxMenuPageView : UIView

@end

@implementation ComposeboxMenuPageView

- (UIEdgeInsets)safeAreaInsets {
  return UIEdgeInsetsZero;
}

@end

@implementation ComposeboxMenuPageViewController {
  // The diffable data source for the collection view.
  UICollectionViewDiffableDataSource<NSNumber*, ComposeboxMenuItem*>*
      _dataSource;
  // The sections to display in the collection view.
  NSArray<ComposeboxMenuSection*>* _sections;
  // The UI input state for the composebox.
  ComposeboxUIInputState* _inputState;
  UIView* _containerView;
}

- (instancetype)initWithPageType:(ComposeboxMenuPageType)menuPageType {
  self = [super init];
  if (self) {
    _menuPageType = menuPageType;
  }

  return self;
}

- (CGFloat)contentHeightToFit {
  return _collectionView.contentSize.height + _collectionView.contentInset.top;
}

#pragma mark - UIKit

- (void)viewDidLoad {
  [super viewDidLoad];
  _containerView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  [self setUpCollectionView];
  [self setUpDataSource];
  [self applySnapshot];

  if (_menuPageType == ComposeboxMenuPageType::kSecondaryPage) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MORE_OPTIONS_MENU_TITLE);
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf focusFirstMenuItem];
  });
}

- (void)computeSections {
  CHECK(_inputState);
  NSMutableArray<ComposeboxMenuSection*>* sections =
      [[NSMutableArray alloc] init];

  if (ComposeboxMenuSection* currentTabSection = [self currentTabSection]) {
    [sections addObject:currentTabSection];
  }
  if (ComposeboxMenuSection* attachmentsSection = [self attachmentsSection]) {
    [sections addObject:attachmentsSection];
  }
  if (ComposeboxMenuSection* sharedTabsSection = [self sharedTabsSection]) {
    [sections addObject:sharedTabsSection];
  }
  if (ComposeboxMenuSection* toolsSection = [self toolsSection]) {
    [sections addObject:toolsSection];
  }
  if (ComposeboxMenuSection* modelsSection = [self modelsSection]) {
    [sections addObject:modelsSection];
  }
  if (ComposeboxMenuSection* moreOptionsSection = [self moreOptionsSection]) {
    [sections addObject:moreOptionsSection];
  }

  _sections = sections;
}

- (void)loadView {
  [super loadView];

  _containerView = [[ComposeboxMenuPageView alloc] init];
  _containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_containerView];
  AddSameConstraints(self.view, _containerView);
}

- (void)setUpCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:_containerView.bounds
                         collectionViewLayout:[self createLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;
  _collectionView.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  _collectionView.showsVerticalScrollIndicator = NO;
  _collectionView.showsHorizontalScrollIndicator = NO;
  _collectionView.contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentAlways;

  CGFloat topPadding = kCollectionViewTopPadding;
  if (_menuPageType != ComposeboxMenuPageType::kSecondaryPage &&
      IsPlusButtonMenuVerticalListView()) {
    topPadding += kCollectionViewSectionVerticalInset;
  }
  _collectionView.contentInset = UIEdgeInsetsMake(topPadding, 0, 0, 0);
  _collectionView.insetsLayoutMarginsFromSafeArea = NO;
  [_containerView addSubview:_collectionView];

  if (_menuPageType == ComposeboxMenuPageType::kSecondaryPage) {
    AddSameConstraints(_collectionView, self.view.safeAreaLayoutGuide);
  } else {
    AddSameConstraints(_collectionView, self.view);
  }
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
  //  ComposeboxMenuSectionIdentifier identifier =
  //      ComposeboxMenuSectionIdentifier::kAttachments;
  //  if (sectionIndex < (NSInteger)_sections.count) {
  //    identifier = _sections[sectionIndex].identifier;
  //  }

  NSString* sectionTitle = nil;
  if (sectionIndex < (NSInteger)_sections.count) {
    sectionTitle = _sections[sectionIndex].title;
  }

  BOOL useCarousel = [self showHorizontalCarouselAtSection:sectionIndex];
  if (useCarousel) {
    CGFloat itemsCount = 1.0;
    if (sectionIndex < (NSInteger)_sections.count) {
      itemsCount = MAX(1.0, (CGFloat)_sections[sectionIndex].items.count);
    }

    CGFloat containerWidth = layoutEnvironment.container.contentSize.width;
    CGFloat availableWidth = containerWidth - kAttachmentSectionInsets.leading -
                             kAttachmentSectionInsets.trailing;
    CGFloat totalSpacing = (itemsCount - 1) * kAttachmentItemSpacing;
    CGFloat naturalItemWidth =
        AlignValueToLowerPixel((availableWidth - totalSpacing) / itemsCount);
    CGFloat attachmentItemMinimumWidth = IsComposeboxPlusButtonLargeCarousel()
                                             ? kAttachmentItemLargeMinimumWidth
                                             : kAttachmentItemMinimumWidth;
    CGFloat itemWidth = MAX(naturalItemWidth, attachmentItemMinimumWidth);

    CGFloat attachmentGroupEstimatedHeight =
        IsComposeboxPlusButtonLargeCarousel()
            ? kAttachmentGroupEstimatedLargeHeight
            : kAttachmentGroupEstimatedHeight;
    NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   absoluteDimension:itemWidth]
               heightDimension:
                   [NSCollectionLayoutDimension
                       estimatedDimension:attachmentGroupEstimatedHeight]];

    NSCollectionLayoutItem* item =
        [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

    NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   absoluteDimension:itemWidth]
               heightDimension:
                   [NSCollectionLayoutDimension
                       estimatedDimension:attachmentGroupEstimatedHeight]];
    NSCollectionLayoutGroup* group =
        [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                      subitems:@[ item ]];

    NSCollectionLayoutSection* section =
        [NSCollectionLayoutSection sectionWithGroup:group];
    section.contentInsets = kAttachmentSectionInsets;
    section.orthogonalScrollingBehavior =
        UICollectionLayoutSectionOrthogonalScrollingBehaviorContinuous;
    section.interGroupSpacing = kAttachmentItemSpacing;

    NSMutableArray<NSCollectionLayoutBoundarySupplementaryItem*>*
        supplementaryItems = [NSMutableArray array];

    if (sectionTitle) {
      UIFont* titleFont =
          [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      CGFloat fontHeight = titleFont.lineHeight;
      NSCollectionLayoutSize* headerSize = [NSCollectionLayoutSize
          sizeWithWidthDimension:[NSCollectionLayoutDimension
                                     fractionalWidthDimension:1.0]
                 heightDimension:[NSCollectionLayoutDimension
                                     estimatedDimension:fontHeight]];

      NSString* elementKind = UICollectionElementKindSectionHeader;
      NSCollectionLayoutBoundarySupplementaryItem* header =
          [NSCollectionLayoutBoundarySupplementaryItem
              boundarySupplementaryItemWithLayoutSize:headerSize
                                          elementKind:elementKind
                                            alignment:NSRectAlignmentTop];

      [supplementaryItems addObject:header];
    }

    if (_sections.count > 1) {
      NSCollectionLayoutSize* footerSize = [NSCollectionLayoutSize
          sizeWithWidthDimension:[NSCollectionLayoutDimension
                                     fractionalWidthDimension:1.0]
                 heightDimension:
                     [NSCollectionLayoutDimension
                         absoluteDimension:2 * kSeparatorVerticalPadding +
                                           kSeparatorHeight]];

      NSString* elementKind = UICollectionElementKindSectionFooter;
      NSCollectionLayoutBoundarySupplementaryItem* footer =
          [NSCollectionLayoutBoundarySupplementaryItem
              boundarySupplementaryItemWithLayoutSize:footerSize
                                          elementKind:elementKind
                                            alignment:NSRectAlignmentBottom];
      [supplementaryItems addObject:footer];
    }

    section.boundarySupplementaryItems = supplementaryItems;
    return section;
  } else {
    UICollectionLayoutListConfiguration* listConfig =
        [[UICollectionLayoutListConfiguration alloc]
            initWithAppearance:UICollectionLayoutListAppearanceInsetGrouped];
    listConfig.headerMode = sectionTitle
                                ? UICollectionLayoutListHeaderModeSupplementary
                                : UICollectionLayoutListHeaderModeNone;
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

// Whether the layout is split in two pages.
- (BOOL)isSplitPageLayout {
  return _menuPageType == ComposeboxMenuPageType::kMainPage ||
         _menuPageType == ComposeboxMenuPageType::kSecondaryPage;
}

// Focuses the first item in the menu for accessibility.
- (void)focusFirstMenuItem {
  if (_collectionView.numberOfSections > 0 &&
      [_collectionView numberOfItemsInSection:0] > 0) {
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
      initWithTitle:[self currentTabTitle]
              image:SymbolWithPointSize(SymbolGlobe, kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kCurrentTab
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kCurrentTab]
            favicon:_inputState.currentTabFavicon];
  ComposeboxMenuItem* tabsItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
              image:SymbolWithPointSize(SymbolNewTabGroupAction,
                                        kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentTabs
           disabled:[_inputState
                        isAttachmentDisabled:ComposeboxAttachmentOption::kTab]];
  ComposeboxMenuItem* cameraItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
              image:SymbolWithPointSize(SymbolSystemCamera,
                                        kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentCamera
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kCamera]];

  UIImage* galleryImage =
      IsPlusButtonMenuStringUpdatesEnabled()
          ? SymbolWithPointSize(SymbolPhotoBadgePlus, kSymbolActionPointSize)
          : SymbolWithPointSize(SymbolPhotoOnRectangleAngled,
                                kSymbolActionPointSize);
  ComposeboxMenuItem* galleryItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
              image:galleryImage
               type:ComposeboxMenuItemType::kAttachmentGallery
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kGallery]];
  ComposeboxMenuItem* filesItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION)
              image:SymbolWithPointSize(SymbolFolder, kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentFiles
           disabled:[_inputState isAttachmentDisabled:
                                     ComposeboxAttachmentOption::kFile]];

  if (_menuPageType == ComposeboxMenuPageType::kMainPage) {
    return @[ galleryItem, cameraItem, tabsItem ];
  }

  BOOL useUpdatedAttachmentsOrder = IsPlusButtonMenuVerticalListView() ||
                                    IsComposeboxPlusButtonLargeCarousel();
  NSMutableArray* attachmentItems;
  if (_menuPageType == ComposeboxMenuPageType::kSecondaryPage) {
    attachmentItems = [NSMutableArray arrayWithObjects:filesItem, nil];
  } else if (useUpdatedAttachmentsOrder) {
    attachmentItems =
        [NSMutableArray arrayWithObjects:currentTabItem, galleryItem,
                                         cameraItem, tabsItem, filesItem, nil];
  } else {
    attachmentItems =
        [NSMutableArray arrayWithObjects:currentTabItem, tabsItem, galleryItem,
                                         cameraItem, filesItem, nil];
  }

  if (IsComposeboxDriveOptionEnabled()) {
    UIImage* driveSymbol =
        SymbolWithPointSize(SymbolFolder, kSymbolActionPointSize);
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
    driveSymbol =
        SymbolWithPointSize(SymbolGoogleDrive, kSymbolActionPointSize);
#endif
    ComposeboxMenuItem* driveItem = [[ComposeboxMenuItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_DRIVE_ACTION)
                image:driveSymbol
                 type:ComposeboxMenuItemType::kAttachmentDrive
             disabled:[_inputState isAttachmentDisabled:
                                       ComposeboxAttachmentOption::kDrive]];
    [attachmentItems addObject:driveItem];
  }
  return attachmentItems;
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
  if (item.type == ComposeboxMenuItemType::kMoreOptionsSubmenu) {
    [self.delegate
        composeboxMenuPageViewControllerDidRequestSecondaryMenu:self];
    return;
  }

  if (item.type == ComposeboxMenuItemType::kModel) {
    // Ignore taps on already selected models.
    if (!item.modelItem.selected) {
      [self.mutator selectModelItem:item.modelItem];
    }
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
                  if ([weakSelf
                          showHorizontalCarouselAtSection:indexPath.section]) {
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
// Whether to show a horizontal carousel for the section at index path.
- (BOOL)showHorizontalCarouselAtSection:(NSInteger)section {
  return _sections[section].showAsHorizontalCarousel;
}

- (void)configureListCell:(ComposeboxMenuListCell*)cell
              atIndexPath:(NSIndexPath*)indexPath
                 withItem:(ComposeboxMenuItem*)item {
  UIListContentConfiguration* configuration =
      [cell defaultContentConfiguration];
  configuration.text = item.title;
  if (item.subtitle.length) {
    configuration.secondaryText = item.subtitle;
    configuration.secondaryTextProperties.color =
        [UIColor colorNamed:kTextSecondaryColor];
    configuration.secondaryTextProperties.numberOfLines = 0;
  }
  configuration.image = item.image;
  cell.accessibilityLabel = item.title;
  cell.isAccessibilityElement = YES;
  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  if (IsToolType(item.type)) {
    cell.accessibilityTraits |= UIAccessibilityTraitToggleButton;
  } else {
    cell.accessibilityTraits &= ~UIAccessibilityTraitToggleButton;
  }

  if (item.disabled) {
    configuration.textProperties.color =
        [UIColor colorNamed:kTextSecondaryColor];
    configuration.imageProperties.tintColor =
        [UIColor colorNamed:kTextSecondaryColor];
    cell.userInteractionEnabled = NO;
    cell.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  } else {
    configuration.textProperties.color = [UIColor colorNamed:kTextPrimaryColor];
    configuration.imageProperties.tintColor =
        [UIColor colorNamed:kTextPrimaryColor];
    cell.userInteractionEnabled = YES;
    cell.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
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
  } else if (item.type == ComposeboxMenuItemType::kModel &&
             item.modelItem.selected) {
    isSelected = YES;
  }

  NSMutableArray<UICellAccessory*>* accessories = [[NSMutableArray alloc] init];
  if (isSelected) {
    [accessories addObject:[[UICellAccessoryCheckmark alloc] init]];
    cell.accessibilityTraits |= UIAccessibilityTraitSelected;
  } else {
    cell.accessibilityTraits &= ~UIAccessibilityTraitSelected;
  }

  if (item.type == ComposeboxMenuItemType::kMoreOptionsSubmenu) {
    [accessories addObject:[[UICellAccessoryDisclosureIndicator alloc] init]];
  }

  if (item.type == ComposeboxMenuItemType::kAttachmentSharedTabs) {
    [accessories addObject:[[UICellAccessoryDisclosureIndicator alloc] init]];
    // TODO(crbug.com/568743843): Update `ComposeboxFaviconsAccordionView` badge
    // background and border colors so the overflow badge contrasts with the
    // cell's `kGroupedSecondaryBackgroundColor` in Dark mode.
    ComposeboxFaviconsAccordionView* faviconsView =
        [[ComposeboxFaviconsAccordionView alloc] initWithFrame:CGRectZero];
    faviconsView.translatesAutoresizingMaskIntoConstraints = YES;
    faviconsView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
    [cell.traitCollection performAsCurrentTraitCollection:^{
      [faviconsView updateWithImages:item.favicons];
    }];
    UICellAccessoryCustomView* customViewAccessory =
        [[UICellAccessoryCustomView alloc]
            initWithCustomView:faviconsView
                     placement:UICellAccessoryPlacementTrailing];
    // Use the actual width of the favicons stack instead of the standard
    // reserved layout width so multi-icon piles do not overflow into the
    // disclosure indicator.
    customViewAccessory.reservedLayoutWidth = 0;
    [accessories addObject:customViewAccessory];
  }
  cell.accessories = accessories;
  cell.accessibilityIdentifier =
      AccessibilityIdentifierForMenuItemType(item.type);
}

// Returns the shared tabs section if available.
- (ComposeboxMenuSection*)attachmentsSection {
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

  if (attachmentsItems.count == 0) {
    return nil;
  }

  BOOL pageEligibleForCarouselView =
      _menuPageType == ComposeboxMenuPageType::kFullMenu ||
      _menuPageType == ComposeboxMenuPageType::kMainPage;
  BOOL showAsHorizontalCarousel =
      !IsPlusButtonMenuVerticalListView() && pageEligibleForCarouselView;

  BOOL showSectionTitle =
      IsPlusButtonMenuStringUpdatesEnabled() &&
      _menuPageType != ComposeboxMenuPageType::kSecondaryPage;
  NSString* title =
      showSectionTitle ? _inputState.uiConfig.attachmentsSectionHeader : nil;

  return [[ComposeboxMenuSection alloc]
                 initWithTitle:title
                         items:attachmentsItems
                    identifier:ComposeboxMenuSectionIdentifier::kAttachments
      showAsHorizontalCarousel:showAsHorizontalCarousel];
}

// Returns the shared tabs section if available.
- (ComposeboxMenuSection*)sharedTabsSection {
  if (_menuPageType != ComposeboxMenuPageType::kFullMenu &&
      _menuPageType != ComposeboxMenuPageType::kMainPage) {
    return nil;
  }

  if (_inputState.sharedTabs.count == 0) {
    return nil;
  }

  NSMutableArray<NSString*>* tabDomains = [[NSMutableArray alloc] init];
  NSMutableArray<UIImage*>* favicons = [[NSMutableArray alloc] init];
  for (ComposeboxMenuSharedTab* tab in _inputState.sharedTabs) {
    if (tab.URL.is_valid() && !tab.URL.host().empty()) {
      std::u16string elidedHost = url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              tab.URL);
      [tabDomains addObject:base::SysUTF16ToNSString(elidedHost)];
    }
    UIImage* favicon =
        tab.favicon
            ?: SymbolWithPointSize(SymbolGlobe, kSharedTabFaviconSymbolSize);
    [favicons addObject:favicon];
  }
  NSString* subtitle = [tabDomains componentsJoinedByString:@", "];

  ComposeboxMenuItem* sharedTabsItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_SHARED_TABS)
           subtitle:subtitle
           favicons:favicons
               type:ComposeboxMenuItemType::kAttachmentSharedTabs];

  return [[ComposeboxMenuSection alloc]
      initWithTitle:nil
              items:@[ sharedTabsItem ]
         identifier:ComposeboxMenuSectionIdentifier::kSharedTabs];
}

// Returns the tools section if there are tools available.
- (ComposeboxMenuSection*)toolsSection {
  if (_menuPageType != ComposeboxMenuPageType::kFullMenu &&
      _menuPageType != ComposeboxMenuPageType::kSecondaryPage) {
    return nil;
  }
  NSMutableArray<ComposeboxMenuItem*>* toolsItems =
      [[NSMutableArray alloc] init];

  ComposeboxUIConfig* uiConfig = _inputState.uiConfig;
  for (ComposeboxMode mode : ComposeboxModeSet::All()) {
    if (mode == ComposeboxMode::kRegularSearch) {
      continue;
    }
    // AIM is shown in a separate section.
    if (mode == ComposeboxMode::kAIM && [self isSplitPageLayout]) {
      continue;
    }
    if (![_inputState isToolHidden:mode]) {
      [toolsItems
          addObject:[[ComposeboxMenuItem alloc]
                        initWithTitle:[uiConfig menuLabelForTool:mode]
                             subtitle:[uiConfig menuSubtitleForTool:mode]
                                image:[uiConfig iconForTool:mode]
                                 type:MenuItemTypeForTool(mode)
                             disabled:[_inputState isToolDisabled:mode]]];
    }
  }

  if (toolsItems.count == 0) {
    return nil;
  }

  BOOL showSectionTitle =
      _menuPageType != ComposeboxMenuPageType::kSecondaryPage;
  NSString* title =
      showSectionTitle ? _inputState.uiConfig.toolsSectionHeader : nil;

  return [[ComposeboxMenuSection alloc]
      initWithTitle:title
              items:toolsItems
         identifier:ComposeboxMenuSectionIdentifier::kTools];
}

// Returns the models section if there are models available.
- (ComposeboxMenuSection*)modelsSection {
  if (_menuPageType != ComposeboxMenuPageType::kFullMenu &&
      _menuPageType != ComposeboxMenuPageType::kSecondaryPage) {
    return nil;
  }

  NSMutableArray<ComposeboxMenuItem*>* modelsItems =
      [[NSMutableArray alloc] init];

  for (ComposeboxModelItem* modelItem in _inputState.modelItems) {
    [modelsItems
        addObject:[[ComposeboxMenuItem alloc] initWithModelItem:modelItem]];
  }

  if (modelsItems.count == 0) {
    return nil;
  }

  BOOL showSectionTitle =
      _menuPageType != ComposeboxMenuPageType::kSecondaryPage;
  NSString* title =
      showSectionTitle ? _inputState.uiConfig.modelSectionHeader : nil;

  return [[ComposeboxMenuSection alloc]
      initWithTitle:title
              items:modelsItems
         identifier:ComposeboxMenuSectionIdentifier::kModels];
}

- (ComposeboxMenuSection*)moreOptionsSection {
  if (_menuPageType != ComposeboxMenuPageType::kMainPage) {
    return nil;
  }

  NSMutableArray<ComposeboxMenuItem*>* moreOptionsItems =
      [[NSMutableArray alloc] init];

  if (!IsPlusButtonMenuVerticalListView() &&
      ![_inputState
          isAttachmentHidden:ComposeboxAttachmentOption::kCurrentTab]) {
    ComposeboxMenuItem* currentTabItem = [[ComposeboxMenuItem alloc]
        initWithTitle:[self currentTabTitle]
                image:[self currentTabIcon]
                 type:ComposeboxMenuItemType::kCurrentTab
             disabled:[_inputState
                          isAttachmentDisabled:ComposeboxAttachmentOption::
                                                   kCurrentTab]];
    [moreOptionsItems addObject:currentTabItem];
  }

  ComposeboxUIConfig* uiConfig = _inputState.uiConfig;
  if (![_inputState isToolHidden:ComposeboxMode::kAIM]) {
    ComposeboxMenuItem* aimItem = [[ComposeboxMenuItem alloc]
        initWithTitle:[uiConfig menuLabelForTool:ComposeboxMode::kAIM]
             subtitle:[uiConfig menuSubtitleForTool:ComposeboxMode::kAIM]
                image:[uiConfig iconForTool:ComposeboxMode::kAIM]
                 type:MenuItemTypeForTool(ComposeboxMode::kAIM)
             disabled:NO];
    [moreOptionsItems addObject:aimItem];
  }

  ComposeboxMenuItem* moreOptionsItem = [[ComposeboxMenuItem alloc]
      initWithTitle:uiConfig.moreOptionsSectionHeader
              image:SymbolWithPointSize(SymbolEllipsisCircle,
                                        kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kMoreOptionsSubmenu];
  [moreOptionsItems addObject:moreOptionsItem];

  return [[ComposeboxMenuSection alloc]
      initWithTitle:nil
              items:moreOptionsItems
         identifier:ComposeboxMenuSectionIdentifier::kMoreOptionsSubmenu];
}

// Returns the section containing only the current tab section, as a separate
// item.
- (ComposeboxMenuSection*)currentTabSection {
  BOOL currentTabHidden =
      [_inputState isAttachmentHidden:ComposeboxAttachmentOption::kCurrentTab];
  if (!IsPlusButtonMenuVerticalListView() || currentTabHidden ||
      _menuPageType == ComposeboxMenuPageType::kSecondaryPage) {
    return nil;
  }

  NSArray<ComposeboxMenuItem*>* items = @[

    [[ComposeboxMenuItem alloc]
        initWithTitle:[self currentTabTitle]
                image:[self currentTabIcon]
                 type:ComposeboxMenuItemType::kCurrentTab
             disabled:[_inputState isAttachmentDisabled:
                                       ComposeboxAttachmentOption::kCurrentTab]]
  ];

  return [[ComposeboxMenuSection alloc]
      initWithTitle:nil
              items:items
         identifier:ComposeboxMenuSectionIdentifier::kCurrentTab];
}

// The title used for the current tab item.
- (NSString*)currentTabTitle {
  return IsPlusButtonMenuStringUpdatesEnabled()
             ? l10n_util::GetNSString(
                   IDS_IOS_COMPOSEBOX_ASK_ABOUT_THIS_PAGE_ACTION)
             : l10n_util::GetNSString(
                   IDS_IOS_COMPOSEBOX_MENU_ADD_CURRENT_TAB_ACTION);
}

// The icon for the current tab.
- (UIImage*)currentTabIcon {
  UIImage* icon =
      ResizeImage(_inputState.currentTabFavicon,
                  CGSizeMake(kSymbolActionPointSize, kSymbolActionPointSize),
                  ProjectionMode::kAspectFill);
  if (icon) {
    return icon;
  }

  return SymbolWithPointSize(SymbolGlobe, kSymbolActionPointSize);
}

@end
