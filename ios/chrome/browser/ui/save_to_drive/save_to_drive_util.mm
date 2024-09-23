// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_util.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace drive {

NSString* GetSizeString(int64_t size_in_bytes) {
  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.countStyle = NSByteCountFormatterCountStyleFile;
  formatter.zeroPadsFractionDigits = YES;
  NSString* result = [formatter stringFromByteCount:size_in_bytes];
  // Replace spaces with non-breaking spaces.
  result = [result stringByReplacingOccurrencesOfString:@" "
                                             withString:@"\u00A0"];
  return result;
}

NSString* GetAccountPickerBodyText(NSString* file_name, int64_t file_size) {
  const auto file_name_u16string = base::SysNSStringToUTF16(file_name);
  if (file_size > -1) {
    const auto file_size_u16string =
        base::SysNSStringToUTF16(GetSizeString(file_size));
    return l10n_util::GetNSStringF(
        IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_BODY_WITH_SIZE,
        file_name_u16string, file_size_u16string);
  } else {
    return l10n_util::GetNSStringF(IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_BODY,
                                   file_name_u16string);
  }
}

AccountPickerConfiguration* GetAccountPickerConfiguration(
    web::DownloadTask* download_task) {
  AccountPickerConfiguration* accountPickerConfiguration =
      [[AccountPickerConfiguration alloc] init];
  accountPickerConfiguration.titleText =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_TITLE);
  NSString* file_name =
      base::apple::FilePathToNSString(download_task->GenerateFileName());
  int64_t file_size = download_task->GetTotalBytes();
  accountPickerConfiguration.bodyText =
      drive::GetAccountPickerBodyText(file_name, file_size);
  accountPickerConfiguration.submitButtonTitle =
      l10n_util::GetNSString(IDS_IOS_SAVE_TO_DRIVE_ACCOUNT_PICKER_SUBMIT);
  accountPickerConfiguration.alwaysBounceVertical = YES;
  accountPickerConfiguration.defaultCornerRadius = YES;
  return accountPickerConfiguration;
}

}  // namespace drive
