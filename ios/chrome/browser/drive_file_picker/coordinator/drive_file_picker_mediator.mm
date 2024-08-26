// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service.h"
#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/web/public/web_state.h"

namespace {
// A param to add to the default query to order the drive items as folders
// first, modification time as the second criteria.
NSString* orderByParam = @"folder,modifiedTime desc";

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
}

- (instancetype)initWithWebState:(web::WebState*)webState
                        identity:(id<SystemIdentity>)identity
                   driveFolderID:(DriveItemIdentifier*)driveFolderID
                    driveService:(drive::DriveService*)driveService {
  self = [super init];
  if (self) {
    CHECK(webState);
    CHECK(identity);
    _webState = webState->GetWeakPtr();
    _identity = identity;
    _driveFolderID = driveFolderID;
    _driveService = driveService;
    _fetchedDriveItems = {};
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
  }
}

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  if (_driveFolderID) {
    [_consumer setCurrentDriveFolderTitle:_driveFolderID.title];
  }
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

#pragma mark - Private

- (void)handleListItemsResponse:(const DriveListResult&)result {
  _fetchedDriveItems.insert(_fetchedDriveItems.end(), result.items.begin(),
                            result.items.end());
  NSMutableArray* res = [[NSMutableArray alloc] init];
  for (auto item : _fetchedDriveItems) {
    DriveItemIdentifier* driveItem = [[DriveItemIdentifier alloc]
        initWithIdentifier:item.identifier
                     title:item.name
                      icon:nil
              creationDate:[item.modified_time description]
                      type:(item.is_folder) ? DriveItemType::kFolder
                                            : DriveItemType::kFile];
    [res addObject:driveItem];
  }
  [self.consumer populateItems:res];
}

@end
