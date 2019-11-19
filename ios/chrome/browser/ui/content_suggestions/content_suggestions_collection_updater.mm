// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_updater.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_articles_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_header_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_text_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_source.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recording.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;
using CSCollectionViewModel = CollectionViewModel<CSCollectionViewItem*>;

// Enum defining the ItemType of this ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeArticle = kItemTypeEnumZero,
  ItemTypeFooter,
  ItemTypeHeader,
  ItemTypeEmpty,
  ItemTypeReadingList,
  ItemTypeMostVisited,
  ItemTypePromo,
  ItemTypeLearnMore,
  ItemTypeUnknown,
};

// Enum defining the SectionIdentifier of this
// ContentSuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierArticles = kSectionIdentifierEnumZero,
  SectionIdentifierReadingList,
  SectionIdentifierMostVisited,
  SectionIdentifierLogo,
  SectionIdentifierPromo,
  SectionIdentifierLearnMore,
  SectionIdentifierDefault,
};

// Returns the ContentSuggestionType associated with an ItemType |type|.
ContentSuggestionType ContentSuggestionTypeForItemType(NSInteger type) {
  if (type == ItemTypeArticle)
    return ContentSuggestionTypeArticle;
  if (type == ItemTypeEmpty)
    return ContentSuggestionTypeEmpty;
  if (type == ItemTypeReadingList)
    return ContentSuggestionTypeReadingList;
  if (type == ItemTypeMostVisited)
    return ContentSuggestionTypeMostVisited;
  if (type == ItemTypePromo)
    return ContentSuggestionTypePromo;
  if (type == ItemTypeLearnMore)
    return ContentSuggestionTypeLearnMore;
  // Add new type here

  // Default type.
  return ContentSuggestionTypeEmpty;
}

// Returns the item type corresponding to the section |info|.
ItemType ItemTypeForInfo(ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionArticles:
      return ItemTypeArticle;
    case ContentSuggestionsSectionReadingList:
      return ItemTypeReadingList;
    case ContentSuggestionsSectionMostVisited:
      return ItemTypeMostVisited;
    case ContentSuggestionsSectionPromo:
      return ItemTypePromo;
    case ContentSuggestionsSectionLearnMore:
      return ItemTypeLearnMore;

    case ContentSuggestionsSectionLogo:
    case ContentSuggestionsSectionUnknown:
      return ItemTypeUnknown;
  }
}

// Returns the section identifier corresponding to the section |info|.
SectionIdentifier SectionIdentifierForInfo(
    ContentSuggestionsSectionInformation* info) {
  switch (info.sectionID) {
    case ContentSuggestionsSectionArticles:
      return SectionIdentifierArticles;
    case ContentSuggestionsSectionReadingList:
      return SectionIdentifierReadingList;
    case ContentSuggestionsSectionMostVisited:
      return SectionIdentifierMostVisited;
    case ContentSuggestionsSectionLogo:
      return SectionIdentifierLogo;
    case ContentSuggestionsSectionPromo:
      return SectionIdentifierPromo;
    case ContentSuggestionsSectionLearnMore:
      return SectionIdentifierLearnMore;

    case ContentSuggestionsSectionUnknown:
      return SectionIdentifierDefault;
  }
}

