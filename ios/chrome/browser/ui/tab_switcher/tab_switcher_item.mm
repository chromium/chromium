// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "base/check.h"

@implementation TabSwitcherItem

- (instancetype)initWithIdentifier:(NSString*)identifier {
  DCHECK(identifier);
  self = [super init];
  if (self) {
    _identifier = [identifier copy];
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
