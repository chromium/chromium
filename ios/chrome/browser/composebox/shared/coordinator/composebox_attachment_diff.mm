// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/shared/coordinator/composebox_attachment_diff.h"

#import <algorithm>
#import <iterator>

namespace composebox {

TabDiff ComputeTabDiff(const std::set<web::WebStateID>& current,
                       const std::set<web::WebStateID>& target) {
  TabDiff diff;
  std::set_difference(target.begin(), target.end(), current.begin(),
                      current.end(),
                      std::inserter(diff.added, diff.added.begin()));
  std::set_difference(current.begin(), current.end(), target.begin(),
                      target.end(),
                      std::inserter(diff.removed, diff.removed.begin()));
  return diff;
}

}  // namespace composebox
