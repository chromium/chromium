// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_file_downloader.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_item.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// Delay after which items are fetched if the request is delayed.
constexpr base::TimeDelta kFetchItemsDelay = base::Seconds(0.5);
// Number of items to fetch for the first page.
constexpr NSInteger kFirstPageSize = 20;
// Number of items to fetch for the next pages.
constexpr NSInteger kNextPageSize = 50;

// A param to add to the default query to order the drive items as folders
// first, modification time as the second criteria.
NSString* orderByParam = @"folder,modifiedTime desc";
// folder_identifier parameter for the My Drive view.
NSString* kMyDriveFolderIdentifier = @"root";
// extra_term parameter for the Starred view.
NSString* kStarredExtraTerm = @"starred=true";
// extra_term parameter for the Recent view.
NSString* kRecentExtraTerm = @"mimeType!='application/vnd.google-apps.folder'";
// order_by parameter for the Recent view.
NSString* kRecentOrderBy = @"recency desc";
// extra_term parameter for the Shared with me view.
NSString* kSharedWithMeExtraTerm = @"sharedWithMe=true";
// order_by parameter for the Shared with me view.
NSString* kSharedWithMeOrderBy = @"sharedWithMeTime desc";
// The key word to sort items in an ascending order.
NSString* kAscendingQueryOrder = @"asc";
// The key word to sort items in an descending order.
NSString* kDescendingQueryOrder = @"desc";
// The key word to sort items by name.
NSString* kQueryOrderNameType = @"name";
// The key word to sort items by opening time.
NSString* kQueryOrderOpeningType = @"viewedByMeTime";
// The key word to sort items by modification time.
NSString* kQueryOrderModifiedType = @"modifiedTime";
// String representing any audio MIME type.
const char kAnyAudioFileMimeType[] = "audio/*";
// String representing any video MIME type.
const char kAnyVideoFileMimeType[] = "video/*";
// String representing any image MIME type.
const char kAnyImageFileMimeType[] = "image/*";
// extra_term parameter for the "Archives" filter.
NSString* kOnlyShowArchivesExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType='application/zip' or"
     " mimeType='application/x-7z-compressed' or"
     " mimeType='application/x-rar-compressed' or"
     " mimeType='application/vnd.rar' or"
     " mimeType='application/x-tar' or"
     " mimeType='application/x-bzip' or"
     " mimeType='application/x-bzip2' or"
     " mimeType='application/x-freearc' or"
     " mimeType='application/java-archive' or"
     " mimeType='application/gzip')";
// extra_term parameter for the "Audio" filter.
NSString* kOnlyShowAudioExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType contains 'audio/')";
// extra_term parameter for the "Video" filter.
NSString* kOnlyShowVideosExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType contains 'video/')";
// extra_term parameter for the "Photos & Images" filter.
NSString* kOnlyShowImagesExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType contains 'image/')";
// extra_term parameter for the "PDFs" filter.
NSString* kOnlyShowPDFsExtraTerm =
    @"(mimeType='application/vnd.google-apps.folder' or"
     " mimeType='application/pdf')";

// Returns the list of unified types accepted for `event`.
NSArray<UTType*>* UTTypesAcceptedForEvent(const ChooseFileEvent& event) {
  NSMutableArray<UTType*>* types = [NSMutableArray array];
  // Add accepted file extensions.
  for (const std::string& file_extension : event.accept_file_extensions) {
    std::string_view truncated_file_extension{std::next(file_extension.begin()),
                                              file_extension.end()};
    UTType* file_extension_type =
        [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(
                                              truncated_file_extension)];
    [types addObject:file_extension_type];
  }
  // Add accepted MIME types.
  for (const std::string& mime_type : event.accept_mime_types) {
    UTType* mime_type_type = nil;
    // Handle "audio/*", "video/*" and "image/*" separately since they are not
    // recognized by UTType.
    if (mime_type == kAnyAudioFileMimeType) {
      mime_type_type = UTTypeAudio;
    } else if (mime_type == kAnyVideoFileMimeType) {
      mime_type_type = UTTypeVideo;
    } else if (mime_type == kAnyImageFileMimeType) {
      mime_type_type = UTTypeImage;
    } else {
      mime_type_type =
          [UTType typeWithMIMEType:base::SysUTF8ToNSString(mime_type)];
    }
    // Test `mime_type_type` before adding since `typeWithMIMEType` can return
    // nil.
    if (mime_type_type) {
      [types addObject:mime_type_type];
    }
  }
  return types;
}

