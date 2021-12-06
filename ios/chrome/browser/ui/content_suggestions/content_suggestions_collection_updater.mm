// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
using CSCollectionViewModel = CollectionViewModel<CSCollectionViewItem*>;

// Enum defining the ItemType of this ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooter = kItemTypeEnumZero,
  ItemTypeHeader,
  ItemTypeEmpty,
  ItemTypeMostVisited,
  ItemTypePromo,
  ItemTypeReturnToRecentTab,
  ItemTypeUnknown,
};

// Enum defining the SectionIdentifier of this
// ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMostVisited = kSectionIdentifierEnumZero,
  SectionIdentifierLogo,
  SectionIdentifierReturnToRecentTab,
  SectionIdentifierPromo,
  SectionIdentifierDefault,
};

// Returns the ContentSuggestionType associated with an ItemType |type|.
ContentSuggestionType ContentSuggestionTypeForItemType(NSInteger type) {
  if (type == ItemTypeEmpty)
    return ContentSuggestionTypeEmpty;
  if (type == ItemTypeReturnToRecentTab)
    return ContentSuggestionTypeReturnToRecentTab;
  if (type == ItemTypeMostVisited)
    return ContentSuggestionTypeMostVisited;
  if (type == ItemTypePromo)
    return ContentSuggestionTypePromo;
  // Add new type here

  // Default type.
  return ContentSuggestionTypeEmpty;
}

// Returns the item type corresponding to the section |info|.
ItemType ItemTypeForInfo(ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionReturnToRecentTab:
      return ItemTypeReturnToRecentTab;
    case ContentSuggestionsSectionMostVisited:
      return ItemTypeMostVisited;
    case ContentSuggestionsSectionPromo:
      return ItemTypePromo;
    case ContentSuggestionsSectionLogo:
    case ContentSuggestionsSectionUnknown:
      return ItemTypeUnknown;
  }
}

// Returns the section identifier corresponding to the section |info|.
SectionIdentifier SectionIdentifierForInfo(
    ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionMostVisited:
      return SectionIdentifierMostVisited;
    case ContentSuggestionsSectionLogo:
      return SectionIdentifierLogo;
    case ContentSuggestionsSectionReturnToRecentTab:
      return SectionIdentifierReturnToRecentTab;
    case ContentSuggestionsSectionPromo:
      return SectionIdentifierPromo;
    case ContentSuggestionsSectionUnknown:
      return SectionIdentifierDefault;
  }
}

NSString* const kContentSuggestionsCollectionUpdaterSnackbarCategory =
    @"ContentSuggestionsCollectionUpdaterSnackbarCategory";

}  // namespace

@interface ContentSuggestionsCollectionUpdater ()<ContentSuggestionsDataSink>

@property(nonatomic, strong)
    NSMutableDictionary<NSNumber*, ContentSuggestionsSectionInformation*>*
        sectionInfoBySectionIdentifier;
// Width of the collection. Upon size change, it reflects the new size.
@property(nonatomic, assign) CGFloat collectionWidth;
// Whether an item of type ItemTypePromo has already been added to the model.
@property(nonatomic, assign) BOOL promoAdded;
// All SectionIdentifier from ContentSuggestions.
@property(nonatomic, strong)
    NSMutableSet<NSNumber*>* sectionIdentifiersFromContentSuggestions;

@end

@implementation ContentSuggestionsCollectionUpdater

@synthesize collectionViewController = _collectionViewController;
@synthesize dataSource = _dataSource;
@synthesize sectionInfoBySectionIdentifier = _sectionInfoBySectionIdentifier;
@synthesize collectionWidth = _collectionWidth;
@synthesize promoAdded = _promoAdded;
@synthesize sectionIdentifiersFromContentSuggestions =
    _sectionIdentifiersFromContentSuggestions;
@synthesize dispatcher = _dispatcher;

- (instancetype)init {
  self = [super init];
  if (self) {
    _promoAdded = NO;
  }
  return self;
}

#pragma mark - Properties

- (void)setCollectionViewController:
    (ContentSuggestionsViewController*)collectionViewController {
  _collectionViewController = collectionViewController;
  self.collectionWidth =
      collectionViewController.collectionView.bounds.size.width;

  if (self.dataSource)
    [self reloadAllData];
}

- (void)setDataSource:(id<ContentSuggestionsDataSource>)dataSource {
  _dataSource = dataSource;
  dataSource.dataSink = self;

  if (self.collectionViewController)
    [self reloadAllData];
}

#pragma mark - ContentSuggestionsDataSink

