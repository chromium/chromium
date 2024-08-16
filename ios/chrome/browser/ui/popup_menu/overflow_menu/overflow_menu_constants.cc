// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"

namespace overflow_menu {
// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
// LINT.IfChange(stringToDestination)
Destination DestinationForStringName(std::string destination) {
  if (destination == "overflow_menu::Destination::Bookmarks") {
    return overflow_menu::Destination::Bookmarks;
  } else if (destination == "overflow_menu::Destination::History") {
    return overflow_menu::Destination::History;
  } else if (destination == "overflow_menu::Destination::ReadingList") {
    return overflow_menu::Destination::ReadingList;
  } else if (destination == "overflow_menu::Destination::Passwords") {
    return overflow_menu::Destination::Passwords;
  } else if (destination == "overflow_menu::Destination::PriceNotifications") {
    return overflow_menu::Destination::PriceNotifications;
  } else if (destination == "overflow_menu::Destination::Downloads") {
    return overflow_menu::Destination::Downloads;
  } else if (destination == "overflow_menu::Destination::RecentTabs") {
    return overflow_menu::Destination::RecentTabs;
  } else if (destination == "overflow_menu::Destination::SiteInfo") {
    return overflow_menu::Destination::SiteInfo;
  } else if (destination == "overflow_menu::Destination::Settings") {
    return overflow_menu::Destination::Settings;
  } else if (destination == "overflow_menu::Destination::WhatsNew") {
    return overflow_menu::Destination::WhatsNew;
  } else if (destination == "overflow_menu::Destination::SpotlightDebugger") {
    return overflow_menu::Destination::SpotlightDebugger;
  } else {
    NOTREACHED_IN_MIGRATION();
    // Randomly chosen destination which should never be returned due to
    // NOTREACHED() above.
    return overflow_menu::Destination::Settings;
  }
}
// LINT.ThenChange(:destinationToString)

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
// LINT.IfChange(destinationToString)
std::string StringNameForDestination(Destination destination) {
  switch (destination) {
    case overflow_menu::Destination::Bookmarks:
      return "overflow_menu::Destination::Bookmarks";
    case overflow_menu::Destination::History:
      return "overflow_menu::Destination::History";
    case overflow_menu::Destination::ReadingList:
      return "overflow_menu::Destination::ReadingList";
    case overflow_menu::Destination::Passwords:
      return "overflow_menu::Destination::Passwords";
    case overflow_menu::Destination::PriceNotifications:
      return "overflow_menu::Destination::PriceNotifications";
    case overflow_menu::Destination::Downloads:
      return "overflow_menu::Destination::Downloads";
    case overflow_menu::Destination::RecentTabs:
      return "overflow_menu::Destination::RecentTabs";
    case overflow_menu::Destination::SiteInfo:
      return "overflow_menu::Destination::SiteInfo";
    case overflow_menu::Destination::Settings:
      return "overflow_menu::Destination::Settings";
    case overflow_menu::Destination::WhatsNew:
      return "overflow_menu::Destination::WhatsNew";
    case overflow_menu::Destination::SpotlightDebugger:
      return "overflow_menu::Destination::SpotlightDebugger";
  }
}
// LINT.ThenChange(:stringToDestination)

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
// LINT.IfChange(stringToActionType)
ActionType ActionTypeForStringName(std::string action) {
  if (action == "Reload") {
    return overflow_menu::ActionType::Reload;
  } else if (action == "NewTab") {
    return overflow_menu::ActionType::NewTab;
  } else if (action == "NewIncognitoTab") {
    return overflow_menu::ActionType::NewIncognitoTab;
  } else if (action == "NewWindow") {
    return overflow_menu::ActionType::NewWindow;
  } else if (action == "Follow") {
    return overflow_menu::ActionType::Follow;
  } else if (action == "Bookmark") {
    return overflow_menu::ActionType::Bookmark;
  } else if (action == "ReadingList") {
    return overflow_menu::ActionType::ReadingList;
  } else if (action == "ClearBrowsingData") {
    return overflow_menu::ActionType::ClearBrowsingData;
  } else if (action == "Translate") {
    return overflow_menu::ActionType::Translate;
  } else if (action == "DesktopSite") {
    return overflow_menu::ActionType::DesktopSite;
  } else if (action == "FindInPage") {
    return overflow_menu::ActionType::FindInPage;
  } else if (action == "TextZoom") {
    return overflow_menu::ActionType::TextZoom;
  } else if (action == "ReportAnIssue") {
    return overflow_menu::ActionType::ReportAnIssue;
  } else if (action == "Help") {
    return overflow_menu::ActionType::Help;
  } else if (action == "ShareChrome") {
    return overflow_menu::ActionType::ShareChrome;
  } else if (action == "EditActions") {
    return overflow_menu::ActionType::EditActions;
  } else if (action == "LensOverlay") {
    return overflow_menu::ActionType::LensOverlay;
  } else {
    NOTREACHED();
  }
}
// LINT.ThenChange(:actionTypeToString)

// LINT.IfChange(actionTypeToString)
std::string StringNameForActionType(ActionType action) {
  switch (action) {
    case overflow_menu::ActionType::Reload:
      return "Reload";
    case overflow_menu::ActionType::NewTab:
      return "NewTab";
    case overflow_menu::ActionType::NewIncognitoTab:
      return "NewIncognitoTab";
    case overflow_menu::ActionType::NewWindow:
      return "NewWindow";
    case overflow_menu::ActionType::Follow:
      return "Follow";
    case overflow_menu::ActionType::Bookmark:
      return "Bookmark";
    case overflow_menu::ActionType::ReadingList:
      return "ReadingList";
    case overflow_menu::ActionType::ClearBrowsingData:
      return "ClearBrowsingData";
    case overflow_menu::ActionType::Translate:
      return "Translate";
    case overflow_menu::ActionType::DesktopSite:
      return "DesktopSite";
    case overflow_menu::ActionType::FindInPage:
      return "FindInPage";
    case overflow_menu::ActionType::TextZoom:
      return "TextZoom";
    case overflow_menu::ActionType::ReportAnIssue:
      return "ReportAnIssue";
    case overflow_menu::ActionType::Help:
      return "Help";
    case overflow_menu::ActionType::ShareChrome:
      return "ShareChrome";
    case overflow_menu::ActionType::EditActions:
      return "EditActions";
    case overflow_menu::ActionType::LensOverlay:
      return "LensOverlay";
  }
}
// LINT.ThenChange(:stringToActionType)

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
void RecordUmaActionForDestination(Destination destination) {
  switch (destination) {
    case Destination::Bookmarks:
      base::RecordAction(base::UserMetricsAction("MobileMenuAllBookmarks"));
      break;
    case Destination::History:
      base::RecordAction(base::UserMetricsAction("MobileMenuHistory"));
      break;
    case Destination::ReadingList:
      base::RecordAction(base::UserMetricsAction("MobileMenuReadingList"));
      break;
    case Destination::Passwords:
      base::RecordAction(base::UserMetricsAction("MobileMenuPasswords"));
      break;
    case Destination::PriceNotifications:
      base::RecordAction(
          base::UserMetricsAction("MobileMenuPriceNotifications"));
      break;
    case Destination::Downloads:
      base::RecordAction(
          base::UserMetricsAction("MobileDownloadFolderUIShownFromToolsMenu"));
      break;
    case Destination::RecentTabs:
      base::RecordAction(base::UserMetricsAction("MobileMenuRecentTabs"));
      break;
    case Destination::SiteInfo:
      base::RecordAction(base::UserMetricsAction("MobileMenuSiteInformation"));
      break;
    case Destination::Settings:
      base::RecordAction(base::UserMetricsAction("MobileMenuSettings"));
      break;
    case Destination::WhatsNew:
      base::RecordAction(base::UserMetricsAction("MobileMenuWhatsNew"));
      break;
    case overflow_menu::Destination::SpotlightDebugger:
      // No need to log metrics for a debug-only feature.
      break;
  }
}
}  // namespace overflow_menu