// Returns a copy of `original_query` updated to account for `filter`,
// `sorting_criteria`, `sorting_direction`, `search_text` and `page_token`.
DriveListQuery GetUpdatedQuery(const DriveListQuery& original_query,
                               DriveFilePickerFilter filter,
                               DriveItemsSortingType sorting_criteria,
                               DriveItemsSortingOrder sorting_direction,
                               NSString* search_text,
                               NSString* page_token) {
  // Update ordering.
  NSString* updated_order_by = original_query.order_by;
  if (!updated_order_by) {
    NSString* sorting_criteria_str;
    switch (sorting_criteria) {
      case DriveItemsSortingType::kName:
        sorting_criteria_str = kQueryOrderNameType;
        break;
      case DriveItemsSortingType::kOpeningTime:
        sorting_criteria_str = kQueryOrderOpeningType;
        break;
      case DriveItemsSortingType::kModificationTime:
        sorting_criteria_str = kQueryOrderModifiedType;
        break;
    }
    NSString* sorting_direction_str;
    switch (sorting_direction) {
      case DriveItemsSortingOrder::kAscending:
        sorting_direction_str = kAscendingQueryOrder;
        break;
      case DriveItemsSortingOrder::kDescending:
        sorting_direction_str = kDescendingQueryOrder;
        break;
    }
    if (search_text.length == 0) {
      // If this is not a search query, present folders first.
      updated_order_by =
          [NSString stringWithFormat:@"folder,%@ %@", sorting_criteria_str,
                                     sorting_direction_str];
    } else {
      // If this is a search query, respect the selected sorting criteria and
      // direction but folders do not necessarily appear first.
      updated_order_by =
          [NSString stringWithFormat:@"%@ %@", sorting_criteria_str,
                                     sorting_direction_str];
    }
  }

  // Update extra terms.
  NSMutableArray<NSString*>* extra_terms = [NSMutableArray array];
  if (original_query.extra_term) {
    [extra_terms addObject:original_query.extra_term];
  }
  NSString* filter_extra_term = nil;
  switch (filter) {
    case DriveFilePickerFilter::kOnlyShowArchives:
      filter_extra_term = kOnlyShowArchivesExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowAudio:
      filter_extra_term = kOnlyShowAudioExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowVideos:
      filter_extra_term = kOnlyShowVideosExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowImages:
      filter_extra_term = kOnlyShowImagesExtraTerm;
      break;
    case DriveFilePickerFilter::kOnlyShowPDFs:
      filter_extra_term = kOnlyShowPDFsExtraTerm;
      break;
    case DriveFilePickerFilter::kShowAllFiles:
      filter_extra_term = nil;
      break;
  }
  if (filter_extra_term) {
    [extra_terms addObject:filter_extra_term];
  }
  NSString* update_extra_term = [extra_terms componentsJoinedByString:@" and "];

  DriveListQuery updated_query = original_query;
  if (search_text.length != 0) {
    // Search queries are performed globally, not within a specific folder.
    updated_query.folder_identifier = nil;
  }
  updated_query.order_by = updated_order_by;
  updated_query.extra_term = update_extra_term;
  updated_query.filename_prefix = search_text;
  updated_query.page_token = page_token;
  return updated_query;
}

