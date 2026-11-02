// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_TESTING_H_

#import <memory>
#import <set>

#import "base/functional/callback_forward.h"
#import "base/unguessable_token.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_mediator.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_item.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/web/public/web_state_id.h"

class GURL;
namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto
namespace web {
class WebState;
}  // namespace web

// Factory callback used to override `PageContextWrapper` creation in unit
// tests.
using PageContextWrapperFactoryCallback =
    base::RepeatingCallback<PageContextWrapper*(
        web::WebState* web_state,
        PageContextWrapperConfig config,
        base::OnceCallback<void(PageContextWrapperCallbackResponse)>
            completion_callback)>;

// Testing category exposing private methods of ComposeboxInputPlateMediator.
@interface ComposeboxInputPlateMediator (Testing)

- (void)setPageContextWrapperFactoryForTesting:
    (PageContextWrapperFactoryCallback)factory;
- (void)setState:(ComposeboxInputItemState)state
          onItem:(ComposeboxInputItem*)item;
- (base::UnguessableToken)createInputItemForWebState:(web::WebState*)webState
                                              source:(ComposeboxInputItemSource)
                                                         source;
- (BOOL)isWebStateIDRemoved:(web::WebStateID)webStateID;
- (void)removeDeselectedIDs:(std::set<web::WebStateID>)deselectedIDs;
- (void)updateAutoAttachedCurrentTab;
- (void)handleFailedAttachment:(base::UnguessableToken)identifier;
- (void)handlePageContextResponse:
            (std::unique_ptr<optimization_guide::proto::PageContext>)
                page_context
                         webState:(web::WebState*)webState
                       identifier:(base::UnguessableToken)identifier;
- (void)reloadSuggestions;
- (void)sendAIMFollowup:(NSString*)text;
- (void)didCreateSearchURL:(GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_INPUT_PLATE_MEDIATOR_TESTING_H_
