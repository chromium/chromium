// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/text_zoom_commands.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    case PopupMenuActionPageBookmark:
      RecordAction(UserMetricsAction("MobileMenuAddToBookmarks"));
      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
      [self.dispatcher bookmarkCurrentPage];
      break;
    case PopupMenuActionTranslate:
      base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
      [self.dispatcher showTranslate];
      break;
    case PopupMenuActionFindInPage:
      RecordAction(UserMetricsAction("MobileMenuFindInPage"));
      [self.dispatcher openFindInPage];
      break;
    case PopupMenuActionRequestDesktop:
      RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
      self.navigationAgent->RequestDesktopSite();
      [self.dispatcher showDefaultSiteViewIPH];
      break;
    case PopupMenuActionRequestMobile:
      RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
      self.navigationAgent->RequestMobileSite();
      break;
    case PopupMenuActionSiteInformation:
      RecordAction(UserMetricsAction("MobileMenuSiteInformation"));
      // TODO(crbug.com/1323758): This will need to be called on the
      // PageInfoCommands handler.
      [self.dispatcher showPageInfo];
      break;
    case PopupMenuActionReportIssue:
      RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      [self.dispatcher
          showReportAnIssueFromViewController:self.baseViewController
                                       sender:UserFeedbackSender::ToolsMenu];
      // Dismisses the popup menu without animation to allow the snapshot to be
      // taken without the menu presented.
      // TODO(crbug.com/1323764): This will need to be called on the
      // PopupMenuCommands handler.
      [self.dispatcher dismissPopupMenuAnimated:NO];
      break;
    case PopupMenuActionHelp:
      RecordAction(UserMetricsAction("MobileMenuHelp"));
      [self.dispatcher showHelpPage];
      break;
    case PopupMenuActionOpenDownloads:
      RecordAction(
          UserMetricsAction("MobileDownloadFolderUIShownFromToolsMenu"));
      [self.delegate recordDownloadsMetricsPerProfile];
      // TODO(crbug.com/906662): This will need to be called on the
      // BrowserCoordinatorCommands handler.
      [self.dispatcher showDownloadsFolder];
      break;
    case PopupMenuActionTextZoom:
      RecordAction(UserMetricsAction("MobileMenuTextZoom"));
      [self.dispatcher openTextZoom];
      break;
#if !defined(NDEBUG)
    case PopupMenuActionViewSource:
      // TODO(crbug.com/906662): This will need to be called on the
      // BrowserCoordinatorCommands handler.
      [self.dispatcher viewSource];
      break;
#endif  // !defined(NDEBUG)
    case PopupMenuActionOpenNewWindow:
      RecordAction(UserMetricsAction("MobileMenuNewWindow"));
      [self.dispatcher openNewWindowWithActivity:ActivityToLoadURL(
                                                     WindowActivityToolsOrigin,
                                                     GURL(kChromeUINewTabURL))];
      break;
    case PopupMenuActionFollow:
      [self.delegate updateFollowStatus];
      break;
    case PopupMenuActionBookmarks:
      RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
      LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
      [self.dispatcher showBookmarksManager];
      break;
    case PopupMenuActionReadingList:
      RecordAction(UserMetricsAction("MobileMenuReadingList"));
      // TODO(crbug.com/906662): This will need to be called on the
      // BrowserCoordinatorCommands handler.
      [self.dispatcher showReadingList];
      break;
    case PopupMenuActionRecentTabs:
      RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
      // TODO(crbug.com/906662): This will need to be called on the
      // BrowserCoordinatorCommands handler.
      [self.dispatcher showRecentTabs];
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
    case PopupMenuActionCloseTab:
      RecordAction(UserMetricsAction("MobileMenuCloseTab"));
      [self.dispatcher closeCurrentTab];
      break;
    case PopupMenuActionNavigate:
      // No metrics for this item.
      [self.delegate navigateToPageForItem:item];
      break;
    case PopupMenuActionVoiceSearch:
      RecordAction(UserMetricsAction("MobileMenuVoiceSearch"));
      [self.dispatcher startVoiceSearch];
      break;
    case PopupMenuActionSearch: {
      RecordAction(UserMetricsAction("MobileMenuSearch"));
      OpenNewTabCommand* command = [OpenNewTabCommand commandWithIncognito:NO];
      command.shouldFocusOmnibox = YES;
      [self.dispatcher openURLInNewTab:command];
      break;
    }
    case PopupMenuActionIncognitoSearch: {
      RecordAction(UserMetricsAction("MobileMenuIncognitoSearch"));
      OpenNewTabCommand* command = [OpenNewTabCommand commandWithIncognito:YES];
      command.shouldFocusOmnibox = YES;
      [self.dispatcher openURLInNewTab:command];
      break;
    }
    case PopupMenuActionQRCodeSearch:
      RecordAction(UserMetricsAction("MobileMenuScanQRCode"));
      // TODO(crbug.com/1323775): This will need to be called on the
      // QRScannerCommands handler.
      [self.dispatcher showQRScanner];
      break;
    case PopupMenuActionSearchCopiedImage: {
      RecordAction(UserMetricsAction("MobileMenuSearchCopiedImage"));
      [self.delegate searchCopiedImage];
      break;
    }
    case PopupMenuActionSearchCopiedText: {
      RecordAction(UserMetricsAction("MobileMenuPasteAndGo"));
      ClipboardRecentContent* clipboardRecentContent =
          ClipboardRecentContent::GetInstance();
      clipboardRecentContent->GetRecentTextFromClipboard(
          base::BindOnce(^(absl::optional<std::u16string> optional_text) {
            if (!optional_text) {
              return;
            }
            [self.dispatcher
                  loadQuery:base::SysUTF16ToNSString(optional_text.value())
                immediately:YES];
          }));
      break;
    }
    case PopupMenuActionVisitCopiedLink: {
      RecordAction(UserMetricsAction("MobileMenuPasteAndGo"));
      ClipboardRecentContent* clipboardRecentContent =
          ClipboardRecentContent::GetInstance();
      clipboardRecentContent->GetRecentURLFromClipboard(
          base::BindOnce(^(absl::optional<GURL> optional_url) {
            if (!optional_url) {
              return;
            }
            [self.dispatcher
                  loadQuery:base::SysUTF8ToNSString(optional_url.value().spec())
                immediately:YES];
          }));
      break;
    }
    case PopupMenuActionEnterpriseInfoMessage:
      [self.dispatcher
          openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:
                                                 GURL(kChromeUIManagementURL)]];
      break;
    default:
      NOTREACHED() << "Unexpected identifier";
      break;
  }

  // Close the tools menu.
  // TODO(crbug.com/1323764): This will need to be called on the
  // PopupMenuCommands handler.
  [self.dispatcher dismissPopupMenuAnimated:YES];
}

@end
