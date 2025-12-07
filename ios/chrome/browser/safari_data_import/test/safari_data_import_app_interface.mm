// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/test/safari_data_import_app_interface.h"

#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

/// Returns the filename of `file`.
NSString* GetFilename(SafariDataImportTestFile file) {
  switch (file) {
    case SafariDataImportTestFile::kValid:
      return @"valid_test_archive.zip";
    case SafariDataImportTestFile::kPartiallyValid:
      return @"garbage_test_archive.zip";
    case SafariDataImportTestFile::kInvalid:
      return @"empty_test_archive.zip";
  }
}

/// Retrieve the file picker, if presented.
UIDocumentPickerViewController* GetFilePicker() {
  UIViewController* view_controller = [GetAnyKeyWindow() rootViewController];
  while (view_controller) {
    if ([view_controller
            isKindOfClass:[UIDocumentPickerViewController class]]) {
      return (UIDocumentPickerViewController*)view_controller;
    } else {
      view_controller = view_controller.presentedViewController;
    }
  }
  return nil;
}

}  // namespace

@implementation SafariDataImportAppInterface

+ (NSString*)selectFile:(SafariDataImportTestFile)file
             completion:(void (^)())completion {
  /// Get the URL from the app bundle.
  NSString* filename = GetFilename(file);
  NSString* resourceName = [filename stringByDeletingPathExtension];
  NSString* resourceType = [filename pathExtension];
  NSURL* fileLocation = [[NSBundle mainBundle] URLForResource:resourceName
                                                withExtension:resourceType];
  if (!fileLocation) {
    return @"File not found in app bundle.";
  }

  UIDocumentPickerViewController* filePicker = GetFilePicker();
  if (!filePicker) {
    return @"File picker is not presented.";
  }
  auto completionHandler = ^{
    [filePicker.delegate documentPicker:filePicker
                 didPickDocumentsAtURLs:@[ fileLocation ]];
    if (completion) {
      completion();
    }
  };
  [filePicker.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completionHandler];
  return nil;
}

@end
