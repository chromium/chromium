// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_return_key_forwarding_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ForwardingReturnDelegate {
  // Weak, acts as a delegate
  OmniboxTextAcceptDelegate* _delegate;
}

- (void)setAcceptDelegate:(OmniboxTextAcceptDelegate*)delegate {
  _delegate = delegate;
}

#pragma mark - OmniboxReturnDelegate

- (void)omniboxReturnPressed:(id)sender {
  if (_delegate) {
    _delegate->OnAccept();
  }
}

@end
