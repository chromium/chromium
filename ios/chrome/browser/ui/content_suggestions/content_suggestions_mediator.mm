// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/optional.h"
#include "base/strings/sys_string_conversions.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_info.h"
#include "components/ntp_snippets/content_suggestion.h"
#include "components/ntp_tiles/metrics.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_learn_more_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_whats_new_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_service_bridge_observer.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/content_suggestions/mediator_util.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#include "ios/chrome/browser/ui/ntp/ntp_tile_saver.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CSCollectionViewItem = CollectionViewItem<SuggestedContent>;

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 4;

}  // namespace

@interface ContentSuggestionsMediator ()<ContentSuggestionsItemDelegate,
                                         ContentSuggestionsServiceObserver,
                                         MostVisitedSitesObserving,
                                         ReadingListModelBridgeObserver> {
  // Bridge for this class to become an observer of a ContentSuggestionsService.
  std::unique_ptr<ContentSuggestionsServiceBridge> _suggestionBridge;
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
  std::unique_ptr<NotificationPromoWhatsNew> _notificationPromo;
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;
}

// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* mostVisitedItems;
@property(nonatomic, strong)
    NSArray<ContentSuggestionsMostVisitedActionItem*>* actionButtonItems;
// Most visited items from the MostVisitedSites service (copied upon receiving
// the callback). Those items are up to date with the model.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedItem*>* freshMostVisitedItems;
// Section Info for the logo and omnibox section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* logoSectionInfo;
// Section Info for the What's New promo section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* promoSectionInfo;
// Section Info for the Most Visited section.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* mostVisitedSectionInfo;
// Section Info for the footer message allowing the user to know more about the
// suggested content.
@property(nonatomic, strong)
    ContentSuggestionsSectionInformation* learnMoreSectionInfo;
// Whether the page impression has been recorded.
@property(nonatomic, assign) BOOL recordedPageImpression;
// The ContentSuggestionsService, serving suggestions.
@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;
// Map the section information created to the relevant category.
@property(nonatomic, strong, nonnull)
    NSMutableDictionary<ContentSuggestionsCategoryWrapper*,
                        ContentSuggestionsSectionInformation*>*
        sectionInformationByCategory;
// Mediator fetching the favicons for the items.
@property(nonatomic, strong) ContentSuggestionsFaviconMediator* faviconMediator;
// Item for the Learn More section, containing the string.
@property(nonatomic, strong) ContentSuggestionsLearnMoreItem* learnMoreItem;
// Item for the reading list action item.  Reference is used to update the
// reading list count.
@property(nonatomic, strong)
    ContentSuggestionsMostVisitedActionItem* readingListItem;
// Number of unread items in reading list model.
@property(nonatomic, assign) NSInteger readingListUnreadCount;

@end

@implementation ContentSuggestionsMediator

@synthesize mostVisitedItems = _mostVisitedItems;
@synthesize actionButtonItems = _actionButtonItems;
@synthesize freshMostVisitedItems = _freshMostVisitedItems;
@synthesize logoSectionInfo = _logoSectionInfo;
@synthesize promoSectionInfo = _promoSectionInfo;
@synthesize mostVisitedSectionInfo = _mostVisitedSectionInfo;
@synthesize learnMoreSectionInfo = _learnMoreSectionInfo;
@synthesize recordedPageImpression = _recordedPageImpression;
@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;
@synthesize sectionInformationByCategory = _sectionInformationByCategory;
@synthesize commandHandler = _commandHandler;
@synthesize headerProvider = _headerProvider;
@synthesize faviconMediator = _faviconMediator;
@synthesize learnMoreItem = _learnMoreItem;
@synthesize readingListNeedsReload = _readingListNeedsReload;
@synthesize readingListItem = _readingListItem;
@synthesize readingListUnreadCount = _readingListUnreadCount;
@synthesize contentArticlesExpanded = _contentArticlesExpanded;
@synthesize contentArticlesEnabled = _contentArticlesEnabled;

#pragma mark - Public

