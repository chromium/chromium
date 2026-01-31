// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_EXTRACTION_DATA_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_EXTRACTION_DATA_H_

#import <Foundation/Foundation.h>

// Configuration for triggering a page context capture.
@interface PageContextExtractionConfig : NSObject <NSSecureCoding>

// When true, the captured page context will be uploaded to MQLS.
@property(nonatomic, assign) BOOL shouldUploadToMQLS;

// When true, the captured page context will be stored in a local file on
// device.
@property(nonatomic, assign) BOOL shouldStorePageContextLocally;

// Output directory where page context should be saved to.
@property(nonatomic, copy, readonly) NSString* outputDir;

// Query to send to the model.
@property(nonatomic, copy, readonly) NSString* modelQuery;

// Tag to identify the MQLS log.
@property(nonatomic, copy, readonly) NSString* mqlsLoggingTag;

- (instancetype)initWithShouldStorePageContextLocally:(BOOL)shouldStore
                                   shouldUploadToMQLS:(BOOL)shouldUpload
                                            outputDir:(NSString*)outputDir
                                           modelQuery:(NSString*)modelQuery
                                       mqlsLoggingTag:(NSString*)mqlsLoggingTag;
@end

// Response containing the result of a page context capture.
@interface PageContextExtractionResult : NSObject <NSSecureCoding>

// The captured page context as a string.
@property(nonatomic, copy, readonly) NSString* pageContext;

// Errors, if any occurred during different stages of capture and upload.
@property(nonatomic, copy, readonly) NSError* wrapperError;
@property(nonatomic, copy, readonly) NSError* storeError;
@property(nonatomic, copy, readonly) NSError* mqlsError;

// The local file path where the context was stored, if requested.
@property(nonatomic, copy, readonly) NSString* filePath;

- (instancetype)initWithPageContext:(NSString*)pageContext
                       wrapperError:(NSError*)wrapperError
                         storeError:(NSError*)storeError
                          mqlsError:(NSError*)mqlsError
                           filePath:(NSString*)filePath;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_EXTRACTION_DATA_H_
