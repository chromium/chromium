// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"

@implementation ComposeboxFocusParams

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
                             query:(NSString*)query
                          toolMode:(ComposeboxMode)toolMode
                         modelMode:(ComposeboxModelOption)modelMode
                    attachmentList:
                        (ComposeboxAttachmentSelection*)attachmentList {
  self = [super init];
  if (self) {
    _entrypoint = entrypoint;
    _query = [query copy];
    _toolMode = toolMode;
    _modelMode = modelMode;
    _attachmentList = attachmentList;
  }
  return self;
}

- (BOOL)hasInitialTabIDs {
  return !self.initialSelectedWebStateIDs.empty() ||
         !self.initialCachedWebStateIDs.empty();
}

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint {
  return [self initWithEntrypoint:entrypoint
                            query:nil
                         toolMode:ComposeboxMode::kRegularSearch
                        modelMode:ComposeboxModelOption::kNone
                   attachmentList:nil];
}

@end
