// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view_label.h"

@implementation IncognitoReauthViewLabel

- (void)layoutSubviews {
  [super layoutSubviews];
  [self.owner labelDidLayout];
}

@end