// Returns whether this |sectionIdentifier| comes from ContentSuggestions.
BOOL IsFromContentSuggestionsService(NSInteger sectionIdentifier) {
  return sectionIdentifier == SectionIdentifierArticles ||
         sectionIdentifier == SectionIdentifierReadingList;
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

- (void)section:(ContentSuggestionsSectionInformation*)sectionInfo
      isLoading:(BOOL)isLoading {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  if (![model hasSectionForSectionIdentifier:sectionIdentifier] ||
      ![model footerForSectionWithIdentifier:sectionIdentifier])
    return;

  CollectionViewItem* footerItem =
      [model footerForSectionWithIdentifier:sectionIdentifier];
  ContentSuggestionsFooterItem* footer =
      base::mac::ObjCCastStrict<ContentSuggestionsFooterItem>(footerItem);

  if (footer.loading != isLoading) {
    footer.loading = isLoading;
    if (footer.configuredCell.delegate == footer) {
      // The cell associated with this footer is probably still on screen.
      [footer configureCell:footer.configuredCell];
    }
  }
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

  if (sectionIdentifier == SectionIdentifierLearnMore) {
    // The "Learn more" items should only be displayed if there is at least one
    // ContentSuggestions section.
    if ((![model hasSectionForSectionIdentifier:SectionIdentifierArticles] &&
         !
         [model hasSectionForSectionIdentifier:SectionIdentifierReadingList]) ||
        [model itemsInSectionWithIdentifier:sectionIdentifier].count > 0) {
      return @[];
    }
  } else if (IsFromContentSuggestionsService(sectionIdentifier)) {
    // If the section is a ContentSuggestions section, add the "Learn more"
    // items if they are not already present.
    if ([model hasSectionForSectionIdentifier:SectionIdentifierLearnMore] &&
        [model itemsInSectionWithIdentifier:SectionIdentifierLearnMore].count ==
            0) {
      ContentSuggestionsSectionInformation* learnMoreSectionInfo =
          self.sectionInfoBySectionIdentifier[@(SectionIdentifierLearnMore)];
      for (CSCollectionViewItem* item in
           [self.dataSource itemsForSectionInfo:learnMoreSectionInfo]) {
        item.type = ItemTypeForInfo(learnMoreSectionInfo);
        NSIndexPath* addedIndexPath = [self addItem:item
                            toSectionWithIdentifier:SectionIdentifierLearnMore];

        [indexPaths addObject:addedIndexPath];
      }
    }
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
    } else {
      [self addHeaderIfNeeded:sectionInfo];
    }
    if (sectionInfo.expanded)
      [self addFooterIfNeeded:sectionInfo];
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

- (BOOL)isContentSuggestionsSection:(NSInteger)section {
  return IsFromContentSuggestionsService(
      [self.collectionViewController.collectionViewModel
          sectionIdentifierForSection:section]);
}

- (void)dismissItem:(CSCollectionViewItem*)item {
  [self.dataSource dismissSuggestion:item.suggestionIdentifier];
}

#pragma mark - Private methods

// Adds a footer to the section identified by |sectionInfo| if there is none
// present and the section info contains a title for it.
- (void)addFooterIfNeeded:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  NSString* footerTitle = sectionInfo.footerTitle;

  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  if (footerTitle && ![self.collectionViewController.collectionViewModel
                         footerForSectionWithIdentifier:sectionIdentifier]) {
    ContentSuggestionsFooterItem* footer = [[ContentSuggestionsFooterItem alloc]
        initWithType:ItemTypeFooter
               title:sectionInfo.footerTitle
            callback:^(ContentSuggestionsFooterItem* item,
                       ContentSuggestionsFooterCell* cell) {
              __typeof(self) strongSelf = weakSelf;
              ContentSuggestionsSectionInformation* strongSectionInfo =
                  strongSelf
                      .sectionInfoBySectionIdentifier[@(sectionIdentifier)];
              DCHECK([footerTitle isEqual:strongSectionInfo.footerTitle]);
              [strongSelf runAdditionalActionForSection:strongSectionInfo
                                               withItem:item
                                                   cell:cell];
            }];

    [self.collectionViewController.collectionViewModel
                       setFooter:footer
        forSectionWithIdentifier:sectionIdentifier];
  }
}

// Adds the header corresponding to |sectionInfo| to the section if there is
// none present and the section info contains a title.
// In addition to that, if the section is for a content suggestion, only show a
// title if there are more than 1 occurence of a content suggestion section.
- (void)addHeaderIfNeeded:(ContentSuggestionsSectionInformation*)sectionInfo {
  NSInteger sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;

  if (![model headerForSectionWithIdentifier:sectionIdentifier] &&
      sectionInfo.title) {
    DCHECK(IsFromContentSuggestionsService(sectionIdentifier));
    if ([self.sectionIdentifiersFromContentSuggestions
            containsObject:@(sectionIdentifier)]) {
      return;
    }
    [self.sectionIdentifiersFromContentSuggestions
        addObject:@(sectionIdentifier)];
    [model setHeader:[self headerForSectionInfo:sectionInfo]
        forSectionWithIdentifier:sectionIdentifier];
  }
}

// Returns the header for this |sectionInfo|.
- (CollectionViewItem*)headerForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  DCHECK(SectionIdentifierForInfo(sectionInfo) == SectionIdentifierArticles);
  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  ContentSuggestionsArticlesHeaderItem* header =
      [[ContentSuggestionsArticlesHeaderItem alloc]
          initWithType:ItemTypeHeader
                 title:sectionInfo.title
              callback:^{
                [weakSelf.dataSource toggleArticlesVisibility];
              }];
  header.expanded = sectionInfo.expanded;
  return header;
}

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

