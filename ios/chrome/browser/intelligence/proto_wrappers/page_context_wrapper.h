// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace web {
class WebState;
}  // namespace web

// A wrapper/helper around the `optimization_guide::proto::PageContext` proto
// which handles populating all the necessary PageContext fields asynchronously.
// By default, no async tasks will be executed, only the title and URL fields
// will be set (synchronous work). Please use the setters below to "enable" some
// or all of those async tasks before calling `populatePageContextFieldsAsync`.
// There are performance implications to enabling some of these, especially if
// the caller is populating PageContext protos for lots of tabs. When adding a
// new async task, ensure a related setter is also created to keep the
// disable-by-default behaviour.
@interface PageContextWrapper : NSObject


// Initializer which takes everything needed to construct the PageContext proto
// as arguments.
- (instancetype)
      initWithWebState:(web::WebState*)webState
    completionCallback:
        (base::OnceCallback<
            void(std::unique_ptr<::optimization_guide::proto::PageContext>)>)
            completionCallback NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Initiates the asynchronous work of populating all the PageContext fields, and
// executes the `completionCallback` when all async work is complete.
// Relinquishes ownership of the PageContext proto back to the handler of the
// callback.
- (void)populatePageContextFieldsAsync;

// Enables force taking snapshots if none could be retrieved from storage, does
// nothing if `shouldGetSnapshot` is NO.
@property(nonatomic, assign) BOOL shouldForceUpdateMissingSnapshots;

// Text to highlight in the snapshot. Will be highlighted just before taking the
// snapshot, and unhighlighted right after. Nil if no text should be
// highlighted. Only applies if the tab being processed is currently visible,
// and if `shouldGetSnapshot` is enabled. Beware this does visibly highlight
// said text in the webpage for the user for a split-second.
@property(nonatomic, copy) NSString* textToHighlight;

// Boolean flags for enabling/disabling the async tasks that the PageContext
// wrapper can execute.

// Whether a snapshot of the associated WebState should be fetched. If the
// WebState is currently visible, updates the snapshot taken instead of getting
// the previously saved snapshot.
@property(nonatomic, assign) BOOL shouldGetSnapshot;

// Whether a full page PDF of the associated WebState should be fetched. This
// force-realizes the associated WebState.
@property(nonatomic, assign) BOOL shouldGetFullPagePDF;

// Whether the entire webpage body's innerText should be fetched. This includes
// the innerText of the webpage's iframes, but only for those from the same
// origin as the main WebFrame.
@property(nonatomic, assign) BOOL shouldGetInnerText;


@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_