- (void)dataAvailableForSection:
            (ContentSuggestionsSectionInformation*)sectionInfo
                    forceReload:(BOOL)forceReload {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;

  if (forceReload && [model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    NSInteger numberOfItems = [model numberOfItemsInSection:section];
    NSMutableArray<NSIndexPath*>* indexesToDelete = [NSMutableArray array];
    for (NSInteger i = 0; i < numberOfItems; i++) {
      [indexesToDelete
          addObject:[NSIndexPath indexPathForItem:i inSection:section]];
    }

    UICollectionView* collection = self.collectionViewController.collectionView;
    // Delete all the items manually to avoid adding an empty item.
    [collection performBatchUpdates:^{
      [self.collectionViewController collectionView:collection
                        willDeleteItemsAtIndexPaths:indexesToDelete];
      [collection deleteItemsAtIndexPaths:indexesToDelete];
    }
                         completion:nil];
  }

  if ([model hasSectionForSectionIdentifier:sectionIdentifier]) {
    NSArray<CSCollectionViewItem*>* items =
        [model itemsInSectionWithIdentifier:sectionIdentifier];
    if (items.count > 0 && items[0].type != ItemTypeEmpty) {
      // Do not dismiss the presented items.
      return;
    }
  }

  [self.collectionViewController
      addSuggestions:[self.dataSource itemsForSectionInfo:sectionInfo]
       toSectionInfo:sectionInfo];
}

- (void)clearSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
  SectionIdentifier sectionIdentifier =
      SectionIdentifierForInfo(suggestionIdentifier.sectionInfo);
  if (![self.collectionViewController.collectionViewModel
          hasSectionForSectionIdentifier:sectionIdentifier]) {
    return;
  }

  NSArray<CSCollectionViewItem*>* itemsInSection =
      [self.collectionViewController.collectionViewModel
          itemsInSectionWithIdentifier:sectionIdentifier];

  CSCollectionViewItem* correspondingItem = nil;
  for (CSCollectionViewItem* item in itemsInSection) {
    if (item.suggestionIdentifier == suggestionIdentifier) {
      correspondingItem = item;
      break;
    }
  }

  if (!correspondingItem)
    return;

  NSIndexPath* indexPath = [self.collectionViewController.collectionViewModel
      indexPathForItem:correspondingItem];
  [self.collectionViewController dismissEntryAtIndexPath:indexPath];
}

- (void)reloadAllData {
  [self resetModels];

  // The data is reset, add the new data directly in the model then reload the
  // collection.
  NSArray<ContentSuggestionsSectionInformation*>* sectionInfos =
      [self.dataSource sectionsInfo];
  [self addSectionsForSectionInfoToModel:sectionInfos];
  for (ContentSuggestionsSectionInformation* sectionInfo in sectionInfos) {
    [self
        addSuggestionsToModel:[self.dataSource itemsForSectionInfo:sectionInfo]
              withSectionInfo:sectionInfo];
  }
  [self.collectionViewController.collectionView reloadData];
}

- (void)addSection:(ContentSuggestionsSectionInformation*)sectionInfo
        completion:(void (^)(void))completion {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;

  if ([model hasSectionForSectionIdentifier:sectionIdentifier])
    return;

  auto addSectionBlock = ^{
    NSIndexSet* addedSection =
        [self addSectionsForSectionInfoToModel:@[ sectionInfo ]];
    [self.collectionViewController.collectionView insertSections:addedSection];
    NSArray<NSIndexPath*>* addedItems = [self
        addSuggestionsToModel:[self.dataSource itemsForSectionInfo:sectionInfo]
              withSectionInfo:sectionInfo];
    [self.collectionViewController.collectionView
        insertItemsAtIndexPaths:addedItems];
  };

  [UIView performWithoutAnimation:^{
    [self.collectionViewController.collectionView
        performBatchUpdates:addSectionBlock
                 completion:^(BOOL finished) {
                   completion();
                 }];
  }];
}

- (void)clearSection:(ContentSuggestionsSectionInformation*)sectionInfo {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;

  if (![model hasSectionForSectionIdentifier:sectionIdentifier])
    return;

  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];

  [self.collectionViewController dismissSection:section];
}

