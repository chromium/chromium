// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_view_label.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation IncognitoReauthViewLabel

- (void)layoutSubviews {
  [super layoutSubviews];
  [self.owner labelDidLayout];
}

@end
