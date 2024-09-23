// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_LAST_TAB_DRAGGED_ALERT_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_LAST_TAB_DRAGGED_ALERT_COMMAND_H_

#import <Foundation/Foundation.h>

class Browser;

namespace base {
class Uuid;
}
namespace tab_groups {
class TabGroupId;
class TabGroupVisualData;
}  // namespace tab_groups
namespace web {
class WebStateID;
}

// Data holder for the parameter used when displaying an alert when the last tab
// of a group has been dragged in the Tab Strip.
@interface TabStripLastTabDraggedAlertCommand : NSObject

// The ID of the dragged tab.
@property(nonatomic, assign) web::WebStateID tabID;
// The origin browser of the dragged tab.
@property(nonatomic, assign) Browser* originBrowser;
// The origin index of the dragged tab.
@property(nonatomic, assign) int originIndex;
// The visual data of the group of the dragged tab.
@property(nonatomic, assign) tab_groups::TabGroupVisualData visualData;
// The ID of the group.
@property(nonatomic, assign) tab_groups::TabGroupId localGroupID;
// The ID of the saved group associated with the local group.
@property(nonatomic, assign) base::Uuid savedGroupID;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_LAST_TAB_DRAGGED_ALERT_COMMAND_H_