// Returns whether an item can be selected in the picker.
bool ItemShouldBeEnabled(const DriveItem& item,
                         NSArray<UTType*>* accepted_types,
                         BOOL ignore_accepted_types) {
  // Folders can be selected so their contents can be inspected.
  if (item.is_folder) {
    return true;
  }
  // Non-downloadable files cannot be selected.
  if (!item.can_download) {
    return false;
  }
  // If the list of accepted types is empty, or the user opted to ignore it,
  // then any downloadable file can be selected.
  if (ignore_accepted_types || accepted_types.count == 0) {
    return true;
  }
  // If there is a non-empty list of accepted types, then any downloadable file
  // conforming to one of these types can be selected.
  UTType* item_type = [UTType typeWithMIMEType:item.mime_type];
  for (UTType* accepted_type in accepted_types) {
    if ([item_type conformsToType:accepted_type]) {
      return true;
    }
  }
  return false;
}

// Returns a `DriveFilePickerItem` based on a `DriveItem`.
DriveFilePickerItem* DriveItemToDriveFilePickerItem(
    const DriveItem& driveItem) {
  DriveFilePickerItem* driveItemIdentifier = [[DriveFilePickerItem alloc]
      initWithIdentifier:driveItem.identifier
                   title:driveItem.name
                    icon:nil
            creationDate:[driveItem.modified_time description]
                    type:(driveItem.is_folder) ? DriveItemType::kFolder
                                               : DriveItemType::kFile];
  return driveItemIdentifier;
}

// Finds a DriveItem within the provided vector based on its identifier.
std::optional<DriveItem> FindDriveItemFromIdentifier(
    const std::vector<DriveItem>& driveItems,
    NSString* identifier) {
  auto it =
      std::find_if(driveItems.begin(), driveItems.end(),
                   [identifier](const DriveItem& driveItem) {
                     return [driveItem.identifier isEqualToString:identifier];
                   });
  if (it != driveItems.end()) {
    return *it;
  }
  return std::nullopt;
}

NSURL* GenerateDownloadFileURL(NSString* download_file_name) {
  base::FilePath download_dir;
  if (!GetTempDir(&download_dir)) {
    return nil;
  }
  download_dir =
      download_dir.Append(base::SysNSStringToUTF8([[NSUUID UUID] UUIDString]));
  base::FilePath download_file_path =
      download_dir.Append(base::SysNSStringToUTF8(download_file_name));
  return base::apple::FilePathToNSURL(download_file_path);
}

}  // namespace

@implementation DriveFilePickerMediator {
  base::WeakPtr<web::WebState> _webState;
  id<SystemIdentity> _identity;
  // The folder associated to the current `BrowseDriveFilePickerCoordinator`.
  raw_ptr<drive::DriveService> _driveService;
  std::unique_ptr<DriveList> _driveList;
  std::unique_ptr<DriveFileDownloader> _driveDownloader;
  NSString* _title;
  // Original query for this collection. Should not be changed.
  DriveListQuery _originalQuery;
  std::vector<DriveItem> _fetchedDriveItems;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // The service responsible for fetching a `DriveFilePickerItem`'s image data.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
  // File URL to which the selected file is being downloaded.
  NSURL* _selectedFileDestinationURL;
  // The selected drive item identifier.
  NSString* _selectedIdentifier;
  // Identifier of the download for the current selected item.
  NSString* _selectedIdentifierDownloadID;
  // If `_selectedIdentifier` is not nil, then this indicates whether it was
  // selected from search items or not.
  BOOL _selectedIdentifierIsSearchItem;
  // If this is true, all downloadable files can be selected regardless of type.
  BOOL _ignoreAcceptedTypes;
  // Filter used to only show items matching a certain type.
  DriveFilePickerFilter _filter;
  // Types accepted by the WebState.
  NSArray<UTType*>* _acceptedTypes;
  // Sorting criteria.
  DriveItemsSortingType _sortingCriteria;
  // Sorting direction.
  DriveItemsSortingOrder _sortingDirection;
  // Whether the search bar is currently focused.
  BOOL _searchBarFocused;
  // Search text.
  NSString* _searchText;
  // If this is `YES`, then items fetched subsequently will be search items.
  BOOL _shouldShowSearchItems;
  // Timer to delay fetching to avoid fetching too frequently if the query
  // parameters are modified frequently.
  base::OneShotTimer _fetchTimer;
  // The page token to use to continue the current list/search.
  NSString* _pageToken;
  // Whether this mediator is the root mediator of the file picker, in which
  // case file selection in the WebState should be stopped when disconnected.
  BOOL _isRoot;
}

