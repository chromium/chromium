// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"

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
    RecordMenuShown(kMenuScenarioHistogramRecentTabsEntry);

    BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
        initWithBrowser:strongSelf.browser
               scenario:kMenuScenarioHistogramRecentTabsEntry];

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    GURL gurl;
    if (item.URL) {
      gurl = item.URL.gurl;
    }

    if (base::ios::IsMultipleScenesSupported()) {
      [menuElements
          addObject:[actionFactory
                        actionToOpenInNewWindowWithURL:gurl
                                        activityOrigin:
                                            WindowActivityRecentTabsOrigin]];
    }

    CrURL* URL = [[CrURL alloc] initWithGURL:gurl];
    [menuElements addObject:[actionFactory actionToCopyURL:URL]];

    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf.contextMenuDelegate
                        shareURL:gurl
                           title:item.title
                        scenario:SharingScenario::RecentTabsEntry
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
        RecordMenuShown(kMenuScenarioHistogramRecentTabsHeader);

        ActionFactory* actionFactory = [[ActionFactory alloc]
            initWithScenario:kMenuScenarioHistogramRecentTabsHeader];

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