- (instancetype)
initWithContentService:(ntp_snippets::ContentSuggestionsService*)contentService
      largeIconService:(favicon::LargeIconService*)largeIconService
        largeIconCache:(LargeIconCache*)largeIconCache
       mostVisitedSite:
           (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
      readingListModel:(ReadingListModel*)readingListModel {
  self = [super init];
  if (self) {
    _suggestionBridge =
        std::make_unique<ContentSuggestionsServiceBridge>(self, contentService);
    _contentService = contentService;
    _sectionInformationByCategory = [[NSMutableDictionary alloc] init];

    _faviconMediator = [[ContentSuggestionsFaviconMediator alloc]
        initWithContentService:contentService
              largeIconService:largeIconService
                largeIconCache:largeIconCache];

    _logoSectionInfo = LogoSectionInformation();
    _promoSectionInfo = PromoSectionInformation();
    _mostVisitedSectionInfo = MostVisitedSectionInformation();
    _learnMoreSectionInfo = LearnMoreSectionInformation();

    _learnMoreItem = [[ContentSuggestionsLearnMoreItem alloc] init];

    _notificationPromo = std::make_unique<NotificationPromoWhatsNew>(
        GetApplicationContext()->GetLocalState());
    _notificationPromo->Init();

    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->SetMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);
    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);
  }
  return self;
}

- (void)blacklistMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlacklistedUrl(URL, true);
  [self useFreshMostVisited];
}

- (void)whitelistMostVisitedURL:(GURL)URL {
  _mostVisitedSites->AddOrRemoveBlacklistedUrl(URL, false);
  [self useFreshMostVisited];
}

- (NotificationPromoWhatsNew*)notificationPromo {
  return _notificationPromo.get();
}

- (void)setDataSink:(id<ContentSuggestionsDataSink>)dataSink {
  _dataSink = dataSink;
  self.faviconMediator.dataSink = dataSink;
}

+ (NSUInteger)maxSitesShown {
  return kMaxNumMostVisitedTiles;
}

#pragma mark - ContentSuggestionsDataSource

- (NSArray<ContentSuggestionsSectionInformation*>*)sectionsInfo {
  NSMutableArray<ContentSuggestionsSectionInformation*>* sectionsInfo =
      [NSMutableArray array];

  [sectionsInfo addObject:self.logoSectionInfo];

  if (_notificationPromo->CanShow()) {
    [sectionsInfo addObject:self.promoSectionInfo];
  }

  [sectionsInfo addObject:self.mostVisitedSectionInfo];

  std::vector<ntp_snippets::Category> categories =
      self.contentService->GetCategories();

  for (auto& category : categories) {
    ContentSuggestionsCategoryWrapper* categoryWrapper =
        [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
    if (!self.sectionInformationByCategory[categoryWrapper]) {
      [self addSectionInformationForCategory:category];
    }
    if ([self isCategoryAvailable:category]) {
      [sectionsInfo
          addObject:self.sectionInformationByCategory[categoryWrapper]];
    }
  }

  [sectionsInfo addObject:self.learnMoreSectionInfo];

  return sectionsInfo;
}

- (NSArray<CSCollectionViewItem*>*)itemsForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  NSMutableArray<CSCollectionViewItem*>* convertedSuggestions =
      [NSMutableArray array];

  if (sectionInfo == self.logoSectionInfo) {
    // Section empty on purpose.
  } else if (sectionInfo == self.promoSectionInfo) {
    if (_notificationPromo->CanShow()) {
      ContentSuggestionsWhatsNewItem* item =
          [[ContentSuggestionsWhatsNewItem alloc] initWithType:0];
      item.icon = ios::GetChromeBrowserProvider()
                      ->GetBrandedImageProvider()
                      ->GetWhatsNewIconImage(_notificationPromo->icon());
      item.text = base::SysUTF8ToNSString(_notificationPromo->promo_text());
      [convertedSuggestions addObject:item];
    }
  } else if (sectionInfo == self.mostVisitedSectionInfo) {
    [convertedSuggestions addObjectsFromArray:self.mostVisitedItems];
    [convertedSuggestions addObjectsFromArray:self.actionButtonItems];
  } else if (sectionInfo == self.learnMoreSectionInfo) {
    [convertedSuggestions addObject:self.learnMoreItem];
  } else {
    ntp_snippets::Category category =
        [[self categoryWrapperForSectionInfo:sectionInfo] category];

    const std::vector<ntp_snippets::ContentSuggestion>& suggestions =
        self.contentService->GetSuggestionsForCategory(category);
    [self addSuggestions:suggestions
            fromCategory:category
             toItemArray:convertedSuggestions];
  }

  return convertedSuggestions;
}

