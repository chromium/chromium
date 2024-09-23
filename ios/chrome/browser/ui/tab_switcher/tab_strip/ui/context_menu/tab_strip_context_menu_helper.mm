// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/context_menu/tab_strip_context_menu_helper.h"

#import "base/check.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_features_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "url/gurl.h"

namespace {

// Creates a `UIMenu` displayed inline with `children` as elements.
UIMenu* CreateDisplayInlineUIMenu(NSArray<UIMenuElement*>* children) {
  return [UIMenu menuWithTitle:@""
                         image:nil
                    identifier:nil
                       options:UIMenuOptionsDisplayInline
                      children:children];
}

// Creates a `UIContextMenuConfiguration` with `children` as elements.
UIContextMenuConfiguration* CreateUIContextMenuConfiguration(
    NSArray<UIMenuElement*>* children) {
  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        return [UIMenu menuWithTitle:@"" children:children];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

}  // namespace

@implementation TabStripContextMenuHelper {
  raw_ptr<BrowserList> _browserList;
  base::WeakPtr<WebStateList> _webStateList;
}

- (instancetype)initWithBrowserList:(BrowserList*)browserList
                       webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    CHECK(browserList);
    CHECK(webStateList);
    _browserList = browserList;
    _webStateList = webStateList->AsWeakPtr();
  }
  return self;
}

- (void)disconnect {
  _browserList = nullptr;
  _webStateList = nullptr;
}

#pragma mark - TabStripContextMenuProvider

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabSwitcherItem:(TabSwitcherItem*)tabSwitcherItem
                                    originView:(UIView*)originView
                                  menuScenario:
                                      (enum MenuScenarioHistogram)scenario {
  NSArray<UIMenuElement*>* elements =
      [self menuElementsForTabSwitcherItem:tabSwitcherItem
                                originView:originView
                              menuScenario:scenario];
  return CreateUIContextMenuConfiguration(elements);
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForTabGroupItem:(TabGroupItem*)tabGroupItem
                                 originView:(UIView*)originView
                               menuScenario:
                                   (enum MenuScenarioHistogram)scenario {
  NSArray<UIMenuElement*>* elements =
      [self menuElementsForTabGroupItem:tabGroupItem
                             originView:originView
                           menuScenario:scenario];
  return CreateUIContextMenuConfiguration(elements);
}

#pragma mark - Private

- (NSArray<UIMenuElement*>*)
    menuElementsForTabSwitcherItem:(TabSwitcherItem*)tabSwitcherItem
                        originView:(UIView*)originView
                      menuScenario:(MenuScenarioHistogram)scenario {
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);
  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];
  __weak __typeof(self) weakSelf = self;

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  // If groups are enabled, add "Add Tab to Group" menu.
  if ([TabStripFeaturesUtils isModernTabStripWithTabGroups]) {
    std::set<const TabGroup*> groups =
        GetAllGroupsForBrowserList(_browserList, self.incognito);
    CHECK(_webStateList);
    int webStateIndex = GetWebStateIndex(
        _webStateList.get(),
        WebStateSearchCriteria{.identifier = tabSwitcherItem.identifier});
    CHECK(_webStateList->ContainsIndex(webStateIndex),
          base::NotFatalUntil::M128);
    const TabGroup* currentGroup =
        _webStateList->GetGroupOfWebStateAt(webStateIndex);
    auto addTabToGroupBlock = ^(const TabGroup* group) {
      if (group) {
        [weakSelf.mutator addItem:tabSwitcherItem toGroup:group];
      } else {
        [weakSelf.mutator createNewGroupWithItem:tabSwitcherItem];
      }
    };
    if (currentGroup) {
      auto removeTabFromGroupBlock = ^{
        [weakSelf.mutator removeItemFromGroup:tabSwitcherItem];
      };
      UIMenuElement* moveTabToGroupMenu = [actionFactory
          menuToMoveTabToGroupWithGroups:groups
                            currentGroup:currentGroup
                               moveBlock:addTabToGroupBlock
                             removeBlock:removeTabFromGroupBlock];
      [menuElements addObject:moveTabToGroupMenu];
    } else {
      UIMenuElement* addTabToGroupMenu =
          [actionFactory menuToAddTabToGroupWithGroups:groups
                                          numberOfTabs:1
                                                 block:addTabToGroupBlock];
      [menuElements addObject:addTabToGroupMenu];
    }
  }

  // If tab is not NTP, add "Share" menu.
  if (!IsURLNewTabPage(tabSwitcherItem.URL)) {
    UIAction* shareAction = [actionFactory actionToShareWithBlock:^{
      [weakSelf.handler shareItem:tabSwitcherItem originView:originView];
    }];
    UIMenu* shareMenu = CreateDisplayInlineUIMenu(@[ shareAction ]);
    [menuElements addObject:shareMenu];
  }

  // Add "Close" menu to close this tab or all tabs except this one.
  NSMutableArray<UIMenuElement*>* closeMenuElements =
      [[NSMutableArray alloc] init];
  UIAction* closeTabAction = [actionFactory actionToCloseRegularTabWithBlock:^{
    [weakSelf.mutator closeItem:tabSwitcherItem];
  }];
  [closeMenuElements addObject:closeTabAction];
  UIAction* closeOtherTabsAction =
      [actionFactory actionToCloseAllOtherTabsWithBlock:^{
        [weakSelf.mutator closeAllItemsExcept:tabSwitcherItem];
      }];
  [closeMenuElements addObject:closeOtherTabsAction];
  UIMenu* closeMenu = CreateDisplayInlineUIMenu(closeMenuElements);
  [menuElements addObject:closeMenu];

  return menuElements;
}

