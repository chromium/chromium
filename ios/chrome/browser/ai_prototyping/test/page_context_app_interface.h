// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_APP_INTERFACE_H_

#import "ios/chrome/browser/ai_prototyping/test/page_context_extraction_data.h"

// App interface for capturing annotated page context for testing.
@interface PageContextAppInterface : NSObject

// Triggers the capture of the annotated page context with the given `config`.
+ (void)triggerPageContextCaptureWithConfig:(PageContextExtractionConfig*)config
                                        url:(NSString*)url;

// Returns whether the page context capture has completed.
+ (BOOL)isPageContextCaptureComplete;

// Returns the page context capture result.
+ (PageContextExtractionResult*)pageContextResult;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TEST_PAGE_CONTEXT_APP_INTERFACE_H_
