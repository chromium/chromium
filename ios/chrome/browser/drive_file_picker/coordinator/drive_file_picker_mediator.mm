// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
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
#import "url/gurl.h"

namespace {
// A param to add to the default query to order the drive items as folders
// first, modification time as the second criteria.
NSString* orderByParam = @"folder,modifiedTime desc";

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

}  // namespace

@implementation DriveFilePickerMediator {
  base::WeakPtr<web::WebState> _webState;
  id<SystemIdentity> _identity;
  // The folder associated to the current `BrowseDriveFilePickerCoordinator`.
  DriveItemIdentifier* _driveFolderID;
  raw_ptr<drive::DriveService> _driveService;
  std::unique_ptr<DriveList> _driveList;
  DriveListQuery _lastQuery;
  std::vector<DriveItem> _fetchedDriveItems;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // The service responsible for fetching a `DriveItemIdentifier`'s image data.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;
}

- (instancetype)
         initWithWebState:(web::WebState*)webState
                 identity:(id<SystemIdentity>)identity
            driveFolderID:(DriveItemIdentifier*)driveFolderID
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
    _driveFolderID = driveFolderID;
    _driveService = driveService;
    _accountManagerService = accountManagerService;
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
    _accountManagerService = nullptr;
    _imageFetcher = nullptr;
  }
}

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];

  [self configureConsumerIdentitiesMenu];

  if (_driveFolderID) {
    [_consumer setCurrentDriveFolderTitle:_driveFolderID.title];
  }
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
  switch (driveItem.type) {
    case DriveItemType::kFile:
    case DriveItemType::kFolder:
      [self.delegate browseDriveFolderWithMediator:self
                                     driveFolderID:driveItem];
  }
}

- (void)fetchDriveItemsForFolderID {
  _driveList = _driveService->CreateList(_identity);
  DriveListQuery query;
  query.folder_identifier = _driveFolderID.identifier;
  query.order_by = orderByParam;
  _lastQuery = query;

  __weak __typeof(self) weakSelf = self;
  _driveList->ListItems(query, base::BindOnce(^(const DriveListResult& result) {
                          [weakSelf handleListItemsResponse:result];
                        }));
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

#pragma mark - Private

- (void)handleListItemsResponse:(const DriveListResult&)result {
  _fetchedDriveItems.insert(_fetchedDriveItems.end(), result.items.begin(),
                            result.items.end());
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
  [_consumer setEmailsMenu:[UIMenu menuWithChildren:@[
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
