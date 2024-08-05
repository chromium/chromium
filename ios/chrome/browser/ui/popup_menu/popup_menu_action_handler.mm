// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/load_query_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

using base::RecordAction;
using base::UserMetricsAction;

@implementation PopupMenuActionHandler

#pragma mark - PopupMenuTableViewControllerDelegate

- (void)popupMenuTableViewController:(PopupMenuTableViewController*)sender
                       didSelectItem:(TableViewItem<PopupMenuItem>*)item
                              origin:(CGPoint)origin {
  DCHECK(self.dispatcher);
  DCHECK(self.delegate);

  PopupMenuAction identifier = item.actionIdentifier;
  switch (identifier) {
    case PopupMenuActionReload:
      RecordAction(UserMetricsAction("MobileMenuReload"));
      self.navigationAgent->Reload();
      break;
    case PopupMenuActionStop:
      RecordAction(UserMetricsAction("MobileMenuStop"));
      self.navigationAgent->StopLoading();
      break;
    case PopupMenuActionOpenNewTab:
      RecordAction(UserMetricsAction("MobileMenuNewTab"));
      RecordAction(UserMetricsAction("MobileTabNewTab"));

      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithIncognito:NO
                                                      originPoint:origin]];
      break;
    case PopupMenuActionOpenNewIncognitoTab:
      RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithIncognito:YES
                                                      originPoint:origin]];
      break;
    case PopupMenuActionReadLater:
      RecordAction(UserMetricsAction("MobileMenuReadLater"));
      [self.delegate readPageLater];
      break;
    case PopupMenuActionPageBookmark: {
      RecordAction(UserMetricsAction("MobileMenuAddToOrEditBookmark"));
      web::WebState* currentWebState = self.delegate.currentWebState;
      if (!currentWebState) {
        return;
      }
      [self.bookmarksCommandsHandler bookmarkWithWebState:currentWebState];
      break;
    }
    case PopupMenuActionTranslate:
      base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
      [self.browserCoordinatorCommandsHandler showTranslate];
      break;
    case PopupMenuActionFindInPage:
      RecordAction(UserMetricsAction("MobileMenuFindInPage"));
      [self.dispatcher openFindInPage];
      break;
    case PopupMenuActionRequestDesktop:
      RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
      self.navigationAgent->RequestDesktopSite();
      [self.helpHandler
          presentInProductHelpWithType:InProductHelpType::kDefaultSiteView];
      break;
    case PopupMenuActionRequestMobile:
      RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
      self.navigationAgent->RequestMobileSite();
      break;
    case PopupMenuActionSiteInformation:
      RecordAction(UserMetricsAction("MobileMenuSiteInformation"));
      [self.pageInfoCommandsHandler showPageInfo];
      break;
    case PopupMenuActionReportIssue:
      RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      [self.dispatcher
          showReportAnIssueFromViewController:self.baseViewController
                                       sender:UserFeedbackSender::ToolsMenu];
      // Dismisses the popup menu without animation to allow the snapshot to be
      // taken without the menu presented.
      [self.popupMenuCommandsHandler dismissPopupMenuAnimated:NO];
      break;
    case PopupMenuActionHelp:
      RecordAction(UserMetricsAction("MobileMenuHelp"));
      [self.browserCoordinatorCommandsHandler showHelpPage];
      break;
    case PopupMenuActionOpenDownloads:
      RecordAction(
          UserMetricsAction("MobileDownloadFolderUIShownFromToolsMenu"));
      [self.delegate recordDownloadsMetricsPerProfile];
      [self.browserCoordinatorCommandsHandler showDownloadsFolder];
      break;
    case PopupMenuActionTextZoom:
      RecordAction(UserMetricsAction("MobileMenuTextZoom"));
      [self.dispatcher openTextZoom];
      break;
#if !defined(NDEBUG)
    case PopupMenuActionViewSource:
      [self.browserCoordinatorCommandsHandler viewSource];
      break;
#endif  // !defined(NDEBUG)
    case PopupMenuActionOpenNewWindow:
      RecordAction(UserMetricsAction("MobileMenuNewWindow"));
      [self.dispatcher openNewWindowWithActivity:ActivityToLoadURL(
                                                     WindowActivityToolsOrigin,
                                                     GURL(kChromeUINewTabURL))];
      break;
    case PopupMenuActionFollow:
      [self.delegate toggleFollowed];
      break;
    case PopupMenuActionBookmarks:
      RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
      [self.browserCoordinatorCommandsHandler showBookmarksManager];
      break;
    case PopupMenuActionReadingList:
      RecordAction(UserMetricsAction("MobileMenuReadingList"));
      [self.browserCoordinatorCommandsHandler showReadingList];
      break;
    case PopupMenuActionRecentTabs:
      RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
      [self.browserCoordinatorCommandsHandler showRecentTabs];
      break;
    case PopupMenuActionHistory:
      RecordAction(UserMetricsAction("MobileMenuHistory"));
      [self.dispatcher showHistory];
      break;
    case PopupMenuActionSettings:
      RecordAction(UserMetricsAction("MobileMenuSettings"));
      [self.delegate recordSettingsMetricsPerProfile];
      [self.dispatcher showSettingsFromViewController:self.baseViewController];
      break;
    case PopupMenuActionEnterpriseInfoMessage:
      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:
                                                 GURL(kChromeUIManagementURL)]];
      break;
    case PopupMenuActionPriceNotifications:
      RecordAction(UserMetricsAction("MobileMenuPriceNotifications"));
      [self.dispatcher showPriceNotifications];
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unexpected identifier";
      break;
  }

  // Close the tools menu.
  [self.popupMenuCommandsHandler dismissPopupMenuAnimated:YES];
}

@end
