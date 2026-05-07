// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_

#import <UIKit/UIKit.h>

#import <set>

#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_mode.h"
#import "ios/chrome/browser/composebox/public/composebox_model_option.h"
#import "ios/chrome/browser/composebox/shared/coordinator/composebox_picker_image_result.h"
#import "ios/chrome/browser/composebox/shared/metrics/composebox_metrics_recorder.h"
#import "ios/web/public/web_state_id.h"

@class ComposeboxAttachmentSelection;

// Parameters used to focus and initialize the composebox with specific state.
@interface ComposeboxFocusParams : NSObject

// The initial query to pre-fill the omnibox.
@property(nonatomic, copy) NSString* query;

// The entrypoint that triggered the composebox.
@property(nonatomic, readonly) ComposeboxEntrypoint entrypoint;

// The initial tool mode.
@property(nonatomic, readonly) ComposeboxMode toolMode;

// The initial model mode.
@property(nonatomic, readonly) ComposeboxModelOption modelMode;

// The initial attachments to preload.
@property(nonatomic, readonly) ComposeboxAttachmentSelection* attachmentList;

/// Initial tab identifiers to attach.
@property(nonatomic, assign) std::set<web::WebStateID>
    initialSelectedWebStateIDs;
@property(nonatomic, assign) std::set<web::WebStateID> initialCachedWebStateIDs;

// Whether there are initial tab IDs set.
@property(nonatomic, readonly) BOOL hasInitialTabIDs;

// The optional shared metrics recorder.
@property(nonatomic, strong) ComposeboxMetricsRecorder* metricsRecorder;

/// The initial model option to force the composebox into.
@property(nonatomic, assign) ComposeboxModelOption initialModelOption;

/// Initializes a new instance with the given entrypoint.
- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
                             query:(NSString*)query
                          toolMode:(ComposeboxMode)toolMode
                         modelMode:(ComposeboxModelOption)modelMode
                    attachmentList:
                        (ComposeboxAttachmentSelection*)attachmentList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_