// Runs the additional action for the section identified by |sectionInfo|.
- (void)runAdditionalActionForSection:
            (ContentSuggestionsSectionInformation*)sectionInfo
                             withItem:(ContentSuggestionsFooterItem*)item
                                 cell:(ContentSuggestionsFooterCell*)cell {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);

  CSCollectionViewModel* model =
      self.collectionViewController.collectionViewModel;
  if (![model hasSectionForSectionIdentifier:sectionIdentifier])
    return;

  // The more button is the footer of the section. So its position is the number
  // of items in the section.
  [self.collectionViewController.metricsRecorder
      onMoreButtonTappedAtPosition:
          [model numberOfItemsInSection:
                     [model sectionForSectionIdentifier:sectionIdentifier]]
                         inSection:sectionInfo];

  item.loading = YES;
  [item configureCell:cell];

  NSMutableArray<ContentSuggestionIdentifier*>* knownSuggestionIdentifiers =
      [NSMutableArray array];

  NSArray<CSCollectionViewItem*>* knownSuggestions =
      [model itemsInSectionWithIdentifier:sectionIdentifier];
  for (CSCollectionViewItem* suggestion in knownSuggestions) {
    if (suggestion.type != ItemTypeEmpty) {
      [knownSuggestionIdentifiers addObject:suggestion.suggestionIdentifier];
    }
  }

  __weak ContentSuggestionsCollectionUpdater* weakSelf = self;
  __weak ContentSuggestionsFooterItem* weakItem = item;
  __weak ContentSuggestionsFooterCell* weakCell = cell;
  [self.dataSource
      fetchMoreSuggestionsKnowing:knownSuggestionIdentifiers
                  fromSectionInfo:sectionInfo
                         callback:^(NSArray<CSCollectionViewItem*>* suggestions,
                                    content_suggestions::StatusCode status) {
                           [weakSelf moreSuggestionsFetched:suggestions
                                              inSectionInfo:sectionInfo
                                                   withItem:weakItem
                                                       cell:weakCell
                                                     status:status];
                         }];
}

// Adds the |suggestions| to the collection view. All the suggestions must have
// the same |sectionInfo|. The fetch has been started by tapping the |cell| and
// its associated |item|. |status| gives the status of the fetch result.
- (void)moreSuggestionsFetched:(NSArray<CSCollectionViewItem*>*)suggestions
                 inSectionInfo:
                     (ContentSuggestionsSectionInformation*)sectionInfo
                      withItem:(ContentSuggestionsFooterItem*)item
                          cell:(ContentSuggestionsFooterCell*)cell
                        status:(content_suggestions::StatusCode)status {
  SectionIdentifier sectionIdentifier = SectionIdentifierForInfo(sectionInfo);
  item.loading = NO;
  if (cell && item && cell.delegate == item) {
    // The cell has not been reconfigured by another item. It should be safe to
    // update it.
    [item configureCell:cell];
  }

  if (suggestions) {
    [self.collectionViewController addSuggestions:suggestions
                                    toSectionInfo:sectionInfo];

  } else if (status == content_suggestions::StatusCodeSuccess ||
             status == content_suggestions::StatusCodePermanentError) {
    // No more suggestions.
    UICollectionView* collectionView =
        self.collectionViewController.collectionView;

    [self.collectionViewController.collectionViewModel
                       setFooter:nil
        forSectionWithIdentifier:sectionIdentifier];

    [collectionView performBatchUpdates:^{
      [collectionView.collectionViewLayout invalidateLayout];
    }
                             completion:nil];

  } else if (status == content_suggestions::StatusCodeError) {
    NSString* text =
        l10n_util::GetNSString(IDS_NTP_ARTICLE_SUGGESTIONS_NOT_AVAILABLE);
    MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
    message.accessibilityLabel = text;
    message.category = kContentSuggestionsCollectionUpdaterSnackbarCategory;
    [self.dispatcher showSnackbarMessage:message];
    TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeError);
  }
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
