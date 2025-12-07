// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"

#import "base/check.h"
#import "ios/web/public/web_state_id.h"
#import "url/gurl.h"

@implementation TabSwitcherItem

- (instancetype)initWithIdentifier:(web::WebStateID)identifier {
  self = [super init];
  if (self) {
    CHECK(identifier.valid());
    _identifier = identifier;
  }
  return self;
}

#pragma mark - Debugging

- (NSString*)description {
  return
      [NSString stringWithFormat:@"Tab ID: %d", self.identifier.identifier()];
}

@end
