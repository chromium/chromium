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
#import "ios/web/public/web_state_id.h"
/// Parameters used to focus and initialize the composebox with specific state.
@interface ComposeboxFocusParams : NSObject

/// The initial query to pre-fill the omnibox.
@property(nonatomic, copy) NSString* query;

/// The entrypoint that triggered the composebox.
@property(nonatomic, readonly) ComposeboxEntrypoint entrypoint;

/// The initial mode to force the composebox into.
@property(nonatomic, assign) ComposeboxMode initialMode;

/// Initial images to attach.
@property(nonatomic, copy) NSArray<ComposeboxPickerImageResult*>* initialImages;

/// Initial files to attach.
@property(nonatomic, copy) NSArray<NSURL*>* initialFiles;

/// Initial tab identifiers to attach.
@property(nonatomic, assign) std::set<web::WebStateID> initialTabIDs;

/// The initial model option to force the composebox into.
@property(nonatomic, assign) ComposeboxModelOption initialModelOption;

/// Initializes a new instance with the given entrypoint.
- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_FOCUS_PARAMS_H_
