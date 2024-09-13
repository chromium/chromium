// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
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
// `sorting_criteria` and `sorting_direction`.
// TODO: use this method to handle text search queries.
DriveListQuery GetUpdatedQuery(const DriveListQuery& original_query,
                               DriveFilePickerFilter filter,
                               DriveItemsSortingType sorting_criteria,
                               DriveItemsSortingOrder sorting_direction,
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
    updated_order_by =
        [NSString stringWithFormat:@"folder,%@ %@", sorting_criteria_str,
                                   sorting_direction_str];
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
  updated_query.order_by = updated_order_by;
  updated_query.extra_term = update_extra_term;
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
  [_consumer setCurrentDriveFolderTitle:_title];
  [_consumer setFilter:_filter];
  [_consumer setAllFilesEnabled:_ignoreAcceptedTypes];
  [_consumer setSortingCriteria:_sortingCriteria direction:_sortingDirection];
}

- (void)updateSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  if (_identity == selectedIdentity) {
    return;
  }
  _identity = selectedIdentity;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self configureConsumerIdentitiesMenu];
}

#pragma mark - DriveFilePickerMutator

- (void)selectDriveItem:(NSString*)driveItemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, driveItemIdentifier);
  // If this is a real file, select and download it.
  if (driveItem && !driveItem->is_folder) {
    if ([_selectedIdentifier isEqual:driveItemIdentifier]) {
      // If the file is already selected, do nothing.
      return;
    }
    _selectedIdentifier = driveItemIdentifier;
    [self.consumer setSelectedItemIdentifier:driveItemIdentifier];
    [self downloadDriveItem:*driveItem];
    return;
  }

  // If the user tries to browse into a folder or collection while an item is
  // already selected, show an alert to ask for confirmation to clear the
  // selection.
  if (_selectedIdentifier != nil) {
    __weak __typeof(self) weakSelf = self;
    [self.consumer showInterruptionAlertWithBlock:^{
      [weakSelf clearSelection];
      [weakSelf selectDriveItem:driveItemIdentifier];
    }];
    return;
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
  [self fetchItemsAppending:YES];
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
  [self fetchItemsAppending:NO];
}

- (void)fetchIconForDriveItem:(NSString*)itemIdentifier {
  std::optional<DriveItem> driveItem =
      FindDriveItemFromIdentifier(_fetchedDriveItems, itemIdentifier);
  CHECK(driveItem);
  __weak __typeof(self) weakSelf = self;

  // By default drive api provides a 16 resolution icons, replacing 16 by 64 in
  // the icon URLs provide better sized icons e.g.
  // the URL https://drive-thirdparty.googleusercontent.com/16/type/video/mp4
  // becomes https://drive-thirdparty.googleusercontent.com/64/type/video/mp4
  NSString* resizedIconLink =
      [driveItem->icon_link stringByReplacingOccurrencesOfString:@"16"
                                                      withString:@"64"];
  GURL iconURL = GURL(base::SysNSStringToUTF16(resizedIconLink));
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
  [self fetchItemsAppending:NO];
}

- (void)browseToParent {
  if (_selectedIdentifier) {
    __weak __typeof(self) weakSelf = self;
    [self.consumer showInterruptionAlertWithBlock:^{
      [weakSelf clearSelection];
      [weakSelf browseToParent];
    }];
    return;
  }

  [self.delegate browseToParentWithMediator:self];
}

#pragma mark - Private

- (void)clearSelection {
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kNotStarted];
  [self.consumer setSelectedItemIdentifier:nil];
  _selectedIdentifier = nil;
}

- (void)downloadDriveItem:(const DriveItem&)driveItem {
  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kInProgress];
  _driveDownloader = _driveService->CreateFileDownloader(_identity);
  NSURL* fileURL = GenerateDownloadFileURL(driveItem.name);
  CHECK(fileURL);
  __weak __typeof(self) weakSelf = self;
  _driveDownloader->DownloadFile(
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

- (void)fetchItemsAppending:(BOOL)append {
  if (!append) {
    _pageToken = nil;
  }
  _driveList = _driveService->CreateList(_identity);

  DriveListQuery updatedQuery = GetUpdatedQuery(
      _originalQuery, _filter, _sortingCriteria, _sortingDirection, _pageToken);
  __weak __typeof(self) weakSelf = self;
  _driveList->ListItems(
      updatedQuery, base::BindOnce(^(const DriveListResult& result) {
        [weakSelf handleListItemsResponse:result appendItems:append];
      }));
}

- (void)handleListItemsResponse:(const DriveListResult&)result
                    appendItems:(BOOL)appendItems {
  _pageToken = result.next_page_token;
  if (appendItems) {
    _fetchedDriveItems.insert(_fetchedDriveItems.end(), result.items.begin(),
                              result.items.end());
  } else {
    _fetchedDriveItems = result.items;
  }
  NSMutableArray<DriveFilePickerItem*>* res = [[NSMutableArray alloc] init];
  for (const DriveItem& item : result.items) {
    DriveFilePickerItem* filePickerItem = DriveItemToDriveFilePickerItem(item);
    filePickerItem.enabled =
        ItemShouldBeEnabled(item, _acceptedTypes, _ignoreAcceptedTypes);
    [res addObject:filePickerItem];
  }
  [self.consumer populateItems:res
                        append:appendItems
             nextPageAvailable:(_pageToken != nil)];
}

- (void)identityUpdatedWithSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
  if (_identity == selectedIdentity) {
    return;
  }

  // If the user tries to select a different identity while the selection is not
  // empty, ask for confirmation first with an alert.
  if (_selectedIdentifier != nil) {
    __weak __typeof(self) weakSelf = self;
    [self.consumer showInterruptionAlertWithBlock:^{
      [weakSelf clearSelection];
      [weakSelf identityUpdatedWithSelectedIdentity:selectedIdentity];
    }];
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