- (instancetype)
         initWithWebState:(web::WebState*)webState
                   isRoot:(BOOL)isRoot
                 identity:(id<SystemIdentity>)identity
                    title:(NSString*)title
                    query:(DriveListQuery)query
                   filter:(DriveFilePickerFilter)filter
      ignoreAcceptedTypes:(BOOL)ignoreAcceptedTypes
          sortingCriteria:(DriveItemsSortingType)sortingCriteria
         sortingDirection:(DriveItemsSortingOrder)sortingDirection
             driveService:(drive::DriveService*)driveService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
             imageFetcher:(std::unique_ptr<image_fetcher::ImageDataFetcher>)
                              imageFetcher {
  self = [super init];
  if (self) {
    CHECK(webState);
    CHECK(identity);
    CHECK(accountManagerService);
    _webState = webState->GetWeakPtr();
    _isRoot = isRoot;
    _identity = identity;
    _driveService = driveService;
    _accountManagerService = accountManagerService;
    _title = [title copy];
    _originalQuery = query;
    _filter = filter;
    _ignoreAcceptedTypes = ignoreAcceptedTypes;
    _sortingCriteria = sortingCriteria;
    _sortingDirection = sortingDirection;
    _fetchedDriveItems = {};
    _imageFetcher = std::move(imageFetcher);
    // Initialize the list of accepted types.
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(webState);
    CHECK(tab_helper->IsChoosingFiles());
    _acceptedTypes = UTTypesAcceptedForEvent(tab_helper->GetChooseFileEvent());
  }
  return self;
}

- (void)disconnect {
  if (_isRoot && _webState && !_webState->IsBeingDestroyed()) {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(_webState.get());
    if (tab_helper->IsChoosingFiles()) {
      tab_helper->StopChoosingFiles();
    }
  }
  _webState = nullptr;
  _driveService = nullptr;
  _driveList = nullptr;
  _driveDownloader = nullptr;
  _accountManagerService = nullptr;
  _imageFetcher = nullptr;
}

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self configureConsumerIdentitiesMenu];
  [_consumer setTitle:_title];
  [_consumer setFilter:_filter];
  [_consumer setAllFilesEnabled:_ignoreAcceptedTypes];
  [_consumer setSortingCriteria:_sortingCriteria direction:_sortingDirection];
  [_consumer setLoadingIndicatorVisible:YES];
}

- (void)updateSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if (_identity == selectedIdentity) {
    return;
  }
  _identity = selectedIdentity;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self clearSelection];
  [self configureConsumerIdentitiesMenu];
}

#pragma mark - DriveFilePickerMutator

