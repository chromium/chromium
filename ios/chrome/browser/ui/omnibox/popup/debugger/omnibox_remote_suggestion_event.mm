// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_remote_suggestion_event.h"

@implementation OmniboxRemoteSuggestionEvent {
  base::UnguessableToken _uniqueIdentifier;
}

- (instancetype)initWithUniqueIdentifier:
    (const base::UnguessableToken&)requestIdentifier {
  self = [super init];
  if (self) {
    _uniqueIdentifier = requestIdentifier;
  }
  return self;
}

- (const base::UnguessableToken&)uniqueIdentifier {
  return _uniqueIdentifier;
}

#pragma mark - OmniboxEvent

- (EventType)type {
  return kRemoteSuggestionUpdate;
}

- (NSString*)title {
  NSString* status = @"Created";
  if (self.responseCode) {
    status = @"Completed";
  } else if (self.requestBody) {
    status = @"Started";
  }

  return [NSString stringWithFormat:@"Remote suggestions Update (%@)", status];
}

@end
