// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/browser/drive/model/drive_file_downloader.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"
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

// Returns a `DriveItemIdentifier` based on a `DriveItem`.
DriveItemIdentifier* DriveItemToDriveItemIdentifier(
    const DriveItem& driveItem) {
  DriveItemIdentifier* driveItemIdentifier = [[DriveItemIdentifier alloc]
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
  DriveListQuery _query;
  std::vector<DriveItem> _fetchedDriveItems;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // The service responsible for fetching a `DriveItemIdentifier`'s image data.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
  // File URL to which the selected file is being downloaded.
  NSURL* _selectedFileDestinationURL;
}

- (instancetype)
         initWithWebState:(web::WebState*)webState
                 identity:(id<SystemIdentity>)identity
                    title:(NSString*)title
                    query:(DriveListQuery)query
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
    _identity = identity;
    _driveService = driveService;
    _accountManagerService = accountManagerService;
    _title = [title copy];
    _query = query;
    _fetchedDriveItems = {};
    _imageFetcher = std::move(imageFetcher);
  }
  return self;
}

- (void)disconnect {
  if (_webState) {
    ChooseFileTabHelper* tab_helper =
        ChooseFileTabHelper::GetOrCreateForWebState(_webState.get());
    if (tab_helper->IsChoosingFiles()) {
      tab_helper->StopChoosingFiles();
    }
    _webState = nullptr;
    _driveService = nullptr;
    _driveList = nullptr;
    _driveDownloader = nullptr;
    _accountManagerService = nullptr;
    _imageFetcher = nullptr;
  }
}

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  [self configureConsumerIdentitiesMenu];
  [_consumer setCurrentDriveFolderTitle:_title];
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

- (void)selectDriveItem:(DriveItemIdentifier*)driveItem {
  DriveListQuery itemQuery;
  switch (driveItem.type) {
    case DriveItemType::kFolder:
      itemQuery.folder_identifier = driveItem.identifier;
      itemQuery.order_by = orderByParam;
      [self.delegate browseDriveCollectionWithMediator:self
                                                 title:driveItem.title
                                                 query:itemQuery];
      break;
    case DriveItemType::kMyDrive:
      itemQuery.folder_identifier = kMyDriveFolderIdentifier;
      itemQuery.order_by = orderByParam;
      [self.delegate browseDriveCollectionWithMediator:self
                                                 title:driveItem.title
                                                 query:itemQuery];
      break;
    case DriveItemType::kStarred:
      itemQuery.extra_term = kStarredExtraTerm;
      itemQuery.order_by = orderByParam;
      [self.delegate browseDriveCollectionWithMediator:self
                                                 title:driveItem.title
                                                 query:itemQuery];
      break;
    case DriveItemType::kRecent:
      itemQuery.extra_term = kRecentExtraTerm;
      itemQuery.order_by = kRecentOrderBy;
      [self.delegate browseDriveCollectionWithMediator:self
                                                 title:driveItem.title
                                                 query:itemQuery];
      break;
    case DriveItemType::kSharedWithMe:
      itemQuery.extra_term = kSharedWithMeExtraTerm;
      itemQuery.order_by = kSharedWithMeOrderBy;
      [self.delegate browseDriveCollectionWithMediator:self
                                                 title:driveItem.title
                                                 query:itemQuery];
      break;
    // TODO(crbug.com/344813593): Add support for Shared Drives.
    case DriveItemType::kSharedDrives:
    case DriveItemType::kComputers:
    case DriveItemType::kFile:
      [self downloadDriveItem:driveItem];
      break;
  }
}

- (void)fetchNextPage {
  [self fetchItemsAppending:YES];
}

- (void)itemsUpdatedWithOrder:(DriveItemsSortingOrder)order
                         type:(DriveItemsSortingType)type {
  // TODO(crbug.com/344812396): Update and move the sorting logic to the
  // mediator.
  [self updateQueryWithOrder:order type:type];
  [self fetchItemsAppending:NO];
}