- (void)fetchMoreSuggestionsKnowing:
            (NSArray<ContentSuggestionIdentifier*>*)knownSuggestions
                    fromSectionInfo:
                        (ContentSuggestionsSectionInformation*)sectionInfo
                           callback:(MoreSuggestionsFetched)callback {
  if (![self isRelatedToContentSuggestionsService:sectionInfo]) {
    callback(nil, content_suggestions::StatusCodeNotRun);
    return;
  }

  self.contentService->user_classifier()->OnEvent(
      ntp_snippets::UserClassifier::Metric::SUGGESTIONS_USED);

  ContentSuggestionsCategoryWrapper* wrapper =
      [self categoryWrapperForSectionInfo:sectionInfo];

  base::Optional<ntp_snippets::CategoryInfo> categoryInfo =
      self.contentService->GetCategoryInfo([wrapper category]);

  if (!categoryInfo) {
    callback(nil, content_suggestions::StatusCodeNotRun);
    return;
  }

  switch (categoryInfo->additional_action()) {
    case ntp_snippets::ContentSuggestionsAdditionalAction::NONE:
      callback(nil, content_suggestions::StatusCodeNotRun);
      return;

    case ntp_snippets::ContentSuggestionsAdditionalAction::VIEW_ALL:
      callback(nil, content_suggestions::StatusCodeNotRun);
      if ([wrapper category].IsKnownCategory(
              ntp_snippets::KnownCategories::READING_LIST)) {
        [self.commandHandler openReadingList];
      }
      break;

    case ntp_snippets::ContentSuggestionsAdditionalAction::FETCH: {
      std::set<std::string> known_suggestion_ids;
      for (ContentSuggestionIdentifier* identifier in knownSuggestions) {
        if (identifier.sectionInfo != sectionInfo)
          continue;
        known_suggestion_ids.insert(identifier.IDInSection);
      }

      if (known_suggestion_ids.size() == 0) {
        callback(nil, content_suggestions::StatusCodeNotRun);
        // No elements in the section, reloads everything to have suggestions
        // for the next NTP. Fetch() do not store the new data.
        self.contentService->ReloadSuggestions();
        return;
      }

      __weak ContentSuggestionsMediator* weakSelf = self;
      ntp_snippets::FetchDoneCallback serviceCallback = base::BindOnce(
          ^void(ntp_snippets::Status status,
                std::vector<ntp_snippets::ContentSuggestion> suggestions) {
            [weakSelf didFetchMoreSuggestions:suggestions
                               withStatusCode:status
                                     callback:callback];
          });

      self.contentService->Fetch([wrapper category], known_suggestion_ids,
                                 std::move(serviceCallback));

      break;
    }
  }
}

- (void)dismissSuggestion:(ContentSuggestionIdentifier*)suggestionIdentifier {
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self categoryWrapperForSectionInfo:suggestionIdentifier.sectionInfo];
  ntp_snippets::ContentSuggestion::ID suggestion_id =
      ntp_snippets::ContentSuggestion::ID([categoryWrapper category],
                                          suggestionIdentifier.IDInSection);

  self.contentService->DismissSuggestion(suggestion_id);
}

- (UIView*)headerViewForWidth:(CGFloat)width {
  return [self.headerProvider headerForWidth:width];
}

