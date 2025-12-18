// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_TEST_ARGS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_TEST_ARGS_H_

#import <Foundation/Foundation.h>

static NSString* const kStorePageContextLocally =
    @"--save_page_context_locally";
static NSString* const kInputFile = @"--input_urls_file=";
static NSString* const kOutputDirName = @"--output_dir=";

// A helper class for reading test arguments.
@interface TestArgs : NSObject

// Input file containing list of urls to extract PageContext from.
+ (NSString*)readUrlListFilePathTestArgs;

// Whether to store PageContext to disk.
+ (BOOL)shouldStorePageContextLocallyFromTestArgs;

// Output directory name to save PageContext in.
+ (NSString*)readOutputDirNameFromTestArgs;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_TEST_ARGS_H_
