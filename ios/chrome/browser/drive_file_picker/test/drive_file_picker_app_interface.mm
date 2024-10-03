// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/test/drive_file_picker_app_interface.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/drive_file_picker_commands.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation DriveFilePickerAppInterface

+ (void)startChoosingFilesInCurrentWebState {
  Browser* currentBrowser = chrome_test_util::GetCurrentBrowser();
  WebStateList* webStateList = currentBrowser->GetWebStateList();
  ChooseFileTabHelper* tab_helper = ChooseFileTabHelper::GetOrCreateForWebState(
      webStateList->GetActiveWebState());
  auto controller = std::make_unique<FakeChooseFileController>(ChooseFileEvent(
      false, std::vector<std::string>{}, std::vector<std::string>{},
      webStateList->GetActiveWebState()));
  tab_helper->StartChoosingFiles(std::move(controller));
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
