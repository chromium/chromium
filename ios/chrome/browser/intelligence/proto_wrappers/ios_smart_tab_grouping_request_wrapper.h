// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_IOS_SMART_TAB_GROUPING_REQUEST_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_IOS_SMART_TAB_GROUPING_REQUEST_WRAPPER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/functional/callback_forward.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"

namespace optimization_guide {
namespace proto {
class IosSmartTabGroupingRequest;
}  // namespace proto
}  // namespace optimization_guide

class WebStateList;
class PersistTabContextBrowserAgent;

// A wrapper/helper around the
// `optimization_guide::proto::IosSmartTabGroupingRequest` proto which handles
// populating all the necessary IosSmartTabGroupingRequest fields
// asynchronously.
@interface IosSmartTabGroupingRequestWrapper : NSObject

// Initializer which takes everything needed to construct the
// IosSmartTabGrouping proto as arguments. The completion callback will be
// executed after the asynchronous operations started by
// `populateRequestFieldsAsyncFromPersistence` or
// `populateRequestFieldsAsyncFromWebStates` are fully complete. This callback
// relinquishes ownership of the proto to the callback's handler.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
       persistTabContextBrowserAgent:
           (PersistTabContextBrowserAgent*)persistTabContextBrowserAgent
                  completionCallback:
                      (base::OnceCallback<void(
                           std::unique_ptr<optimization_guide::proto::
                                               IosSmartTabGroupingRequest>)>)
                          completionCallback NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Initiates populating fields by loading from the
// PersistTabContextBrowserAgent. Executes the `completionCallback` when all
// async work is complete.
- (void)populateRequestFieldsAsyncFromPersistence;

// Initiates populating fields by extracting directly from the WebStates
// using PageContextWrapper. Executes the `completionCallback` when all async
// work is complete.
- (void)populateRequestFieldsAsyncFromWebStates;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_IOS_SMART_TAB_GROUPING_REQUEST_WRAPPER_H_
