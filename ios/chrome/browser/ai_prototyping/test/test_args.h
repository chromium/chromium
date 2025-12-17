// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_TEST_ARGS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_TEST_ARGS_H_

#import <Foundation/Foundation.h>

static NSString* const kStorePageContextLocally =
    @"--save_page_context_locally";
static NSString* const kInputDir = @"--input_dir=";

// A helper class for reading test arguments.
@interface TestArgs : NSObject

// Whether to store PageContext to disk.
+ (BOOL)shouldStorePageContextLocallyFromTestArgs;

// Input directory for containing data required for PageContext extraction.
+ (NSString*)readInputDirFromTestArgs;

// TODO(crbug.com/465016086): Add more test args such as upload to MQLS.

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_TEST_ARGS_H_
