// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_suggestion_bottom_sheet_mediator.h"

@implementation PasskeySuggestionBottomSheetMediator {
  // Identifier of the passkey request which triggered the bottom sheet.
  std::string _requestID;
}

- (instancetype)initWithRequestID:(std::string)requestID {
  if ((self = [super init])) {
    _requestID = std::move(requestID);
  }

  return self;
}

@end
