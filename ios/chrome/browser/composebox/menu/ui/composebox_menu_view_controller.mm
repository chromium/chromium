// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import <optional>

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/ui/composebox_strings.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_input_state.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height of the attachments group.
const CGFloat kAttachmentGroupHeight = 80.0f;

// Insets for the safe area (top, left, bottom, right).
const UIEdgeInsets kSafeAreaInsets = {20.0, 15.0, 20.0, 15.0};

// Trailing inset for the attachment item.
const CGFloat kAttachmentItemTrailingInset = 6.0f;

// Insets for the model and tools sections.
const NSDirectionalEdgeInsets kListSectionInsets = {0, 15.0, 20.0, 15.0};

// Insets for the attachments section.
const NSDirectionalEdgeInsets kAttachmentSectionInsets = {20.0, 0, 20.0, 0};

// Leading constant for the header label.
const CGFloat kHeaderLabelLeadingPadding = 15.0f;

// Vertical constant for the header label.
const CGFloat kHeaderLabelVerticalPadding = 10.0f;

// Font size for the header label.
const CGFloat kHeaderLabelFontSize = 16.0f;

// Composebox menu section identifier.
enum class ComposeboxMenuSectionIdentifier {
  kAttachments = 0,
  kTools,
  kModels,
};

// Maps a menu item type to its corresponding attachment option.
std::optional<ComposeboxAttachmentOption> AttachmentOptionForMenuItemType(
    ComposeboxMenuItemType type) {
  switch (type) {
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
    case ComposeboxModelOption::kThinkingNoGenUI:
      return ComposeboxMenuItemType::kModelThinking;
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
  [self setAdditionalSafeAreaInsets:kSafeAreaInsets];
  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];

  [self setUpCollectionView];
  [self setUpDataSource];
  [self applySnapshot];
}

- (CGSize)preferredContentSize {
  CGSize size = super.preferredContentSize;
  [self.view layoutIfNeeded];
  size.height = _collectionView.contentSize.height +
                _collectionView.contentInset.top +
                _collectionView.contentInset.bottom;
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
    ComposeboxMenuSection* attachmentsSection =
        [[ComposeboxMenuSection alloc] initWithTitle:nil
                                               items:attachmentsItems];
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
    ComposeboxMenuSection* toolsSection =
        [[ComposeboxMenuSection alloc] initWithTitle:strings.toolsSectionHeader
                                               items:toolsItems];
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
    ComposeboxMenuSection* modelsSection =
        [[ComposeboxMenuSection alloc] initWithTitle:strings.modelSectionHeader
                                               items:modelsItems];
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
  _collectionView.backgroundColor = [UIColor colorNamed:kGrey100Color];
  _collectionView.showsVerticalScrollIndicator = NO;
  _collectionView.showsHorizontalScrollIndicator = NO;

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
  if (sectionIndex ==
      static_cast<NSInteger>(ComposeboxMenuSectionIdentifier::kAttachments)) {
    CGFloat itemsCount = MAX(1.0, (CGFloat)_sections[sectionIndex].items.count);
    CGFloat fractionalWidth = 1.0 / itemsCount;

    NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:fractionalWidth]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.0]];
    NSCollectionLayoutItem* item =
        [NSCollectionLayoutItem itemWithLayoutSize:itemSize];
    item.contentInsets =
        NSDirectionalEdgeInsetsMake(0, 0, 0, kAttachmentItemTrailingInset);

    NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:1.0]
               heightDimension:[NSCollectionLayoutDimension
                                   absoluteDimension:kAttachmentGroupHeight]];
    NSCollectionLayoutGroup* group =
        [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                      subitems:@[ item ]];

    NSCollectionLayoutSection* section =
        [NSCollectionLayoutSection sectionWithGroup:group];
    section.contentInsets = kAttachmentSectionInsets;
    section.orthogonalScrollingBehavior =
        UICollectionLayoutSectionOrthogonalScrollingBehaviorContinuous;
    return section;
  } else {
    UICollectionLayoutListConfiguration* listConfig =
        [[UICollectionLayoutListConfiguration alloc]
            initWithAppearance:UICollectionLayoutListAppearanceInsetGrouped];
    listConfig.headerMode = UICollectionLayoutListHeaderModeSupplementary;
    NSCollectionLayoutSection* section = [NSCollectionLayoutSection
        sectionWithListConfiguration:listConfig
                   layoutEnvironment:layoutEnvironment];
    section.contentInsets = kListSectionInsets;
    return section;
  }
}

#pragma mark - Private

- (NSArray<ComposeboxMenuItem*>*)availableAttachmentItems {
  CHECK(_inputState);
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

  return @[ tabsItem, cameraItem, galleryItem, filesItem ];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [collectionView deselectItemAtIndexPath:indexPath animated:YES];

  ComposeboxMenuItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  if (!item || item.disabled) {
    return;
  }

  [self.mutator handleItemPickedWithType:item.type];
}

#pragma mark - Data Source Helpers

