// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace overflow_menu {
// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
Destination DestinationForStringName(std::string destination) {
  if (destination == "overflow_menu::Destination::Bookmarks") {
    return overflow_menu::Destination::Bookmarks;
  } else if (destination == "overflow_menu::Destination::History") {
    return overflow_menu::Destination::History;
  } else if (destination == "overflow_menu::Destination::ReadingList") {
    return overflow_menu::Destination::ReadingList;
  } else if (destination == "overflow_menu::Destination::Passwords") {
    return overflow_menu::Destination::Passwords;
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
  } else {
    NOTREACHED();
    // Randomly chosen destination which should never be returned due to
    // NOTREACHED() above.
    return overflow_menu::Destination::Settings;
  }
}

Destination DestinationForNSStringName(NSString* destination) {
  std::string name = base::SysNSStringToUTF8(destination);

  return DestinationForStringName(name);
}

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

NSString* NSStringNameForDestination(Destination destination) {
  std::string name = StringNameForDestination(destination);

  return base::SysUTF8ToNSString(name);
}

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
