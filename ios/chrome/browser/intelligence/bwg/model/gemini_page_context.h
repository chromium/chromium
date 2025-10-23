// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_PAGE_CONTEXT_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_PAGE_CONTEXT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <memory>

namespace ios::provider {
enum class BWGPageContextComputationState;
enum class BWGPageContextAttachmentState;
}  // namespace ios::provider

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

@interface GeminiPageContext : NSObject

// The PageContext for the current WebState. This is a unique_ptr, so subsequent
// calls to the getter will return a nullptr.
@property(nonatomic, assign)
    std::unique_ptr<optimization_guide::proto::PageContext>
        uniquePageContext;

// The state of the BWG PageContext computation.
@property(nonatomic, assign) ios::provider::BWGPageContextComputationState
    BWGPageContextComputationState;

// The state of the BWG PageContext attachment.
@property(nonatomic, assign)
    ios::provider::BWGPageContextAttachmentState BWGPageContextAttachmentState;

// The favicon of the attached page. Uses a default icon if it's unavailable.
@property(nonatomic, strong) UIImage* favicon;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_PAGE_CONTEXT_H_