- (NSArray<UIMenuElement*>*)
    menuElementsForTabGroupItem:(TabGroupItem*)tabGroupItem
                     originView:(UIView*)originView
                   menuScenario:(MenuScenarioHistogram)scenario {
  // Record that this context menu was shown to the user.
  RecordMenuShown(scenario);
  ActionFactory* actionFactory =
      [[ActionFactory alloc] initWithScenario:scenario];
  __weak __typeof(self) weakSelf = self;

  NSMutableArray<UIMenuElement*>* menuElements = [[NSMutableArray alloc] init];

  // Add menu to edit group e.g. rename it, add a new tab to it or ungroup it.
  NSMutableArray<UIMenuElement*>* editGroupMenuElements =
      [[NSMutableArray alloc] init];

  base::WeakPtr<const TabGroup> tabGroup = tabGroupItem.tabGroup->GetWeakPtr();

  [editGroupMenuElements
      addObject:[actionFactory actionToRenameTabGroupWithBlock:^{
        [weakSelf.handler showTabStripGroupEditionForGroup:tabGroup];
      }]];
  [editGroupMenuElements
      addObject:[actionFactory actionToAddNewTabInGroupWithBlock:^{
        [weakSelf.mutator addNewTabInGroup:tabGroupItem];
      }]];
  [editGroupMenuElements
      addObject:[actionFactory actionToUngroupTabGroupWithBlock:^{
        [weakSelf.mutator ungroupGroup:tabGroupItem sourceView:originView];
      }]];
  UIMenu* editGroupMenu = CreateDisplayInlineUIMenu(editGroupMenuElements);
  [menuElements addObject:editGroupMenu];

  if (IsTabGroupSyncEnabled()) {
    [menuElements addObject:[actionFactory actionToCloseTabGroupWithBlock:^{
                    [weakSelf.mutator closeGroup:tabGroupItem];
                  }]];
    if (!self.incognito) {
      [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                      [weakSelf.mutator deleteGroup:tabGroupItem
                                         sourceView:originView];
                    }]];
    }
  } else {
    [menuElements addObject:[actionFactory actionToDeleteTabGroupWithBlock:^{
                    [weakSelf.mutator deleteGroup:tabGroupItem
                                       sourceView:originView];
                  }]];
  }

  return menuElements;
}

@end