- (void)selectDriveItem:(NSString*)driveItemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, driveItemIdentifier);
  // If this is a real file, select and download it.
  if (driveItem && !driveItem->is_folder) {
    // Unfocusing the search bar so the confirmation button can become visible.
    _searchBarFocused = NO;
    [self.consumer setSearchBarFocused:NO searchText:_searchText];
    if ([_selectedIdentifier isEqual:driveItemIdentifier]) {
      // If the file is already selected, there is nothing else to do.
      return;
    }
    _selectedIdentifier = driveItemIdentifier;
    _selectedIdentifierIsSearchItem = _shouldShowSearchItems;
    [self.consumer setSelectedItemIdentifier:driveItemIdentifier];
    [self downloadDriveItem:*driveItem];
    return;
  }

  // If the user tries to browse into a folder or collection while an item is
  // already selected, clear the selection.
  if (_selectedIdentifier != nil) {
    [self clearSelection];
  }

  DriveListQuery itemQuery;
  if (driveItem && driveItem->is_folder) {
    // If this is a real folder, then browse this folder.
    itemQuery.folder_identifier = driveItem->identifier;
    [self.delegate browseDriveCollectionWithMediator:self
                                               title:driveItem->name
                                               query:itemQuery
                                              filter:_filter
                                 ignoreAcceptedTypes:_ignoreAcceptedTypes
                                     sortingCriteria:_sortingCriteria
                                    sortingDirection:_sortingDirection];
    return;
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem myDriveItem].identifier]) {
    itemQuery.folder_identifier = kMyDriveFolderIdentifier;
    DriveFilePickerItem* myDriveItem = [DriveFilePickerItem myDriveItem];
    [self.delegate browseDriveCollectionWithMediator:self
                                               title:myDriveItem.title
                                               query:itemQuery
                                              filter:_filter
                                 ignoreAcceptedTypes:_ignoreAcceptedTypes
                                     sortingCriteria:_sortingCriteria
                                    sortingDirection:_sortingDirection];
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem starredItem].identifier]) {
    itemQuery.extra_term = kStarredExtraTerm;
    DriveFilePickerItem* starredItem = [DriveFilePickerItem starredItem];
    [self.delegate browseDriveCollectionWithMediator:self
                                               title:starredItem.title
                                               query:itemQuery
                                              filter:_filter
                                 ignoreAcceptedTypes:_ignoreAcceptedTypes
                                     sortingCriteria:_sortingCriteria
                                    sortingDirection:_sortingDirection];
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem recentItem].identifier]) {
    itemQuery.extra_term = kRecentExtraTerm;
    itemQuery.order_by = kRecentOrderBy;
    DriveFilePickerItem* recentItem = [DriveFilePickerItem recentItem];
    [self.delegate browseDriveCollectionWithMediator:self
                                               title:recentItem.title
                                               query:itemQuery
                                              filter:_filter
                                 ignoreAcceptedTypes:_ignoreAcceptedTypes
                                     sortingCriteria:_sortingCriteria
                                    sortingDirection:_sortingDirection];
  } else if ([driveItemIdentifier
                 isEqual:[DriveFilePickerItem sharedWithMeItem].identifier]) {
    itemQuery.extra_term = kSharedWithMeExtraTerm;
    itemQuery.order_by = kSharedWithMeOrderBy;
    DriveFilePickerItem* sharedWithMeItem =
        [DriveFilePickerItem sharedWithMeItem];
    [self.delegate browseDriveCollectionWithMediator:self
                                               title:sharedWithMeItem.title
                                               query:itemQuery
                                              filter:_filter
                                 ignoreAcceptedTypes:_ignoreAcceptedTypes
                                     sortingCriteria:_sortingCriteria
                                    sortingDirection:_sortingDirection];
  } else {
    // TODO(crbug.com/344813593): Add support for Shared Drives.
  }
}

- (void)fetchNextPage {
  [self fetchItemsAppending:YES delayed:NO animated:YES];
}

- (void)setSortingCriteria:(DriveItemsSortingType)criteria
                 direction:(DriveItemsSortingOrder)direction {
  if (_sortingCriteria == criteria && _sortingDirection == direction) {
    // If no sorting parameter changed, do nothing.
    return;
  }
  _sortingCriteria = criteria;
  _sortingDirection = direction;
  [self.consumer setSortingCriteria:criteria direction:direction];
  [self fetchItemsAppending:NO delayed:NO animated:YES];
}

- (void)fetchIconForDriveItem:(NSString*)itemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  CHECK(driveItem);

  // By default drive api provides a 16 resolution icons, replacing 16 by 64 in
  // the icon URLs provide better sized icons e.g.
  // the URL https://drive-thirdparty.googleusercontent.com/16/type/video/mp4
  // becomes https://drive-thirdparty.googleusercontent.com/64/type/video/mp4
  NSString* resizedIconLink =
      [driveItem->icon_link stringByReplacingOccurrencesOfString:@"16"
                                                      withString:@"64"];
  GURL iconURL = GURL(base::SysNSStringToUTF16(resizedIconLink));
  __weak __typeof(self) weakSelf = self;
  _imageFetcher->FetchImageData(
      iconURL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf updateDriveItem:itemIdentifier withImageData:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

- (void)submitFileSelection {
  if (!_webState || _webState->IsBeingDestroyed()) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }
  ChooseFileTabHelper* tab_helper =
      ChooseFileTabHelper::GetOrCreateForWebState(_webState.get());
  if (!tab_helper->IsChoosingFiles()) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }
  CHECK(_selectedFileDestinationURL);
  tab_helper->StopChoosingFiles(@[ _selectedFileDestinationURL ], nil, nil);
  [self.delegate mediatorDidSubmitFileSelection:self];
}

