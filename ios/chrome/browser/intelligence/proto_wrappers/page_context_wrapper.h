// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/functional/callback.h"
#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace web {
class WebState;
}  // namespace web

// PageContextWrapper error states, for when no PageContext is provided to the
// caller.
enum class PageContextWrapperError {
  // Generic error.
  kGenericError,
  // APC was expected, but none was extracted.
  kAPCError,
  // Screenshot was expected, but none could be taken.
  kScreenshotError,
  // PDF data was expected, but none could be extracted.
  kPDFDataError,
  // The webpage is protected, PageContext was force-detached.
  kForceDetachError,
  // The Page Context retrieval timed out.
  kTimeout,
  // innerText was expected, but none was extracted.
  kInnerTextError,
};

using PageContextWrapperCallbackResponse =
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError>;

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

// Initializer with a PageContextWrapperConfig.
- (instancetype)initWithWebState:(web::WebState*)webState
                          config:(PageContextWrapperConfig)config
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback NS_DESIGNATED_INITIALIZER;

// Initializer with the default config.
- (instancetype)initWithWebState:(web::WebState*)webState
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback;

- (instancetype)init NS_UNAVAILABLE;

// Initiates the asynchronous work of populating all the PageContext fields, and
// executes the `completionCallback` when all async work is complete.
// Relinquishes ownership of the PageContext proto back to the handler of the
// callback. Uses a default timeout.
- (void)populatePageContextFieldsAsync;

// Same as `populatePageContextFieldsAsync`, but with a custom timeout.
- (void)populatePageContextFieldsAsyncWithTimeout:(base::TimeDelta)timeout;

// Enables force taking snapshots if none could be retrieved from storage, does
// nothing if `shouldGetSnapshot` is NO.
@property(nonatomic, assign) BOOL shouldForceUpdateMissingSnapshots;

// Since most of the extraction needs to run on the main thread anyways,
// enabling this flag will simply post the task on the main thread instead of
// executing it synchronously, so it can be picked up at a later (hopefully
// better) time. Use this for non time-sensitive or user-facing PageContext
// extractions.
@property(nonatomic, assign) BOOL isLowPriorityExtraction;

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

// Whether the entire webpage AnnotatedPageContent (APC) of innerTexts should be
// fetched. This will construct an APC tree with all same-origin and
// cross-origin frames as FrameData ContentNodes, each with their single
// corresponding TextInfo ContentNode filled with their innerText. For the main
// frame and its same-origin iframes, the original hierarchy is kept. All
// cross-origin iframes will be direct children of the main frame's root node,
// with their descendents keeping their relative (WRT to their parent
// cross-origin iframes) hierarchy.
@property(nonatomic, assign) BOOL shouldGetAnnotatedPageContent;

// Whether the entire webpage's innerText should be fetched. This includes the
// innerText of all of the webpage's iframes as the information is aggregated
// while the AnnotatedPageContent (APC) tree is built.
@property(nonatomic, assign) BOOL shouldGetInnerText;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_PROTO_WRAPPERS_PAGE_CONTEXT_WRAPPER_H_
