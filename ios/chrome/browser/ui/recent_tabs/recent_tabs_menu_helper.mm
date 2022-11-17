// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/synced_sessions.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface RecentTabsContextMenuHelper () <RecentTabsMenuProvider>

@property(nonatomic, assign) Browser* browser;

@property(nonatomic, weak) id<RecentTabsPresentationDelegate>
    recentTabsPresentationDelegate;

@property(nonatomic, weak) id<TabContextMenuDelegate> contextMenuDelegate;

@end

@implementation RecentTabsContextMenuHelper

- (instancetype)initWithBrowser:(Browser*)browser
    recentTabsPresentationDelegate:
        (id<RecentTabsPresentationDelegate>)recentTabsPresentationDelegate
            tabContextMenuDelegate:
                (id<TabContextMenuDelegate>)tabContextMenuDelegate {
  self = [super init];
  if (self) {
    _browser = browser;
    _recentTabsPresentationDelegate = recentTabsPresentationDelegate;
    _contextMenuDelegate = tabContextMenuDelegate;
  }
  return self;
}

#pragma mark - RecentTabsMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (TableViewURLItem*)item
                                                      fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    if (!weakSelf) {
      // Return an empty menu.
      return [UIMenu menuWithTitle:@"" children:@[]];
    }

    RecentTabsContextMenuHelper* strongSelf = weakSelf;

    // Record that this context menu was shown to the user.
    RecordMenuShown(MenuScenarioHistogram::kRecentTabsEntry);

    BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
        initWithBrowser:strongSelf.browser
               scenario:MenuScenarioHistogram::kRecentTabsEntry];

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    GURL gurl;
    if (item.URL) {
      gurl = item.URL.gurl;
    }
    [menuElements
        addObject:
            [actionFactory
                actionToOpenInNewTabWithURL:gurl
                                 completion:^{
                                   [weakSelf.recentTabsPresentationDelegate
                                           showActiveRegularTabFromRecentTabs];
                                 }]];

    if (base::ios::IsMultipleScenesSupported()) {
      [menuElements
          addObject:[actionFactory
                        actionToOpenInNewWindowWithURL:gurl
                                        activityOrigin:
                                            WindowActivityRecentTabsOrigin]];
    }

    [menuElements addObject:[actionFactory actionToCopyURL:gurl]];

    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf.contextMenuDelegate
                        shareURL:gurl
                           title:item.title
                        scenario:ActivityScenario::RecentTabsEntry
                        fromView:view];
                  }]];

    return [UIMenu menuWithTitle:@"" children:menuElements];
  };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (UIContextMenuConfiguration*)
    contextMenuConfigurationForHeaderWithSectionIdentifier:
        (NSInteger)sectionIdentifier {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        RecentTabsContextMenuHelper* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenarioHistogram::kRecentTabsHeader);

        ActionFactory* actionFactory = [[ActionFactory alloc]
            initWithScenario:MenuScenarioHistogram::kRecentTabsHeader];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        synced_sessions::DistantSession const* session =
            [weakSelf.contextMenuDelegate
                sessionForTableSectionWithIdentifier:sectionIdentifier];

        if (!session->tabs.empty()) {
          [menuElements addObject:[actionFactory actionToOpenAllTabsWithBlock:^{
                          [strongSelf.recentTabsPresentationDelegate
                              openAllTabsFromSession:session];
                        }]];
        }

        [menuElements
            addObject:[actionFactory actionToHideWithBlock:^{
              [strongSelf.contextMenuDelegate
                  removeSessionAtTableSectionWithIdentifier:sectionIdentifier];
            }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

@end
