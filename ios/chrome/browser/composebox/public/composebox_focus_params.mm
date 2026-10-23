// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"

#import <utility>

#import "components/contextual_search/contextual_search_session_handle.h"
#import "ios/chrome/browser/composebox/public/composebox_attachment_selection.h"

@implementation ComposeboxFocusParams {
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      _contextualSearchSession;
}

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint
                             query:(NSString*)query
                          toolMode:(ComposeboxMode)toolMode
                         modelMode:(omnibox::ModelMode)modelMode
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

- (BOOL)hasContextualSearchSession {
  return _contextualSearchSession != nullptr;
}

- (void)setContextualSearchSession:
    (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)session {
  _contextualSearchSession = std::move(session);
}

- (std::unique_ptr<contextual_search::ContextualSearchSessionHandle>)
    takeContextualSearchSession {
  return std::move(_contextualSearchSession);
}

- (BOOL)hasInitialTabIDs {
  return !self.initialSelectedWebStateIDs.empty() ||
         !self.initialCachedWebStateIDs.empty();
}

- (BOOL)hasInitialAttachments {
  return [self.attachmentList hasAttachments];
}

- (instancetype)initWithEntrypoint:(ComposeboxEntrypoint)entrypoint {
  return [self initWithEntrypoint:entrypoint
                            query:nil
                         toolMode:entrypoint == ComposeboxEntrypoint::kCobrowse
                                      ? ComposeboxMode::kAIM
                                      : ComposeboxMode::kRegularSearch
                        modelMode:omnibox::ModelMode::MODEL_MODE_UNSPECIFIED
                   attachmentList:nil];
}

@end