- (void)setUpDataSource {
  __weak __typeof(self) weakSelf = self;

  UICollectionViewCellRegistration* listCellRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[UICollectionViewListCell class]
               configurationHandler:^(UICollectionViewListCell* cell,
                                      NSIndexPath* indexPath,
                                      ComposeboxMenuItem* item) {
                 [weakSelf configureListCell:cell
                                 atIndexPath:indexPath
                                    withItem:item];
               }];

  UICollectionViewCellRegistration* attachmentCellRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[UICollectionViewCell class]
               configurationHandler:^(UICollectionViewCell* cell,
                                      NSIndexPath* indexPath,
                                      ComposeboxMenuItem* item) {
                 [weakSelf configureAttachmentCell:cell
                                       atIndexPath:indexPath
                                          withItem:item];
               }];

  UICollectionViewSupplementaryRegistration* headerRegistration =
      [UICollectionViewSupplementaryRegistration
          registrationWithSupplementaryClass:[UICollectionReusableView class]
                                 elementKind:
                                     UICollectionElementKindSectionHeader
                        configurationHandler:^(UICollectionReusableView* view,
                                               NSString* elementKind,
                                               NSIndexPath* indexPath) {
                          [weakSelf configureHeaderView:view
                                            atIndexPath:indexPath];
                        }];

  _dataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    ComposeboxMenuItem* item) {
                  if (indexPath.section ==
                      static_cast<NSInteger>(
                          ComposeboxMenuSectionIdentifier::kAttachments)) {
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
    if ([elementKind isEqualToString:UICollectionElementKindSectionHeader] &&
        indexPath.section > 0) {
      return [collectionView
          dequeueConfiguredReusableSupplementaryViewWithRegistration:
              headerRegistration
                                                        forIndexPath:indexPath];
    }
    return nil;
  };
}

- (void)applySnapshot {
  NSDiffableDataSourceSnapshot<NSNumber*, ComposeboxMenuItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  // Attachments
  if (_sections.count > 0) {
    NSNumber* attachmentsIdentifier = @(
        static_cast<NSInteger>(ComposeboxMenuSectionIdentifier::kAttachments));
    [snapshot appendSectionsWithIdentifiers:@[ attachmentsIdentifier ]];
    [snapshot appendItemsWithIdentifiers:_sections[0].items
               intoSectionWithIdentifier:attachmentsIdentifier];
  }

  // Tools
  if (_sections.count > 1) {
    NSNumber* toolsIdentifier =
        @(static_cast<NSInteger>(ComposeboxMenuSectionIdentifier::kTools));
    [snapshot appendSectionsWithIdentifiers:@[ toolsIdentifier ]];
    [snapshot appendItemsWithIdentifiers:_sections[1].items
               intoSectionWithIdentifier:toolsIdentifier];
  }

  // Models
  if (_sections.count > 2) {
    NSNumber* modelsIdentifier =
        @(static_cast<NSInteger>(ComposeboxMenuSectionIdentifier::kModels));
    [snapshot appendSectionsWithIdentifiers:@[ modelsIdentifier ]];
    [snapshot appendItemsWithIdentifiers:_sections[2].items
               intoSectionWithIdentifier:modelsIdentifier];
  }

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - ComposeboxMenuConsumer

- (void)setUIInputState:(ComposeboxUIInputState*)state {
  _inputState = state;
  [self computeSections];
}

#pragma mark - Private Configuration Helpers

- (void)configureListCell:(UICollectionViewListCell*)cell
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
  } else {
    configuration.textProperties.color = [UIColor colorNamed:kTextPrimaryColor];
    configuration.imageProperties.tintColor =
        [UIColor colorNamed:kTextPrimaryColor];
    cell.userInteractionEnabled = YES;
  }

  cell.contentConfiguration = configuration;

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
}

- (void)configureAttachmentCell:(UICollectionViewCell*)cell
                    atIndexPath:(NSIndexPath*)indexPath
                       withItem:(ComposeboxMenuItem*)item {
  ComposeboxMenuAttachmentView* attachmentView =
      [[ComposeboxMenuAttachmentView alloc] init];
  attachmentView.translatesAutoresizingMaskIntoConstraints = NO;
  attachmentView.title = item.title;
  if (item.disabled) {
    attachmentView.image = SymbolWithPalette(
        item.image, @[ [UIColor colorNamed:kTextSecondaryColor] ]);
    attachmentView.alpha = 0.5;
    cell.userInteractionEnabled = NO;
  } else {
    attachmentView.image = SymbolWithPalette(
        item.image, @[ [UIColor colorNamed:kTextPrimaryColor] ]);
    attachmentView.alpha = 1.0;
    cell.userInteractionEnabled = YES;
  }

  attachmentView.userInteractionEnabled = NO;
  attachmentView.accessibilityLabel = item.title;

  [cell.contentView addSubview:attachmentView];
  AddSameConstraints(attachmentView, cell.contentView);
}

- (void)configureHeaderView:(UICollectionReusableView*)view
                atIndexPath:(NSIndexPath*)indexPath {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont systemFontOfSize:kHeaderLabelFontSize
                                 weight:UIFontWeightBold];
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.text = _sections[indexPath.section].title;

  [view addSubview:label];

  [NSLayoutConstraint activateConstraints:@[
    [label.leadingAnchor constraintEqualToAnchor:view.leadingAnchor
                                        constant:kHeaderLabelLeadingPadding],
    [label.topAnchor constraintEqualToAnchor:view.topAnchor
                                    constant:kHeaderLabelVerticalPadding],
    [label.bottomAnchor constraintEqualToAnchor:view.bottomAnchor
                                       constant:-kHeaderLabelVerticalPadding],
  ]];
}

@end
