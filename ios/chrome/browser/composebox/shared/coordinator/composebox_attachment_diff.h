// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_ATTACHMENT_DIFF_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_ATTACHMENT_DIFF_H_

#import <set>

#import "ios/web/public/web_state_id.h"

namespace composebox {

struct TabDiff {
  std::set<web::WebStateID> added;
  std::set<web::WebStateID> removed;
};

// Computes the difference between the current and target sets of Tab IDs.
TabDiff ComputeTabDiff(const std::set<web::WebStateID>& current,
                       const std::set<web::WebStateID>& target);

}  // namespace composebox

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_SHARED_COORDINATOR_COMPOSEBOX_ATTACHMENT_DIFF_H_
