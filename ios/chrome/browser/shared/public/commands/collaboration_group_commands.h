// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COLLABORATION_GROUP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COLLABORATION_GROUP_COMMANDS_H_

namespace collaboration {
enum class CollaborationServiceShareOrManageEntryPoint;
}  // namespace collaboration
class TabGroup;

// Commands related to collaboration groups.
@protocol CollaborationGroupCommands

// Shows the "share" or "manage" screen for the `tabGroup`. The choice is
// automatically made based on whether the group is already shared or not.
- (void)
    shareOrManageTabGroup:(const TabGroup*)tabGroup
               entryPoint:
                   (collaboration::CollaborationServiceShareOrManageEntryPoint)
                       entryPoint;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COLLABORATION_GROUP_COMMANDS_H_
