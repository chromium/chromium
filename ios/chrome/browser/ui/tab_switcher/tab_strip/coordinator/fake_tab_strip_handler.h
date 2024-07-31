// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_FAKE_TAB_STRIP_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_FAKE_TAB_STRIP_HANDLER_H_

#import <set>

#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"

class TabGroup;
namespace web {
class WebStateID;
}

// Fake handler to get commands in tests.
@interface FakeTabStripHandler : NSObject <TabStripCommands>

@property(nonatomic, assign) std::set<web::WebStateID>
    identifiersForTabGroupCreation;

@property(nonatomic, assign) const TabGroup* groupForTabGroupEdition;

// Command sent when asking to display an alert when dragging the last tab.
@property(nonatomic, strong)
    TabStripLastTabDraggedAlertCommand* lastTabDraggedCommand;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_COORDINATOR_FAKE_TAB_STRIP_HANDLER_H_
