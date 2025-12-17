// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_EXTRACTION_DATA_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_EXTRACTION_DATA_H_

#import <Foundation/Foundation.h>

// Configuration for triggering a page context capture.
@interface PageContextExtractionConfig : NSObject <NSSecureCoding>

// When true, the captured page context will be stored in a local file on
// device.
@property(nonatomic, assign) BOOL shouldStorePageContextLocally;

// TODO(crbug.com/465016086): Add more configuration properties as needed.

- (instancetype)initWithShouldStorePageContextLocally:(BOOL)shouldStore;

@end

// Response containing the result of a page context capture.
@interface PageContextExtractionResult : NSObject <NSSecureCoding>

// The captured page context as a string.
@property(nonatomic, copy, readonly) NSString* pageContext;

// An error, if one occurred during capture.
@property(nonatomic, copy, readonly) NSError* error;

// The local file path where the context was stored, if requested.
@property(nonatomic, copy, readonly) NSString* filePath;

- (instancetype)initWithPageContext:(NSString*)pageContext
                              error:(NSError*)error
                           filePath:(NSString*)filePath;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_EXTRACTION_DATA_H_
