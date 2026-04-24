// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"
#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_section.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height of the attachments stack view.
const CGFloat kAttachmentStackViewHeight = 80.0f;

// Insets for the safe area (top, left, bottom, right).
const UIEdgeInsets kSafeAreaInsets = {20.0, 15.0, 20.0, 15.0};

// Fractional width of the attachment item.
const CGFloat kAttachmentItemFractionalWidth = 0.25f;

// Trailing inset for the attachment item.
const CGFloat kAttachmentItemTrailingInset = 6.0f;

// Insets for the attachments section.
const NSDirectionalEdgeInsets kAttachmentsSectionInsets = {20.0, 15.0, 20.0,
                                                           15.0};

// Leading constant for the header label.
const CGFloat kHeaderLabelLeadingPadding = 15.0f;

// Vertical constant for the header label.
const CGFloat kHeaderLabelVerticalPadding = 10.0f;

// Composebox menu section identifier.
enum class ComposeboxMenuSectionIdentifier {
  kAttachments = 0,
  kTools,
  kModels,
};

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
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setAdditionalSafeAreaInsets:kSafeAreaInsets];
  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];

  [self setUpSections];
  [self setUpCollectionView];
  [self setUpDataSource];
  [self applyInitialSnapshot];
}

- (void)setUpSections {
  ComposeboxMenuSection* attachmentsSection = [[ComposeboxMenuSection alloc]
      initWithTitle:nil
              items:[self availableAttachmentItems]];

  ComposeboxMenuItem* aimItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_AIM_ACTION)
              image:CustomSymbolWithPointSize(kMagnifyingglassSparkSymbol,
                                              kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAIM];
  ComposeboxMenuItem* createImageItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_CREATE_IMAGE_ACTION)
              image:GetBananaIcon(kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kCreateImage];
  ComposeboxMenuItem* deepSearchItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_DEEP_SEARCH_ACTION)
              image:CustomSymbolWithPointSize(kDeepSearchSymbol,
                                              kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kDeepSearch];
  ComposeboxMenuItem* canvasItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CANVAS_ACTION)
              image:CustomSymbolWithPointSize(kDocumentBadgeSpark,
                                              kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kCanvas];

  // TODO(crbug.com/506070697): Integrate with server side strings.
  ComposeboxMenuItem* regularModelItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO)
              image:DefaultSymbolWithPointSize(kBoltSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kModelRegular];

  ComposeboxMenuItem* autoItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_AUTO)
              image:DefaultSymbolWithPointSize(
                        kClockArrowTriangleheadCounterclockwiseRotate90Symbol,
                        kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kModelAuto];
  ComposeboxMenuItem* thinkingItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_OPTION_THINKING)
              image:DefaultSymbolWithPointSize(kClockSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kModelThinking];

  ComposeboxMenuSection* toolsSection = [[ComposeboxMenuSection alloc]
      initWithTitle:@"Tools"  // TODO (crbug.com/504976247): Add the translated
                              // string.
              items:@[ aimItem, createImageItem, deepSearchItem, canvasItem ]];

  ComposeboxMenuSection* modelsSection = [[ComposeboxMenuSection alloc]
      initWithTitle:l10n_util::GetNSStringF(
                        IDS_IOS_COMPOSEBOX_MODEL_SELECTOR_TITLE, u"3")
              items:@[ regularModelItem, autoItem, thinkingItem ]];

  _sections = @[ attachmentsSection, toolsSection, modelsSection ];
}

- (void)setUpCollectionView {
  _collectionView =
      [[UICollectionView alloc] initWithFrame:self.view.bounds
                         collectionViewLayout:[self createLayout]];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;
  _collectionView.backgroundColor = [UIColor colorNamed:kGrey100Color];

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
    // Attachments section (Horizontal)
    NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:
            [NSCollectionLayoutDimension
                fractionalWidthDimension:kAttachmentItemFractionalWidth]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.0]];
    NSCollectionLayoutItem* item =
        [NSCollectionLayoutItem itemWithLayoutSize:itemSize];
    item.contentInsets =
        NSDirectionalEdgeInsetsMake(0, 0, 0, kAttachmentItemTrailingInset);

    NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:1.0]
               heightDimension:
                   [NSCollectionLayoutDimension
                       absoluteDimension:kAttachmentStackViewHeight]];
    NSCollectionLayoutGroup* group =
        [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                      subitems:@[ item ]];

    NSCollectionLayoutSection* section =
        [NSCollectionLayoutSection sectionWithGroup:group];
    section.contentInsets = kAttachmentsSectionInsets;
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
    return section;
  }
}