- (void)toggleArticlesVisibility {
  [self.contentArticlesExpanded setValue:![self.contentArticlesExpanded value]];

  // Update the section information for new collapsed state.
  ntp_snippets::Category category = ntp_snippets::Category::FromKnownCategory(
      ntp_snippets::KnownCategories::ARTICLES);
  ContentSuggestionsCategoryWrapper* wrapper =
      [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
  ContentSuggestionsSectionInformation* sectionInfo =
      self.sectionInformationByCategory[wrapper];
  sectionInfo.expanded = [self.contentArticlesExpanded value];

  // Reloading the section with animations looks bad because the section
  // border with the new collapsed height draws before the elements collapse.
  [UIView setAnimationsEnabled:NO];
  [self.dataSink reloadSection:sectionInfo];
  [UIView setAnimationsEnabled:YES];
}

#pragma mark - ContentSuggestionsServiceObserver

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
         newSuggestionsInCategory:(ntp_snippets::Category)category {
  ContentSuggestionsCategoryWrapper* wrapper =
      [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
  if (!self.sectionInformationByCategory[wrapper]) {
    [self addSectionInformationForCategory:category];
  }
  BOOL forceReload = NO;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST)) {
    forceReload = self.readingListNeedsReload;
    self.readingListNeedsReload = NO;
  }

  if ([self isCategoryAvailable:category]) {
    [self.dataSink
        dataAvailableForSection:self.sectionInformationByCategory[wrapper]
                    forceReload:forceReload];
  }
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
                         category:(ntp_snippets::Category)category
                  statusChangedTo:(ntp_snippets::CategoryStatus)status {
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc] initWithCategory:category];
  if (![self isCategoryInitOrAvailable:category]) {
    // Remove the category from the UI if it is not available.
    ContentSuggestionsSectionInformation* sectionInfo =
        self.sectionInformationByCategory[wrapper];

    [self.dataSink clearSection:sectionInfo];
    [self.sectionInformationByCategory removeObjectForKey:wrapper];
  } else {
    if (!self.sectionInformationByCategory[wrapper]) {
      [self addSectionInformationForCategory:category];
    }
    ContentSuggestionsSectionInformation* sectionInfo =
        self.sectionInformationByCategory[wrapper];
    [self.dataSink dataAvailableForSection:sectionInfo forceReload:NO];
    if (status == ntp_snippets::CategoryStatus::AVAILABLE_LOADING) {
      [self.dataSink section:sectionInfo isLoading:YES];
    } else {
      [self.dataSink section:sectionInfo isLoading:NO];
    }
  }
}

- (void)contentSuggestionsService:
            (ntp_snippets::ContentSuggestionsService*)suggestionsService
            suggestionInvalidated:
                (const ntp_snippets::ContentSuggestion::ID&)suggestion_id {
  ContentSuggestionsCategoryWrapper* wrapper =
      [[ContentSuggestionsCategoryWrapper alloc]
          initWithCategory:suggestion_id.category()];
  ContentSuggestionIdentifier* suggestionIdentifier =
      [[ContentSuggestionIdentifier alloc] init];
  suggestionIdentifier.IDInSection = suggestion_id.id_within_category();
  suggestionIdentifier.sectionInfo = self.sectionInformationByCategory[wrapper];

  [self.dataSink clearSuggestion:suggestionIdentifier];
}

- (void)contentSuggestionsServiceFullRefreshRequired:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  [self.dataSink reloadAllData];
}

- (void)contentSuggestionsServiceShutdown:
    (ntp_snippets::ContentSuggestionsService*)suggestionsService {
  // Update dataSink.
}

#pragma mark - SuggestedContentDelegate