- (void)setAcceptedTypesIgnored:(BOOL)ignoreAcceptedTypes {
  if (ignoreAcceptedTypes == _ignoreAcceptedTypes) {
    return;
  }
  _ignoreAcceptedTypes = ignoreAcceptedTypes;
  NSMutableSet<NSString*>* enabledItemsIdentifiers = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    if (ItemShouldBeEnabled(item, _acceptedTypes, _ignoreAcceptedTypes)) {
      [enabledItemsIdentifiers addObject:item.identifier];
    }
  }
  [self.consumer setEnabledItems:enabledItemsIdentifiers];
  [self.consumer setAllFilesEnabled:_ignoreAcceptedTypes];
}

- (void)setFilter:(DriveFilePickerFilter)filter {
  if (_filter == filter) {
    return;
  }
  _filter = filter;
  [self.consumer setFilter:filter];
  [self fetchItemsAppending:NO delayed:NO animated:YES];
}

- (void)setSearchBarFocused:(BOOL)focused {
  if (_searchBarFocused == focused) {
    return;
  }
  _searchBarFocused = focused;
  // If `_searchBarFocused` is set from the mutator interface, either because
  // the user focused the search bar by tapping on it or defocused it by tapping
  // "Cancel", then the search items should respectively be shown or hidden.
  [self setShouldShowSearchItems:focused];
}

- (void)setSearchText:(NSString*)searchText {
  if ([searchText isEqualToString:_searchText]) {
    return;
  }
  NSString* previousSearchText = _searchText;
  _searchText = searchText;
  if (_searchText.length == 0 || previousSearchText.length == 0) {
    // When switching from zero-state to non-zero-state search or the other way
    // around, items are trashed and the loading indicator is presented.
    _fetchedDriveItems = {};
    [self.consumer setLoadingIndicatorVisible:YES];
  }
  // Fetching new items is delayed when `_searchText` is modified, to ensure
  // modifying it very frequently does not equally too frequent API calls. This
  // works because only one pending fetch request is ever allowed at a time.
  [self fetchItemsAppending:NO delayed:YES animated:YES];
}

- (void)browseBack {
  if (_shouldShowSearchItems) {
    // If tapping "Back" from search items, simply hide search items.
    [self setShouldShowSearchItems:NO];
  } else {
    // If tapping "Back" outside of search, browse back to parent.
    [self.delegate browseToParentWithMediator:self];
  }
}

#pragma mark - Private

- (void)setShouldShowSearchItems:(BOOL)shouldShowSearchItems {
  if (shouldShowSearchItems == _shouldShowSearchItems) {
    return;
  }
  // When this line is reached, the mediator is switching between two modes:
  // showing search items and showing non-search items.
  _shouldShowSearchItems = shouldShowSearchItems;
  if (_selectedIdentifier != nil && _selectedIdentifierIsSearchItem &&
      !_shouldShowSearchItems) {
    // If the selected item was a search item and search items are hidden, clear
    // the selection.
    [self clearSelection];
  }
  if (!_shouldShowSearchItems) {
    // If search items are hidden, then ensure the search bar is defocused and
    // the search text is cleared.
    _searchBarFocused = NO;
    [self.consumer setSearchBarFocused:NO searchText:nil];
    _searchText = nil;
  }
  // When switching between search items and non-search items, the list of items
  // is cleared and the loading indicator is presented.
  _fetchedDriveItems = {};
  [self.consumer setLoadingIndicatorVisible:YES];
  // When showing search items, the title is hidden.
  [self.consumer setTitle:_shouldShowSearchItems ? nil : _title];
  [self fetchItemsAppending:NO delayed:YES animated:NO];
}

