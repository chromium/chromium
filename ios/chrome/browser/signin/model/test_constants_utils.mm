// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/test_constants_utils.h"

#import "ios/chrome/browser/signin/model/test_constants.h"

namespace signin {

NSArray<NSString*>* FakeSystemIdentityManagerStaySignedOutButtons() {
  if (!@available(iOS 26, *)) {
    // Once iOS 18 is dropped, all calls to
    // `FakeSystemIdentityManagerStaySignedOutButton` can be replaced by `@[
    // kFakeAuthCancelButtonIdentifier ]`.
    return
        @[ kFakeAuthDismissButtonIdentifier, kFakeAuthCancelButtonIdentifier ];
  }
  return @[ kFakeAuthCancelButtonIdentifier ];
}

}  // namespace signin