- (void)reloadSection:(ContentSuggestionsSectionInformation*)sectionInfo {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (![model hasSectionForSectionIdentifier:sectionIdentifier]) {
    [self.collectionViewController
        addSuggestions:[self.dataSource itemsForSectionInfo:sectionInfo]
         toSectionInfo:sectionInfo];
    return;
  }

  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];

  // Reset collection model data for |sectionIdentifier|
  [self.collectionViewController.collectionViewModel
                     setFooter:nil
      forSectionWithIdentifier:sectionIdentifier];
  [self.collectionViewController.collectionViewModel
                     setHeader:nil
      forSectionWithIdentifier:sectionIdentifier];
  [self.sectionIdentifiersFromContentSuggestions
      removeObject:@(sectionIdentifier)];

  // Update the section and the other ones.
  auto addSectionBlock = ^{
    [self.collectionViewController.collectionViewModel
        removeSectionWithIdentifier:sectionIdentifier];
    [self.collectionViewController.collectionView
        deleteSections:[NSIndexSet indexSetWithIndex:section]];

    NSIndexSet* addedSections =
        [self addSectionsForSectionInfoToModel:@[ sectionInfo ]];
    [self.collectionViewController.collectionView insertSections:addedSections];

    NSArray<NSIndexPath*>* addedItems = [self
        addSuggestionsToModel:[self.dataSource itemsForSectionInfo:sectionInfo]
              withSectionInfo:sectionInfo];
    [self.collectionViewController.collectionView
        insertItemsAtIndexPaths:addedItems];
  };
  [UIView animateWithDuration:0
                   animations:^{
                     [self.collectionViewController.collectionView
                         performBatchUpdates:addSectionBlock
                                  completion:nil];
                   }];

  // Make sure the section is still in the model and that the index is correct.
  if (![model hasSectionForSectionIdentifier:sectionIdentifier])
    return;
  section = [model sectionForSectionIdentifier:sectionIdentifier];

  [self.collectionViewController.collectionView
      reloadSections:[NSIndexSet indexSetWithIndex:section]];
}

- (void)itemHasChanged:(CollectionViewItem<SuggestedContent>*)item {
  if (![self.collectionViewController.collectionViewModel hasItem:item]) {
    return;
  }
  [self.collectionViewController reconfigureCellsForItems:@[ item ]];
}

#pragma mark - Public methods

- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section {
  NSNumber* identifier = @([self.collectionViewController.collectionViewModel
      sectionIdentifierForSection:section]);
  ContentSuggestionsSectionInformation* sectionInformation =
      self.sectionInfoBySectionIdentifier[identifier];
  return sectionInformation.layout == ContentSuggestionsSectionLayoutCustom;
}

- (ContentSuggestionType)contentSuggestionTypeForItem:
    (CollectionViewItem*)item {
  return ContentSuggestionTypeForItemType(item.type);
}

- (NSIndexPath*)removeEmptySuggestionsForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (![model hasSectionForSectionIdentifier:sectionIdentifier])
    return nil;

  NSArray<CSCollectionViewItem*>* existingItems =
      [model itemsInSectionWithIdentifier:sectionIdentifier];
  if (existingItems.count == 1 && existingItems[0].type == ItemTypeEmpty) {
    [model removeItemWithType:ItemTypeEmpty
        fromSectionWithIdentifier:sectionIdentifier];
    NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
    return [NSIndexPath indexPathForItem:0 inSection:section];
  }
  return nil;
}

- (NSArray<NSIndexPath*>*)
addSuggestionsToModel:(NSArray<CSCollectionViewItem*>*)suggestions
      withSectionInfo:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSMutableArray<NSIndexPath*>* indexPaths = [NSMutableArray array];

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  if (suggestions.count == 0) {
    // No suggestions for this section. Add the item signaling this section is
    // empty if there is currently no item in it.
    if ([model hasSectionForSectionIdentifier:sectionIdentifier] &&
        [model numberOfItemsInSection:[model sectionForSectionIdentifier:
                                                 sectionIdentifier]] == 0) {
      NSIndexPath* emptyItemIndexPath =
          [self addEmptyItemForSection:
                    [model sectionForSectionIdentifier:sectionIdentifier]];
      if (emptyItemIndexPath) {
        [indexPaths addObject:emptyItemIndexPath];
      }
    }
    return indexPaths;
  }

  // Add the items from this section.
  [suggestions enumerateObjectsUsingBlock:^(CSCollectionViewItem* item,
                                            NSUInteger index, BOOL* stop) {
    ItemType type = ItemTypeForInfo(sectionInfo);
    if (type == ItemTypePromo && !self.promoAdded) {
      self.promoAdded = YES;
      [self.collectionViewController.audience promoShown];
    }
    item.type = type;
    NSIndexPath* addedIndexPath =
        [self addItem:item toSectionWithIdentifier:sectionIdentifier];

    [indexPaths addObject:addedIndexPath];
  }];

  return indexPaths;
}