// Clears the selected identifier and updates the consumer accordingly.
- (void)clearSelection {
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kNotStarted];
  [self.consumer setSelectedItemIdentifier:nil];
  if (_driveDownloader && _selectedIdentifierDownloadID) {
    _driveDownloader->CancelDownload(_selectedIdentifierDownloadID);
  }
  _selectedIdentifierDownloadID = nil;
  _selectedIdentifier = nil;
}

- (void)downloadDriveItem:(const DriveItem&)driveItem {
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kInProgress];
  _driveDownloader = _driveService->CreateFileDownloader(_identity);
  NSURL* fileURL = GenerateDownloadFileURL(driveItem.name);
  CHECK(fileURL);
  __weak __typeof(self) weakSelf = self;
  _selectedIdentifierDownloadID = _driveDownloader->DownloadFile(
      driveItem, fileURL,
      base::BindRepeating(^(DriveFileDownloadID driveFileDownloadID,
                            const DriveFileDownloadProgress& progress){
      }),
      base::BindOnce(^(DriveFileDownloadID driveFileDownloadID, BOOL sucess,
                       NSError* error) {
        [weakSelf handleDownloadResponse:driveFileDownloadID
                                   error:error
                                 fileURL:fileURL];
      }));
}

- (void)handleDownloadResponse:(DriveFileDownloadID)driveFileDownloadID
                         error:(NSError*)error
                       fileURL:(NSURL*)fileURL {
  if (error) {
    // TODO(crbug.com/344815565): Display error message.
  }
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kSuccess];
  _selectedFileDestinationURL = fileURL;
}

// Fetches items according to the original query and the current state of the
// mediator i.e. selected filters, sorting parameters, search text, focus state
// of the search bar. If there is already a fetch query pending, a new one
// replaces it. If `append` is YES, then fetched items are inserted at the end
// of `_fetchedDriveItems`, otherwise they replace existing items. If `delayed`
// is YES then the query will be delayed and potentially canceled if a new query
// is triggered before the end of the delay. If `animated` is YES then the items
// in the response to that query will be animated into the consumer.
- (void)fetchItemsAppending:(BOOL)append
                    delayed:(BOOL)delayed
                   animated:(BOOL)animated {
  // If there is already a timer programmed to fetch items, cancel it.
  _fetchTimer.Stop();
  // If the fetching needs to be delayed, post it for later and return early.
  if (delayed) {
    __weak __typeof(self) weakSelf = self;
    _fetchTimer.Start(FROM_HERE, kFetchItemsDelay, base::BindOnce(^{
                        [weakSelf fetchItemsAppending:append
                                              delayed:NO
                                             animated:animated];
                      }));
    return;
  }

  if (!append) {
    // If this is a new query, then `_pageToken` can be reset.
    _pageToken = nil;
  }

  _driveList = _driveService->CreateList(_identity);

  DriveListQuery query;
  if (_shouldShowSearchItems && _searchText.length == 0) {
    // Zero-state search is treated separately and does not account for the
    // original query parameters, filtering and sorting parameters.
    query.order_by = kRecentOrderBy;
    query.extra_term = kRecentExtraTerm;
    query.page_token = _pageToken;
  } else {
    query = GetUpdatedQuery(_originalQuery, _filter, _sortingCriteria,
                            _sortingDirection, _searchText, _pageToken);
  }

  // Set page size according to whether this is the first page or not.
  query.page_size = append ? kNextPageSize : kFirstPageSize;

  __weak __typeof(self) weakSelf = self;
  _driveList->ListFiles(query, base::BindOnce(^(const DriveListResult& result) {
                          [weakSelf handleListItemsResponse:result
                                                   animated:animated];
                        }));
}