- (void)loadImageForSuggestedItem:(ContentSuggestionsItem*)suggestedItem {
  __weak ContentSuggestionsMediator* weakSelf = self;
  __weak ContentSuggestionsItem* weakItem = suggestedItem;

  ntp_snippets::ContentSuggestion::ID suggestionID = SuggestionIDForSectionID(
      [self categoryWrapperForSectionInfo:suggestedItem.suggestionIdentifier
                                              .sectionInfo],
      suggestedItem.suggestionIdentifier.IDInSection);
  self.contentService->FetchSuggestionImage(
      suggestionID, base::BindOnce(^(const gfx::Image& image) {
        if (image.IsEmpty()) {
          return;
        }

        ContentSuggestionsMediator* strongSelf = weakSelf;
        ContentSuggestionsItem* strongItem = weakItem;
        if (!strongSelf || !strongItem) {
          return;
        }

        strongItem.image = [image.ToUIImage() copy];
        [strongSelf.dataSink itemHasChanged:strongItem];
      }));
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
  // This is used by the content widget.
  ntp_tile_saver::SaveMostVisitedToDisk(
      mostVisited, self.faviconMediator.mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  self.freshMostVisitedItems = [NSMutableArray array];
  for (const ntp_tiles::NTPTile& tile : mostVisited) {
    ContentSuggestionsMostVisitedItem* item =
        ConvertNTPTile(tile, self.mostVisitedSectionInfo);
    [self.faviconMediator fetchFaviconForMostVisited:item];
    [self.freshMostVisitedItems addObject:item];
  }

  if ([self.mostVisitedItems count] > 0) {
    // If some content is already displayed to the user, do not update without a
    // user action.
    return;
  }

  [self useFreshMostVisited];

  if (mostVisited.size() && !self.recordedPageImpression) {
    self.recordedPageImpression = YES;
    [self.faviconMediator setMostVisitedDataForLogging:mostVisited];
    ntp_tiles::metrics::RecordPageImpression(mostVisited.size());
  }
}

- (void)onIconMadeAvailable:(const GURL&)siteURL {
  // This is used by the content widget.
  ntp_tile_saver::UpdateSingleFavicon(
      siteURL, self.faviconMediator.mostVisitedAttributesProvider,
      app_group::ContentWidgetFaviconsFolder());

  for (ContentSuggestionsMostVisitedItem* item in self.mostVisitedItems) {
    if (item.URL == siteURL) {
      [self.faviconMediator fetchFaviconForMostVisited:item];
      return;
    }
  }
}

#pragma mark - ContentSuggestionsMetricsRecorderDelegate

- (ContentSuggestionsCategoryWrapper*)categoryWrapperForSectionInfo:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  return [[self.sectionInformationByCategory allKeysForObject:sectionInfo]
      firstObject];
}

#pragma mark - Private

// Converts the |suggestions| from |category| to CSCollectionViewItem and adds
// them to the |contentArray| if the category is available.
- (void)addSuggestions:
            (const std::vector<ntp_snippets::ContentSuggestion>&)suggestions
          fromCategory:(ntp_snippets::Category&)category
           toItemArray:(NSMutableArray<CSCollectionViewItem*>*)itemArray {
  if (![self isCategoryExpanded:category]) {
    return;
  }

  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [ContentSuggestionsCategoryWrapper wrapperWithCategory:category];
  if (!self.sectionInformationByCategory[categoryWrapper]) {
    [self addSectionInformationForCategory:category];
  }
  ContentSuggestionsSectionInformation* sectionInfo =
      self.sectionInformationByCategory[categoryWrapper];
  for (auto& contentSuggestion : suggestions) {
    ContentSuggestionsItem* suggestion =
        ConvertSuggestion(contentSuggestion, sectionInfo, category);
    suggestion.delegate = self;
    suggestion.commandHandler = self.commandHandler;
    [self.faviconMediator fetchFaviconForSuggestions:suggestion
                                          inCategory:category];

    [itemArray addObject:suggestion];
  }
}