- (NSIndexSet*)addSectionsForSectionInfoToModel:
    (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo {
  NSMutableIndexSet* addedSectionIdentifiers = [NSMutableIndexSet indexSet];
  NSArray<ContentSuggestionsSectionInformation*>* orderedSectionsInfo =
      [self.dataSource sectionsInfo];

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  for (ContentSuggestionsSectionInformation* sectionInfo in sectionsInfo) {
    NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

    if ([model hasSectionForSectionIdentifier:sectionIdentifier] ||
        (!sectionInfo.showIfEmpty &&
         [self.dataSource itemsForSectionInfo:sectionInfo].count == 0)) {
      continue;
    }

    NSUInteger sectionIndex = 0;
    for (ContentSuggestionsSectionInformation* orderedSectionInfo in
             orderedSectionsInfo) {
      NSInteger orderedSectionIdentifier =
          SectionIdentifierForInfo(orderedSectionInfo);
      if (orderedSectionIdentifier == sectionIdentifier) {
        break;
      }
      if ([model hasSectionForSectionIdentifier:orderedSectionIdentifier]) {
        sectionIndex++;
      }
    }
    [model insertSectionWithIdentifier:sectionIdentifier atIndex:sectionIndex];

    self.sectionInfoBySectionIdentifier[@(sectionIdentifier)] = sectionInfo;
    [addedSectionIdentifiers addIndex:sectionIdentifier];

    if (sectionIdentifier == SectionIdentifierLogo) {
      [self addLogoHeaderIfNeeded];
    }
  }

  NSMutableIndexSet* indexSet = [NSMutableIndexSet indexSet];
  [addedSectionIdentifiers enumerateIndexesUsingBlock:^(
                               NSUInteger sectionIdentifier,
                               BOOL* _Nonnull stop) {
    [indexSet addIndex:[model sectionForSectionIdentifier:sectionIdentifier]];
  }];
  return indexSet;
}

- (NSIndexPath*)addEmptyItemForSection:(NSInteger)section {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger sectionIdentifier = [model sectionIdentifierForSection:section];
  ContentSuggestionsSectionInformation* sectionInfo =
      self.sectionInfoBySectionIdentifier[@(sectionIdentifier)];

  CSCollectionViewItem* item = [self emptyItemForSectionInfo:sectionInfo];
  if (!item) {
    return nil;
  }
  return [self addItem:item toSectionWithIdentifier:sectionIdentifier];
}

- (BOOL)isReturnToRecentTabSection:(NSInteger)section {
  return [self.collectionViewController.collectionViewModel
             sectionIdentifierForSection:section] ==
         SectionIdentifierReturnToRecentTab;
}

- (BOOL)isMostVisitedSection:(NSInteger)section {
  return
      [self.collectionViewController.collectionViewModel
          sectionIdentifierForSection:section] == SectionIdentifierMostVisited;
}

- (BOOL)isHeaderSection:(NSInteger)section {
  return [self.collectionViewController.collectionViewModel
             sectionIdentifierForSection:section] == SectionIdentifierLogo;
}

- (BOOL)isPromoSection:(NSInteger)section {
  return [self.collectionViewController.collectionViewModel
             sectionIdentifierForSection:section] == SectionIdentifierPromo;
}

#pragma mark - Private methods

// Adds the header for the first section, containing the logo and the omnibox,
// if there is no header for the section.
- (void)addLogoHeaderIfNeeded {
  if (![self.collectionViewController.collectionViewModel
          headerForSectionWithIdentifier:SectionIdentifierLogo]) {
    ContentSuggestionsHeaderItem* header =
        [[ContentSuggestionsHeaderItem alloc] initWithType:ItemTypeHeader];
    header.view = [self.dataSource headerViewForWidth:self.collectionWidth];
    [self.collectionViewController.collectionViewModel
                       setHeader:header
        forSectionWithIdentifier:SectionIdentifierLogo];
  }
}

// Resets the models, removing the current CollectionViewItem and the
// SectionInfo.
- (void)resetModels {
  [self.collectionViewController loadModel];
  self.sectionInfoBySectionIdentifier = [[NSMutableDictionary alloc] init];
  self.sectionIdentifiersFromContentSuggestions = [[NSMutableSet alloc] init];
}

// Returns a item to be displayed when the section identified by |sectionInfo|
// is empty.
// Returns nil if there is no empty item for this section info.
- (CSCollectionViewItem*)emptyItemForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  if (!sectionInfo.emptyText || !sectionInfo.expanded)
    return nil;
  ContentSuggestionsTextItem* item =
      [[ContentSuggestionsTextItem alloc] initWithType:ItemTypeEmpty];
  item.text = l10n_util::GetNSString(IDS_NTP_TITLE_NO_SUGGESTIONS);
  item.detailText = sectionInfo.emptyText;

  return item;
}

// Adds |item| to |sectionIdentifier| section of the model of the
// CollectionView. Returns the IndexPath of the newly added item.
- (NSIndexPath*)addItem:(CSCollectionViewItem*)item
    toSectionWithIdentifier:(NSInteger)sectionIdentifier {
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  NSInteger section = [model sectionForSectionIdentifier:sectionIdentifier];
  NSInteger itemNumber = [model numberOfItemsInSection:section];
  [model addItem:item toSectionWithIdentifier:sectionIdentifier];

  return [NSIndexPath indexPathForItem:itemNumber inSection:section];
}

@end
