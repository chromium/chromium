// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

/// Available files that could be used for Safari data import test cases.
enum class SafariDataImportTestFile { kValid, kPartiallyValid, kInvalid };

@interface SafariDataImportAppInterface : NSObject

/// If the file picker is currently being presented, select the file, dismiss
/// the picker and returns `nil`. Otherwise, returns an error message.
+ (NSString*)selectFile:(SafariDataImportTestFile)file
             completion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_TEST_SAFARI_DATA_IMPORT_APP_INTERFACE_H_
