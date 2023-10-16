// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/extension_open_url.h"

bool ExtensionOpenURL(NSURL* url,
                      UIResponder* responder,
                      BlockWithBoolean pre_open_block) {
  while ((responder = responder.nextResponder)) {
    if ([responder respondsToSelector:@selector(openURL:)]) {
      if (pre_open_block) {
        pre_open_block(YES);
      }
      [responder performSelector:@selector(openURL:) withObject:url];
      return YES;
    }
  }
  if (pre_open_block) {
    pre_open_block(NO);
  }
  return NO;
}
