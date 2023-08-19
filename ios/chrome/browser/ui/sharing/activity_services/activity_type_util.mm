// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activity_type_util.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"

namespace activity_type_util {

struct PrefixTypeAssociation {
  activity_type_util::ActivityType type_;
  NSString* const prefix_;
  bool requiresExactMatch_;
};

ActivityType TypeFromString(NSString* activityString) {
  DCHECK(activityString);

  static const PrefixTypeAssociation prefixTypeAssociations[] = {
      {BOOKMARK, @"com.google.chrome.bookmarkActivity", true},
      {COPY, @"com.google.chrome.copyActivity", true},
      {NATIVE_FACEBOOK, @"com.apple.UIKit.activity.PostToFacebook", true},
      {NATIVE_MAIL, @"com.apple.UIKit.activity.Mail", true},
      {NATIVE_MESSAGE, @"com.apple.UIKit.activity.Message", true},
      {NATIVE_TWITTER, @"com.apple.UIKit.activity.PostToTwitter", true},
      {NATIVE_WEIBO, @"com.apple.UIKit.activity.PostToWeibo", true},
      {NATIVE_CLIPBOARD, @"com.apple.UIKit.activity.CopyToPasteboard", true},
      {NATIVE_SAVE_IMAGE, @"com.apple.UIKit.activity.SaveToCameraRoll", true},
      {NATIVE_SAVE_FILE, @"com.apple.DocumentManagerUICore.SaveToFiles", true},
      {NATIVE_MARKUP, @"com.apple.UIKit.activity.MarkupAsPDF", true},
      {NATIVE_PRINT, @"com.apple.UIKit.activity.Print", true},
      {NATIVE_ADD_TO_HOME, @"com.apple.UIKit.activity.AddToHomeScreen", true},
      {PRINT, @"com.google.chrome.printActivity", true},
      {FIND_IN_PAGE, @"com.google.chrome.FindInPageActivityType", true},
      {GENERATE_QR_CODE, @"com.google.chrome.GenerateQrCodeActivityType", true},
      // The trailing '.' prevents false positives.
      // For instance, "com.viberific" won't be matched by the "com.viber.".
      {GOOGLE_DRIVE, @"com.google.Drive.", false},
      {GOOGLE_GMAIL, @"com.google.Gmail.", false},
      {GOOGLE_GOOGLEPLUS, @"com.google.GooglePlus.", false},
      {GOOGLE_HANGOUTS, @"com.google.hangouts.", false},
      {GOOGLE_INBOX, @"com.google.inbox.", false},
      {READ_LATER, @"com.google.chrome.readingListActivity", true},
      {REQUEST_DESKTOP_MOBILE_SITE,
       @"com.google.chrome.requestDesktopOrMobileSiteActivity", true},
      {SEND_TAB_TO_SELF, @"com.google.com.sendTabToSelfActivity", true},
      {THIRD_PARTY_MAILBOX, @"com.orchestra.v2.", false},
      {THIRD_PARTY_FACEBOOK_MESSENGER, @"com.facebook.Messenger.", false},
      {THIRD_PARTY_WHATS_APP, @"net.whatsapp.WhatsApp.", false},
      {THIRD_PARTY_LINE, @"jp.naver.line.", false},
      {THIRD_PARTY_VIBER, @"com.viber.", false},
      {THIRD_PARTY_SKYPE, @"com.skype.", false},
      {THIRD_PARTY_TANGO, @"com.sgiggle.Tango.", false},
      {THIRD_PARTY_WECHAT, @"com.tencent.xin.", false},
      {THIRD_PARTY_EVERNOTE, @"com.evernote.", false},
      {THIRD_PARTY_PINTEREST, @"pinterest.", false},
      {THIRD_PARTY_POCKET, @"com.ideashower.ReadItLaterPro.", false},
      {THIRD_PARTY_READABILITY, @"com.readability.ReadabilityMobile.", false},
      {THIRD_PARTY_INSTAPAPER, @"com.marcoarment.instapaperpro.", false},
      // Put Google Unknown at the end to make sure it doesn't prevent anything
      // else from being recorded.
      {GOOGLE_UNKNOWN, @"com.google.", false},
  };

  for (auto const& association : prefixTypeAssociations) {
    if (association.requiresExactMatch_) {
      if ([activityString isEqualToString:association.prefix_]) {
        return association.type_;
      }
    } else {
      if ([activityString hasPrefix:association.prefix_]) {
        return association.type_;
      }
    }
  }
  return UNKNOWN;
}

void RecordMetricForActivity(ActivityType type) {
  switch (type) {
    case UNKNOWN:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuUnknown"));
      break;
    case BOOKMARK:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuBookmark"));
      break;
    case COPY:
    case NATIVE_CLIPBOARD:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuClipboard"));
      break;
    case NATIVE_SAVE_IMAGE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuSaveImage"));
      break;
    case NATIVE_SAVE_FILE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuSaveFile"));
      break;
    case NATIVE_MARKUP:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuMarkupPDF"));
      break;
    case FIND_IN_PAGE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuFindInPage"));
      break;
    case PRINT:
    case NATIVE_PRINT:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuPrint"));
      break;
    case READ_LATER:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuReadLater"));
      break;
    case REQUEST_DESKTOP_MOBILE_SITE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuRequestSite"));
      break;
    case GOOGLE_GMAIL:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuGmail"));
      break;
    case GOOGLE_GOOGLEPLUS:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuGooglePlus"));
      break;
    case GOOGLE_DRIVE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuGoogleDrive"));
      break;
    case GOOGLE_HANGOUTS:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuHangouts"));
      break;
    case GOOGLE_INBOX:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuInbox"));
      break;
    case GOOGLE_UNKNOWN:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuGoogleUnknown"));
      break;
    case NATIVE_MAIL:
    case THIRD_PARTY_MAILBOX:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuToMail"));
      break;
    case NATIVE_FACEBOOK:
    case NATIVE_TWITTER:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuToSocialNetwork"));
      break;
    case NATIVE_MESSAGE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuToSMSApp"));
      break;
    case NATIVE_WEIBO:
    case THIRD_PARTY_FACEBOOK_MESSENGER:
    case THIRD_PARTY_WHATS_APP:
    case THIRD_PARTY_LINE:
    case THIRD_PARTY_VIBER:
    case THIRD_PARTY_SKYPE:
    case THIRD_PARTY_TANGO:
    case THIRD_PARTY_WECHAT:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuToInstantMessagingApp"));
      break;
    case THIRD_PARTY_EVERNOTE:
    case THIRD_PARTY_PINTEREST:
    case THIRD_PARTY_POCKET:
    case THIRD_PARTY_READABILITY:
    case THIRD_PARTY_INSTAPAPER:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuToContentApp"));
      break;
    case SEND_TAB_TO_SELF:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuSendTabToSelf"));
      break;
    case GENERATE_QR_CODE:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuGenerateQRCode"));
      break;
    case NATIVE_ADD_TO_HOME:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuAddToHome"));
      break;
  }
}

}  // namespace activity_type_util