#pragma mark - Private

- (NSArray<ComposeboxMenuItem*>*)availableAttachmentItems {
  ComposeboxMenuItem* tabsItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
              image:DefaultSymbolWithPointSize(kNewTabGroupActionSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentTabs];
  ComposeboxMenuItem* cameraItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
              image:DefaultSymbolWithPointSize(kSystemCameraSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentCamera];
  ComposeboxMenuItem* galleryItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
              image:DefaultSymbolWithPointSize(kPhotoOnRectangleAngled,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentGallery];
  ComposeboxMenuItem* filesItem = [[ComposeboxMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_FILES_ACTION)
              image:DefaultSymbolWithPointSize(kFolderSymbol,
                                               kSymbolActionPointSize)
               type:ComposeboxMenuItemType::kAttachmentFiles];

  return @[ tabsItem, cameraItem, galleryItem, filesItem ];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [collectionView deselectItemAtIndexPath:indexPath animated:YES];

  ComposeboxMenuItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  if (!item) {
    return;
  }

  // TODO (crbug.com/505269628): Implement menu items selection.
  switch (item.type) {
    case ComposeboxMenuItemType::kAIM:
      break;
    case ComposeboxMenuItemType::kCreateImage:
      break;
    case ComposeboxMenuItemType::kDeepSearch:
      break;
    case ComposeboxMenuItemType::kCanvas:
      break;
    case ComposeboxMenuItemType::kModelRegular:
      break;
    case ComposeboxMenuItemType::kModelAuto:
      break;
    case ComposeboxMenuItemType::kModelThinking:
      break;
    case ComposeboxMenuItemType::kAttachmentTabs:
      break;
    case ComposeboxMenuItemType::kAttachmentCamera:
      break;
    case ComposeboxMenuItemType::kAttachmentGallery:
      break;
    case ComposeboxMenuItemType::kAttachmentFiles:
      break;
    case ComposeboxMenuItemType::kUnknown:
      break;
  }
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

- (void)applyInitialSnapshot {
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

#pragma mark - Private Configuration Helpers

- (void)configureListCell:(UICollectionViewListCell*)cell
              atIndexPath:(NSIndexPath*)indexPath
                 withItem:(ComposeboxMenuItem*)item {
  UIListContentConfiguration* configuration =
      [cell defaultContentConfiguration];
  configuration.text = item.title;
  configuration.image = item.image;
  configuration.imageProperties.tintColor =
      [UIColor colorNamed:kTextPrimaryColor];
  cell.contentConfiguration = configuration;
}

- (void)configureAttachmentCell:(UICollectionViewCell*)cell
                    atIndexPath:(NSIndexPath*)indexPath
                       withItem:(ComposeboxMenuItem*)item {
  ComposeboxMenuAttachmentView* attachmentView =
      [[ComposeboxMenuAttachmentView alloc] init];
  attachmentView.translatesAutoresizingMaskIntoConstraints = NO;
  attachmentView.title = item.title;
  attachmentView.image = SymbolWithPalette(
      item.image, @[ [UIColor colorNamed:kTextPrimaryColor] ]);

  [cell.contentView addSubview:attachmentView];
  AddSameConstraints(attachmentView, cell.contentView);
}

- (void)configureHeaderView:(UICollectionReusableView*)view
                atIndexPath:(NSIndexPath*)indexPath {
  UILabel* label = [[UILabel alloc] init];
  label.font = [UIFont systemFontOfSize:16.0 weight:UIFontWeightBold];
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
