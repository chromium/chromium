// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_

#import <UIKit/UIKit.h>

#import <memory>
#import <set>

#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/web/public/web_state_id.h"
#import "third_party/omnibox_proto/model_mode.pb.h"

namespace contextual_search {
class ContextualSearchSessionHandle;
}  // namespace contextual_search

@class AssistantAIMMetricsRecorder;
@class ComposeboxAttachmentSelection;
@class ComposeboxInputItem;

// Parameters used to focus and initialize the composebox with specific state.
@interface ComposeboxFocusParams : NSObject

// The initial query to pre-fill the omnibox.
@property(nonatomic, copy) NSString* query;

// The entrypoint that triggered the composebox.
@property(nonatomic, readonly) ComposeboxEntrypoint entrypoint;

// The initial tool mode.
@property(nonatomic, readonly) ComposeboxMode toolMode;

// The initial model mode, or `MODEL_MODE_UNSPECIFIED` to use the default model.
@property(nonatomic, readonly) omnibox::ModelMode modelMode;

// The initial attachments to preload.
@property(nonatomic, readonly) ComposeboxAttachmentSelection* attachmentList;

// Pre-attached and already-uploaded tab items transferred from a previous
// composebox session (e.g., when transitioning into Cobrowse).
@property(nonatomic, copy) NSArray<ComposeboxInputItem*>* attachedTabItems;

// Returns whether this focus params instance holds a transferable contextual
// search session handle.
@property(nonatomic, assign, readonly) BOOL hasContextualSearchSession;

// Sets the contextual search session handle transferred from a previous
// composebox session.
- (void)setContextualSearchSession:
    (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)session;

// Transfers ownership of the stored contextual search session handle to the
// caller, leaving the stored handle empty.
- (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)
    takeContextualSearchSession;

/// Initial tab identifiers to attach.
@property(nonatomic, assign) std::set<web::WebStateID>
    initialSelectedWebStateIDs;
@property(nonatomic, assign) std::set<web::WebStateID> initialCachedWebStateIDs;

// Whether there are initial tab IDs set.
@property(nonatomic, readonly) BOOL hasInitialTabIDs;

// Whether there are any attachments (images, files, or tabs) set.
@property(nonatomic, readonly) BOOL hasInitialAttachments;

// The optional shared metrics recorder.
@property(nonatomic, strong) ComposeboxMetricsRecorder* metricsRecorder;

// The optional metrics recorder for the Assistant AIM bottom sheet session.
@property(nonatomic, strong)
    AssistantAIMMetricsRecorder* assistantMetricsRecorder;

// Initializes a new instance with the given `entrypoint`, `query`, `toolMode`,
// `modelMode` (or `MODEL_MODE_UNSPECIFIED` to use the default model), and
// `attachmentList`.
- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
                             query:(NSString*)query
                          toolMode:(ComposeboxMode)toolMode
                         modelMode:(omnibox::ModelMode)modelMode
                    attachmentList:
                        (ComposeboxAttachmentSelection*)attachmentList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_
