// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/tab_strip_last_tab_dragged_alert_command.h"

#import "base/uuid.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/web/public/web_state_id.h"

@implementation TabStripLastTabDraggedAlertCommand {
  // Use an optional as TabGroupId doesn't have a default constructor.
  std::optional<tab_groups::TabGroupId> _localGroupID;
}

#pragma mark - Accessors

- (tab_groups::TabGroupId)localGroupID {
  CHECK(_localGroupID);
  return _localGroupID.value();
}

- (void)setLocalGroupID:(tab_groups::TabGroupId)localGroupID {
  _localGroupID = std::make_optional(localGroupID);
}

@end