- (void)fetchIconForDriveItem:(DriveItemIdentifier*)driveItem {
  std::optional<DriveItem> itemForIdentifier =
      FindDriveItemFromIdentifier(_fetchedDriveItems, driveItem.identifier);
  CHECK(itemForIdentifier);
  __weak __typeof(self) weakSelf = self;

  // By default drive api provides a 16 resolution icons, replacing 16 by 64 in
  // the icon URLs provide better sized icons e.g.
  // the URL https://drive-thirdparty.googleusercontent.com/16/type/video/mp4
  // becomes https://drive-thirdparty.googleusercontent.com/64/type/video/mp4
  NSString* resizedIconLink =
      [itemForIdentifier->icon_link stringByReplacingOccurrencesOfString:@"16"
                                                              withString:@"64"];
  GURL iconURL = GURL(base::SysNSStringToUTF16(resizedIconLink));
  _imageFetcher->FetchImageData(
      iconURL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf updateDriveItem:driveItem withImageData:imageData];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

- (void)submitFileSelection {
  if (!_webState) {
    [self.driveFilePickerHandler hideDriveFilePicker];
    return;
  }

  ChooseFileTabHelper* tab_helper =
      ChooseFileTabHelper::GetOrCreateForWebState(_webState.get());
  if (tab_helper->IsChoosingFiles()) {
    CHECK(_selectedFileDestinationURL);
    tab_helper->StopChoosingFiles(@[ _selectedFileDestinationURL ], nil, nil);
  }
  [self.driveFilePickerHandler hideDriveFilePicker];
}

#pragma mark - Private

- (void)downloadDriveItem:(DriveItemIdentifier*)driveItemIdentifier {
  std::optional<DriveItem> driveItem = FindDriveItemFromIdentifier(
      _fetchedDriveItems, driveItemIdentifier.identifier);
  if (!driveItem.has_value()) {
    return;
  }

  [self.consumer setDownloadStatus:DriveFileDownloadStatus::kInProgress];
  _driveDownloader = _driveService->CreateFileDownloader(_identity);
  NSURL* fileURL = GenerateDownloadFileURL(driveItemIdentifier.title);
  CHECK(fileURL);
  __weak __typeof(self) weakSelf = self;
  _driveDownloader->DownloadFile(
      driveItem.value(), fileURL,
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
  _driveList = _driveService->CreateList(_identity);
  __weak __typeof(self) weakSelf = self;
  _driveList->ListItems(
      _query, base::BindOnce(^(const DriveListResult& result) {
        [weakSelf handleListItemsResponse:result appendItems:append];
      }));
}

- (void)updateQueryWithOrder:(DriveItemsSortingOrder)order
                        type:(DriveItemsSortingType)type {
  NSString* queryType;
  NSString* queryOrder;
  switch (order) {
    case DriveItemsSortingOrder::kAscending:
      queryOrder = kAscendingQueryOrder;
      break;
    case DriveItemsSortingOrder::kDescending:
      queryOrder = kDescendingQueryOrder;
      break;
  }

  switch (type) {
    case DriveItemsSortingType::kName:
      queryType = kQueryOrderNameType;
      break;
    case DriveItemsSortingType::kOpeningTime:
      queryType = kQueryOrderOpeningType;
      break;
    case DriveItemsSortingType::kModificationTime:
      queryType = kQueryOrderModifiedType;
      break;
  }

  _query.order_by =
      [NSString stringWithFormat:@"%@,%@ %@", @"folder", queryType, queryOrder];
}

- (void)handleListItemsResponse:(const DriveListResult&)result
                    appendItems:(BOOL)appendItems {
  if (appendItems) {
    _fetchedDriveItems.insert(_fetchedDriveItems.end(), result.items.begin(),
                              result.items.end());
  } else {
    _fetchedDriveItems = result.items;
  }

  NSMutableArray* res = [[NSMutableArray alloc] init];
  for (auto item : _fetchedDriveItems) {
    [res addObject:DriveItemToDriveItemIdentifier(item)];
  }
  [self.consumer populateItems:res];
}

- (void)identityUpdatedWithSelectedIdentity:
    (id<SystemIdentity>)selectedIdentity {
  if (_identity == selectedIdentity) {
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

- (void)updateDriveItem:(DriveItemIdentifier*)driveItem
          withImageData:(const std::string&)imageData {
  UIImage* driveIcon =
      [UIImage imageWithData:[NSData dataWithBytes:imageData.data()
                                            length:imageData.size()]
                       scale:[UIScreen mainScreen].scale];
  // An early return if no drive icon is available, avoiding an infinite look in
  // the consumer.
  if (!driveIcon) {
    return;
  }
  driveItem.icon = driveIcon;
  [self.consumer reconfigureDriveItem:driveItem];
}

@end