// Adds the section information for |category| in
// self.sectionInformationByCategory.
- (void)addSectionInformationForCategory:(ntp_snippets::Category)category {
  base::Optional<ntp_snippets::CategoryInfo> categoryInfo =
      self.contentService->GetCategoryInfo(category);

  BOOL expanded = [self isCategoryExpanded:category];
  ContentSuggestionsSectionInformation* sectionInfo =
      SectionInformationFromCategoryInfo(categoryInfo, category, expanded);

  self.sectionInformationByCategory[[ContentSuggestionsCategoryWrapper
      wrapperWithCategory:category]] = sectionInfo;
}

// If the |statusCode| is a success and |suggestions| is not empty, runs the
// |callback| with the |suggestions| converted to Objective-C.
- (void)didFetchMoreSuggestions:
            (const std::vector<ntp_snippets::ContentSuggestion>&)suggestions
                 withStatusCode:(ntp_snippets::Status)statusCode
                       callback:(MoreSuggestionsFetched)callback {
  NSMutableArray<CSCollectionViewItem*>* contentSuggestions = nil;
  if (statusCode.IsSuccess() && !suggestions.empty() && callback) {
    contentSuggestions = [NSMutableArray array];
    ntp_snippets::Category category = suggestions[0].id().category();
    [self addSuggestions:suggestions
            fromCategory:category
             toItemArray:contentSuggestions];
  }
  callback(contentSuggestions, ConvertStatusCode(statusCode));
}

// Returns whether the |sectionInfo| is associated with a category from the
// content suggestions service.
- (BOOL)isRelatedToContentSuggestionsService:
    (ContentSuggestionsSectionInformation*)sectionInfo {
  return sectionInfo != self.mostVisitedSectionInfo &&
         sectionInfo != self.logoSectionInfo;
}

// Replaces the Most Visited items currently displayed by the most recent ones.
- (void)useFreshMostVisited {
  self.mostVisitedItems = self.freshMostVisitedItems;
  [self.dataSink reloadSection:self.mostVisitedSectionInfo];
}

// ntp_snippets doesn't differentiate between disabled vs collapsed, so if
// the status is |CATEGORY_EXPLICITLY_DISABLED|, check the value of
// |contentArticlesEnabled|.
- (BOOL)isCategoryInitOrAvailable:(ntp_snippets::Category)category {
  ntp_snippets::CategoryStatus status =
      self.contentService->GetCategoryStatus(category);
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES) &&
      status == ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED)
    return [self.contentArticlesEnabled value];
  else
    return IsCategoryStatusInitOrAvailable(
        self.contentService->GetCategoryStatus(category));
}

// ntp_snippets doesn't differentiate between disabled vs collapsed, so if
// the status is |CATEGORY_EXPLICITLY_DISABLED|, check the value of
// |contentArticlesEnabled|.
- (BOOL)isCategoryAvailable:(ntp_snippets::Category)category {
  ntp_snippets::CategoryStatus status =
      self.contentService->GetCategoryStatus(category);
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES) &&
      status == ntp_snippets::CategoryStatus::CATEGORY_EXPLICITLY_DISABLED) {
    return [self.contentArticlesEnabled value];
  } else {
    return IsCategoryStatusAvailable(
        self.contentService->GetCategoryStatus(category));
  }
}

// Returns whether the Articles category pref indicates it should be expanded,
// otherwise returns YES.
- (BOOL)isCategoryExpanded:(ntp_snippets::Category)category {
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES))
    return [self.contentArticlesExpanded value];
  else
    return YES;
}

#pragma mark - Properties

- (NSArray<ContentSuggestionsMostVisitedActionItem*>*)actionButtonItems {
  if (!_actionButtonItems) {
    self.readingListItem = ReadingListActionItem();
    self.readingListItem.count = self.readingListUnreadCount;
    _actionButtonItems = @[
      BookmarkActionItem(), self.readingListItem, RecentTabsActionItem(),
      HistoryActionItem()
    ];
  }
  return _actionButtonItems;
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self readingListModelDidApplyChanges:model];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  self.readingListUnreadCount = model->unread_size();
  if (self.readingListItem) {
    self.readingListItem.count = self.readingListUnreadCount;
    [self.dataSink itemHasChanged:self.readingListItem];
  }
}

@end
