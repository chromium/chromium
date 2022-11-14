// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_histograms.h"

#import "base/metrics/histogram_functions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ActivityType = activity_type_util::ActivityType;

namespace {
// Histogram names. Make sure to add an entry in histograms.xml when creating
// a new one that will get used.
const char kShareScenariosHistogram[] = "Mobile.Share.EntryPoints";
const char kShareOmniboxActionsHistogram[] =
    "Mobile.Share.TabShareButton.Actions";
const char kShareQRCodeImageActionsHistogram[] =
    "Mobile.Share.QRCodeImage.Actions";
const char kShareHistoryEntryActionsHistogram[] =
    "Mobile.Share.HistoryEntry.Actions";
const char kShareReadingListEntryActionsHistogram[] =
    "Mobile.Share.ReadingListEntry.Actions";
const char kShareBookmarkEntryActionsHistogram[] =
    "Mobile.Share.BookmarkEntry.Actions";
const char kShareMostVisitedEntryActionsHistogram[] =
    "Mobile.Share.MostVisitedEntry.Actions";
const char kShareRecentTabsEntryActionsHistogram[] =
    "Mobile.Share.RecentTabsEntry.Actions";
const char kShareSharedHighlightActionsHistogram[] =
    "Mobile.Share.SharedHighlight.Actions";
const char kShareTabGridItemActionsHistogram[] =
    "Mobile.Share.TabGridItem.Actions";
const char kShareTabGridSelectionModeActionsHistogram[] =
    "Mobile.Share.TabGridSelectionMode.Actions";
const char kShareShareChromeActionsHistogram[] =
    "Mobile.Share.ShareChrome.Actions";
const char kShareOmniboxMostVisitedEntryActionsHistogram[] =
    "Mobile.Share.OmniboxMostVisitedEntry.Actions";

// Enum representing an aggregation of the `ActivityType` enum values in a way
// that is relevant for metric collection. Current values should not
// be renumbered. Please keep in sync with "IOSShareAction" in
// src/tools/metrics/histograms/enums.xml.
enum class ShareActionType {
  Unknown = 0,
  Cancel = 1,
  Bookmark = 2,
  Copy = 3,
  SaveImage = 4,
  FindInPage = 5,
  Print = 6,
  ReadingList = 7,
  Mail = 8,
  RequestDesktopMobileSite = 9,
  SendTabToSelf = 10,
  CreateQRCode = 11,
  NativeMessage = 12,
  UnknownGoogleApp = 13,
  NativeSocialApp = 14,
  ThirdPartyMessagingApp = 15,
  ThirdPartyContentApp = 16,
  SaveFile = 17,
  Markup = 18,
  kMaxValue = Markup
};

ShareActionType MapActionType(ActivityType type) {
  switch (type) {
    case activity_type_util::UNKNOWN:
      return ShareActionType::Unknown;

    case activity_type_util::BOOKMARK:
      return ShareActionType::Bookmark;

    case activity_type_util::COPY:
    case activity_type_util::NATIVE_CLIPBOARD:
      return ShareActionType::Copy;

    case activity_type_util::NATIVE_SAVE_IMAGE:
      return ShareActionType::SaveImage;

    case activity_type_util::NATIVE_SAVE_FILE:
      return ShareActionType::SaveFile;

    case activity_type_util::NATIVE_MARKUP:
      return ShareActionType::Markup;

    case activity_type_util::FIND_IN_PAGE:
      return ShareActionType::FindInPage;

    case activity_type_util::PRINT:
    case activity_type_util::NATIVE_PRINT:
      return ShareActionType::Print;

    case activity_type_util::READ_LATER:
      return ShareActionType::ReadingList;

    case activity_type_util::THIRD_PARTY_MAILBOX:
    case activity_type_util::NATIVE_MAIL:
      return ShareActionType::Mail;

    case activity_type_util::REQUEST_DESKTOP_MOBILE_SITE:
      return ShareActionType::RequestDesktopMobileSite;

    case activity_type_util::SEND_TAB_TO_SELF:
      return ShareActionType::SendTabToSelf;

    case activity_type_util::GENERATE_QR_CODE:
      return ShareActionType::CreateQRCode;

    case activity_type_util::NATIVE_MESSAGE:
      return ShareActionType::NativeMessage;

    case activity_type_util::GOOGLE_DRIVE:
    case activity_type_util::GOOGLE_GMAIL:
    case activity_type_util::GOOGLE_GOOGLEPLUS:
    case activity_type_util::GOOGLE_HANGOUTS:
    case activity_type_util::GOOGLE_INBOX:
    case activity_type_util::GOOGLE_UNKNOWN:
      return ShareActionType::UnknownGoogleApp;

    case activity_type_util::NATIVE_FACEBOOK:
    case activity_type_util::NATIVE_TWITTER:
      return ShareActionType::NativeSocialApp;

    case activity_type_util::NATIVE_WEIBO:
    case activity_type_util::THIRD_PARTY_FACEBOOK_MESSENGER:
    case activity_type_util::THIRD_PARTY_WHATS_APP:
    case activity_type_util::THIRD_PARTY_LINE:
    case activity_type_util::THIRD_PARTY_VIBER:
    case activity_type_util::THIRD_PARTY_SKYPE:
    case activity_type_util::THIRD_PARTY_TANGO:
    case activity_type_util::THIRD_PARTY_WECHAT:
      return ShareActionType::ThirdPartyMessagingApp;

    case activity_type_util::THIRD_PARTY_EVERNOTE:
    case activity_type_util::THIRD_PARTY_PINTEREST:
    case activity_type_util::THIRD_PARTY_POCKET:
    case activity_type_util::THIRD_PARTY_READABILITY:
    case activity_type_util::THIRD_PARTY_INSTAPAPER:
      return ShareActionType::ThirdPartyContentApp;
  }
}

void RecordActionForScenario(ShareActionType actionType,
                             ActivityScenario scenario) {
  const char* histogramName;
  switch (scenario) {
    case ActivityScenario::TabShareButton:
      histogramName = kShareOmniboxActionsHistogram;
      break;
    case ActivityScenario::QRCodeImage:
      histogramName = kShareQRCodeImageActionsHistogram;
      break;
    case ActivityScenario::HistoryEntry:
      histogramName = kShareHistoryEntryActionsHistogram;
      break;
    case ActivityScenario::ReadingListEntry:
      histogramName = kShareReadingListEntryActionsHistogram;
      break;
    case ActivityScenario::BookmarkEntry:
      histogramName = kShareBookmarkEntryActionsHistogram;
      break;
    case ActivityScenario::MostVisitedEntry:
      histogramName = kShareMostVisitedEntryActionsHistogram;
      break;
    case ActivityScenario::RecentTabsEntry:
      histogramName = kShareRecentTabsEntryActionsHistogram;
      break;
    case ActivityScenario::SharedHighlight:
      histogramName = kShareSharedHighlightActionsHistogram;
      break;
    case ActivityScenario::TabGridItem:
      histogramName = kShareTabGridItemActionsHistogram;
      break;
    case ActivityScenario::TabGridSelectionMode:
      histogramName = kShareTabGridSelectionModeActionsHistogram;
      break;
    case ActivityScenario::ShareChrome:
      histogramName = kShareShareChromeActionsHistogram;
      break;
    case ActivityScenario::OmniboxMostVisitedEntry:
      histogramName = kShareOmniboxMostVisitedEntryActionsHistogram;
  }
  base::UmaHistogramEnumeration(histogramName, actionType);
}

}  // namespace

#pragma mark - Public Methods

void RecordScenarioInitiated(ActivityScenario scenario) {
  base::UmaHistogramEnumeration(kShareScenariosHistogram, scenario);
}

void RecordActivityForScenario(ActivityType type, ActivityScenario scenario) {
  ShareActionType actionType = MapActionType(type);
  RecordActionForScenario(actionType, scenario);
}

void RecordCancelledScenario(ActivityScenario scenario) {
  RecordActionForScenario(ShareActionType::Cancel, scenario);
}
