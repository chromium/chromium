// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/test/drive_file_picker_app_interface.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/drive/model/drive_list.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/drive/model/test_drive_list.h"
#import "ios/chrome/browser/drive/model/test_drive_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {

// DriveListResult object which can be edited and will be passed to the current
// DriveService when `-[DriveFilePickerAppInterface endDriveListResult]` is
// called.
std::unique_ptr<DriveListResult> gDriveListResult;

}  // namespace

@implementation DriveFilePickerAppInterface

+ (void)startChoosingSingleFileInCurrentWebState {
  Browser* currentBrowser = chrome_test_util::GetCurrentBrowser();
  WebStateList* webStateList = currentBrowser->GetWebStateList();
  ChooseFileTabHelper* tab_helper = ChooseFileTabHelper::GetOrCreateForWebState(
      webStateList->GetActiveWebState());
  auto controller = std::make_unique<FakeChooseFileController>(ChooseFileEvent(
      false /*allow_multiple_files*/, false /*has_selected_file*/,
      std::vector<std::string>{}, std::vector<std::string>{},
      webStateList->GetActiveWebState()));
  tab_helper->StartChoosingFiles(std::move(controller));
}

+ (void)startChoosingMultipleFilesInCurrentWebState {
  Browser* currentBrowser = chrome_test_util::GetCurrentBrowser();
  WebStateList* webStateList = currentBrowser->GetWebStateList();
  ChooseFileTabHelper* tab_helper = ChooseFileTabHelper::GetOrCreateForWebState(
      webStateList->GetActiveWebState());
  auto controller = std::make_unique<FakeChooseFileController>(ChooseFileEvent(
      true /*allow_multiple_files*/, false /*has_selected_file*/,
      std::vector<std::string>{}, std::vector<std::string>{},
      webStateList->GetActiveWebState()));
  tab_helper->StartChoosingFiles(std::move(controller));
}

+ (void)beginDriveListResult {
  CHECK(!gDriveListResult);
  gDriveListResult = std::make_unique<DriveListResult>();
}

+ (void)addDriveItemWithIdentifier:(NSString*)identifier
                              name:(NSString*)name
                          isFolder:(BOOL)isFolder
                          mimeType:(NSString*)mimeType
                       canDownload:(BOOL)canDownload {
  CHECK(gDriveListResult);
  DriveItem newItem;
  newItem.identifier = [identifier copy];
  newItem.name = [name copy];
  newItem.is_folder = isFolder;
  newItem.mime_type = [mimeType copy];
  newItem.can_download = canDownload;
  gDriveListResult->items.push_back(newItem);
}

+ (void)endDriveListResult {
  CHECK(gDriveListResult);
  auto testDriveList = std::make_unique<TestDriveList>(nil);
  testDriveList->SetDriveListResult(*gDriveListResult);
  gDriveListResult.reset();
  ProfileIOS* currentProfile =
      chrome_test_util::GetCurrentBrowser()->GetProfile();
  drive::TestDriveService* driveService = static_cast<drive::TestDriveService*>(
      drive::DriveServiceFactory::GetForProfile(currentProfile));
  driveService->SetDriveList(std::move(testDriveList));
}

+ (void)showDriveFilePicker {
  id<DriveFilePickerCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler showDriveFilePicker];
}

+ (void)hideDriveFilePicker {
  id<DriveFilePickerCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler hideDriveFilePicker];
}
@end
