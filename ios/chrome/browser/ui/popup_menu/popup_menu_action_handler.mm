// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_action_handler_commands.h"
#import "ios/chrome/browser/ui/popup_menu/public/cells/popup_menu_item.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

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
  DCHECK(self.commandHandler);

  PopupMenuAction identifier = item.actionIdentifier;
  switch (identifier) {
    case PopupMenuActionReload:
      RecordAction(UserMetricsAction("MobileMenuReload"));
      [self.dispatcher reload];
      break;
    case PopupMenuActionStop:
      RecordAction(UserMetricsAction("MobileMenuStop"));
      [self.dispatcher stopLoading];
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
      [self.commandHandler readPageLater];
      break;
    case PopupMenuActionPageBookmark:
      RecordAction(UserMetricsAction("MobileMenuAddToBookmarks"));
      [self.dispatcher bookmarkPage];
      break;
    case PopupMenuActionTranslate:
      base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
      [self.dispatcher showTranslate];
      break;
    case PopupMenuActionFindInPage:
      RecordAction(UserMetricsAction("MobileMenuFindInPage"));
      [self.dispatcher showFindInPage];
      break;
    case PopupMenuActionRequestDesktop:
      RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
      [self.dispatcher requestDesktopSite];
      break;
    case PopupMenuActionRequestMobile:
      RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
      [self.dispatcher requestMobileSite];
      break;
    case PopupMenuActionSiteInformation:
      RecordAction(UserMetricsAction("MobileMenuSiteInformation"));
      [self.dispatcher
          showPageInfoForOriginPoint:self.baseViewController.view.center];
      break;
    case PopupMenuActionReportIssue:
      RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
      [self.dispatcher
          showReportAnIssueFromViewController:self.baseViewController];
      // Dismisses the popup menu without animation to allow the snapshot to be
      // taken without the menu presented.
      [self.dispatcher dismissPopupMenuAnimated:NO];
      break;
    case PopupMenuActionHelp:
      RecordAction(UserMetricsAction("MobileMenuHelp"));
      [self.dispatcher showHelpPage];
      break;
#if !defined(NDEBUG)
    case PopupMenuActionViewSource:
      [self.dispatcher viewSource];
      break;
#endif  // !defined(NDEBUG)

    case PopupMenuActionBookmarks:
      RecordAction(UserMetricsAction("MobileMenuAllBookmarks"));
      [self.dispatcher showBookmarksManager];
      break;
    case PopupMenuActionReadingList:
      RecordAction(UserMetricsAction("MobileMenuReadingList"));
      [self.dispatcher showReadingList];
      break;
    case PopupMenuActionRecentTabs:
      RecordAction(UserMetricsAction("MobileMenuRecentTabs"));
      [self.dispatcher showRecentTabs];
      break;
    case PopupMenuActionHistory:
      RecordAction(UserMetricsAction("MobileMenuHistory"));
      [self.dispatcher showHistory];
      break;
    case PopupMenuActionSettings:
      RecordAction(UserMetricsAction("MobileMenuSettings"));
      [self.dispatcher showSettingsFromViewController:self.baseViewController];
      break;
    case PopupMenuActionCloseTab:
      RecordAction(UserMetricsAction("MobileMenuCloseTab"));
      [self.dispatcher closeCurrentTab];
      break;
    case PopupMenuActionNavigate:
      // No metrics for this item.
      [self.commandHandler navigateToPageForItem:item];
      break;
    case PopupMenuActionPasteAndGo: {
      RecordAction(UserMetricsAction("MobileMenuPasteAndGo"));
      NSString* query;
      ClipboardRecentContent* clipboardRecentContent =
          ClipboardRecentContent::GetInstance();
      if (base::Optional<GURL> optional_url =
              clipboardRecentContent->GetRecentURLFromClipboard()) {
        query = base::SysUTF8ToNSString(optional_url.value().spec());
      } else if (base::Optional<base::string16> optional_text =
                     clipboardRecentContent->GetRecentTextFromClipboard()) {
        query = base::SysUTF16ToNSString(optional_text.value());
      }
      [self.dispatcher loadQuery:query immediately:YES];
      break;
    }
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
      [self.dispatcher showQRScanner];
      break;
    case PopupMenuActionSearchCopiedImage: {
      RecordAction(UserMetricsAction("MobileMenuSearchCopiedImage"));
      ClipboardRecentContent* clipboardRecentContent =
          ClipboardRecentContent::GetInstance();
      if (base::Optional<gfx::Image> image =
              clipboardRecentContent->GetRecentImageFromClipboard()) {
        [self.dispatcher searchByImage:[image.value().ToUIImage() copy]];
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected identifier";
      break;
  }

  // Close the tools menu.
  [self.dispatcher dismissPopupMenuAnimated:YES];
}

@end
