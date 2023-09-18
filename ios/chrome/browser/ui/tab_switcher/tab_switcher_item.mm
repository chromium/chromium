// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

#import "base/check.h"
#import "ios/web/public/web_state_id.h"

@implementation TabSwitcherItem

- (instancetype)initWithIdentifier:(web::WebStateID)identifier {
  self = [super init];
  if (self) {
    CHECK(identifier.valid());
    _identifier = identifier;
  }
  return self;
}

#pragma mark - Image Fetching

- (void)fetchFavicon:(TabSwitcherImageFetchingCompletionBlock)completion {
  // Subclasses should override this method. It is OK not to call super.
  completion(self, nil);
}

- (void)fetchSnapshot:(TabSwitcherImageFetchingCompletionBlock)completion {
  // Subclasses should override this method. It is OK not to call super.
  completion(self, nil);
}

- (void)prefetchSnapshot {
  // Subclasses should override this method. It is OK not to call super.
}

- (void)clearPrefetchedSnapshot {
  // Subclasses should override this method. It is OK not to call super.
}

@end
