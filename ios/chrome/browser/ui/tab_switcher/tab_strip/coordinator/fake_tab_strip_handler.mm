// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/fake_tab_strip_handler.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

@implementation FakeTabStripHandler

- (void)showTabStripGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
  _identifiersForTabGroupCreation = identifiers;
}

- (void)showTabStripGroupEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup {
  _groupForTabGroupEdition = tabGroup.get();
}

- (void)hideTabStripGroupCreation {
}

- (void)shareItem:(TabSwitcherItem*)tabSwitcherItem
       originView:(UIView*)originView {
}

- (void)showAlertForLastTabDragged:
    (TabStripLastTabDraggedAlertCommand*)command {
  self.lastTabDraggedCommand = command;
}

- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                groupItem:(TabGroupItem*)tabGroupItem
                               sourceView:(UIView*)sourceView {
}

- (void)showTabStripTabGroupSnackbarAfterClosingGroups:
    (int)numberOfClosedGroups {
}

@end