// Called as a completion of `_driveList->ListItems(...)`. Either replaces
// exisiting items with new ones (`append` is NO) or appends new items to
// existing ones (`append` is YES). If `animated` is YES then new items are
// animated into the consumer.
- (void)handleListItemsResponse:(const DriveListResult&)result
                       animated:(BOOL)animated {
  [self.consumer setLoadingIndicatorVisible:NO];

  // Remember old item identifiers so they can be reconfigured if they also show
  // up in `result.items`.
  NSMutableSet<NSString*>* previousIdentifiers = [NSMutableSet set];
  for (const DriveItem& item : _fetchedDriveItems) {
    [previousIdentifiers addObject:item.identifier];
  }

  BOOL append = _pageToken != nil;
  if (append) {
    // If `append`, then this is the next page to insert at the end.
    _fetchedDriveItems.insert(_fetchedDriveItems.end(), result.items.begin(),
                              result.items.end());
  } else {
    // Otherwise this is a first page so existing items are replaced.
    _fetchedDriveItems = result.items;
  }
  _pageToken = result.next_page_token;

  NSMutableArray<DriveFilePickerItem*>* res = [[NSMutableArray alloc] init];
  NSMutableArray<NSString*>* itemsToReconfigure = [NSMutableArray array];
  for (const DriveItem& item : result.items) {
    if ([previousIdentifiers containsObject:item.identifier]) {
      [itemsToReconfigure addObject:item.identifier];
    }
    DriveFilePickerItem* filePickerItem = DriveItemToDriveFilePickerItem(item);
    filePickerItem.enabled =
        ItemShouldBeEnabled(item, _acceptedTypes, _ignoreAcceptedTypes);
    // If the search text is not empty, emphasize the first match of the search
    // text inside the name of the item.
    if (_searchText.length != 0) {
      filePickerItem.titleRangeToEmphasize =
          [filePickerItem.title rangeOfString:_searchText
                                      options:NSCaseInsensitiveSearch];
    }
    [res addObject:filePickerItem];
  }
  // Showing the "Recent" search header in zero-state search.
  BOOL showSearchHeader = _shouldShowSearchItems && _searchText.length == 0;
  // If the next page token is nil, then the consumer does not need to try and
  // fetch new items when the end of the list is reached.
  BOOL nextPageAvailable = _pageToken != nil;
  [self.consumer populateItems:res
                        append:append
              showSearchHeader:showSearchHeader
             nextPageAvailable:nextPageAvailable
                      animated:animated];
  // If some items were already in the previous list, reconfigure these items.
  [self.consumer reconfigureItemsWithIdentifiers:itemsToReconfigure];
}

- (void)identityUpdatedWithSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
  if ([_identity isEqual:selectedIdentity]) {
    return;
  }
  [self.driveFilePickerHandler
      setDriveFilePickerSelectedIdentity:selectedIdentity];
}

- (void)configureConsumerIdentitiesMenu {
  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramSelectDriveIdentityEntry];

  __weak __typeof(self) weakSelf = self;
  auto actionResult = ^(id<SystemIdentity> identity) {
    [weakSelf identityUpdatedWithSelectedIdentity:identity];
  };
  // TODO(crbug.com/344812396): Add the identites block.
  UIMenuElement* identitiesMenu = [actionFactory
      menuToSelectDriveIdentityWithIdentities:_accountManagerService
                                                  ->GetAllIdentities()
                              currentIdentity:_identity
                                        block:actionResult];
  // TODO(crbug.com/344812396): Add the new account block.
  UIAction* addAccountAction =
      [actionFactory actionToAddAccountForDriveWithBlock:nil];
  [self.consumer setEmailsMenu:[UIMenu menuWithChildren:@[
                   addAccountAction, identitiesMenu
                 ]]];
}

- (void)updateDriveItem:(NSString*)itemIdentifier
          withImageData:(const std::string&)imageData {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  if (!driveItem) {
    return;
  }
  UIImage* driveIcon =
      [UIImage imageWithData:[NSData dataWithBytes:imageData.data()
                                            length:imageData.size()]
                       scale:[UIScreen mainScreen].scale];
  // An early return if no drive icon is available, avoiding an infinite look in
  // the consumer.
  if (!driveIcon) {
    return;
  }

  [self.consumer setIcon:driveIcon forItem:itemIdentifier];
}

@end
