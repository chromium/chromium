// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_TAB_ORGANIZATION_REQUEST_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_TAB_ORGANIZATION_REQUEST_WRAPPER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"

class WebStateList;

// A wrapper/helper around the
// `optimization_guide::proto::TabOrganizationRequest` proto which handles
// populating all the necessary TabOrganizationRequest fields asynchronously.
@interface TabOrganizationRequestWrapper : NSObject

// Initializer which takes everything needed to construct the TabOrganization
// proto as arguments. Once all the async work is completed by calling
// `populateRequestFieldsAsync`, the `completionCallback` will be executed
// (which will relinquish ownership of the proto to the callback's handler).
- (instancetype)
               initWithWebStateList:(WebStateList*)webStateList
    allowReorganizingExistingGroups:(bool)allowReorganizingExistingGroups
                   groupingStrategy:
                       (optimization_guide::proto::
                            TabOrganizationRequest_TabOrganizationModelStrategy)
                           groupingStrategy
                 completionCallback:
                     (base::OnceCallback<
                         void(std::unique_ptr<optimization_guide::proto::
                                                  TabOrganizationRequest>)>)
                         completionCallback NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Initiates the synchronous and asynchronous work of populating all the
// TabOrganizationRequest fields, and executes the `completionCallback` when all
// async work is complete. Relinquishes ownership of the TabOrganizationRequest
// proto back to the handler of the callback.
- (void)populateRequestFieldsAsync;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_TAB_ORGANIZATION_REQUEST_WRAPPER_H_
