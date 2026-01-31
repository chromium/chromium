// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_COORDINATOR_MAGIC_STACK_RANKING_MODEL_TESTING_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_COORDINATOR_MAGIC_STACK_RANKING_MODEL_TESTING_H_

#import "ios/chrome/browser/content_suggestions/magic_stack/coordinator/magic_stack_ranking_model.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Exposes internal state of MagicStackRankingModel for testing.
@interface MagicStackRankingModel (ForTesting)

- (int)getNumPriceDropsForTesting:
    (std::vector<const bookmarks::BookmarkNode*>)subscriptions;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_COORDINATOR_MAGIC_STACK_RANKING_MODEL_TESTING_H_
