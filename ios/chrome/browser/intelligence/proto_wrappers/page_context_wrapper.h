// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

namespace web {
class WebState;
}  // namespace web

// A wrapper/helper around the `optimization_guide::proto::PageContext` proto
// which handles populating all the necessary PageContext fields asynchronously.
// Hopefully generic enough for use with any optimization guide feature.
@interface PageContextWrapper : NSObject

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

// Initializer which takes everything needed to construct the PageContext proto
// as arguments. Once all the async work is completed by calling
// `populatePageContextFieldsAsync`, the `completionCallback` will be executed
// (which will relinquish ownership of the proto to the callback's handler).
- (instancetype)
      initWithWebState:(web::WebState*)webState
    completionCallback:
        (base::OnceCallback<
            void(std::unique_ptr<optimization_guide::proto::PageContext>)>)
            completionCallback NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Initiates the synchronous and asynchronous work of populating all the
// PageContext fields, and executes the `completionCallback` when all async work
// is complete. Relinquishes ownership of the PageContext proto back to the
// handler of the callback.
- (void)populatePageContextFieldsAsync;

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_
