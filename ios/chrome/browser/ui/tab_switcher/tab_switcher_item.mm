// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
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

#pragma mark - Image Fetching

- (void)fetchFavicon:(TabSwitcherImageFetchingCompletionBlock)completion {
  // Subclasses should override this method. It is OK not to call super.
  completion(self, nil);
  // This should not be called in production, as only real
  // WebStateTabSwitcherItem should be asked to fetch a favicon.
  // TODO(crbug.com/331159004): Remove in a later milestone if we don't receive
  // any.
  base::debug::DumpWithoutCrashing();
}

- (void)fetchSnapshot:(TabSwitcherImageFetchingCompletionBlock)completion {
  // Subclasses should override this method. It is OK not to call super.
  completion(self, nil);
  // This should not be called in production, as only real
  // WebStateTabSwitcherItem should be asked to fetch a snapshot.
  // TODO(crbug.com/331159004): Remove in a later milestone if we don't receive
  // any.
  base::debug::DumpWithoutCrashing();
}

- (void)prefetchSnapshot {
  // Subclasses should override this method. It is OK not to call super.
}

- (void)clearPrefetchedSnapshot {
  // Subclasses should override this method. It is OK not to call super.
}

@end
