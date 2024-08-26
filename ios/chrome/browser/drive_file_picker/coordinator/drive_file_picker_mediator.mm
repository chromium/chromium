// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator.h"

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_mediator_delegate.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_consumer.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_item_identifier.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/web/public/web_state.h"

@implementation DriveFilePickerMediator {
  base::WeakPtr<web::WebState> _webState;
  id<SystemIdentity> _identity;
  // The folder associated to the current `BrowseDriveFilePickerCoordinator`.
  DriveItemIdentifier* _driveFolderID;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                        identity:(id<SystemIdentity>)identity
                   driveFolderID:(DriveItemIdentifier*)driveFolderID {
  self = [super init];
  if (self) {
    CHECK(webState);
    CHECK(identity);
    _webState = webState->GetWeakPtr();
    _identity = identity;
    _driveFolderID = driveFolderID;
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
  }
}

- (void)setConsumer:(id<DriveFilePickerConsumer>)consumer {
  _consumer = consumer;
  [_consumer setSelectedUserIdentityEmail:_identity.userEmail];
  if (_driveFolderID) {
    [_consumer setCurrentDriveFolderTitle:_driveFolderID.title];
  }
}

- (void)selectDriveItem:(DriveItemIdentifier*)driveItem {
  switch (driveItem.type) {
    case DriveItemType::kFile:
    case DriveItemType::kFolder:
      [self.delegate browseDriveFolderWithMediator:self
                                     driveFolderID:driveItem];
  }
}

@end
