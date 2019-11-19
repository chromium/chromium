// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A substring to identify activity strings that are from Password Management
// App Extensions. This string is intentionally without the leading and
// trailing "." so it can be used as a prefix, suffix, or substring of the
// App Extension's bundle ID.
NSString* const kFindLoginActionBundleSubstring = @"find-login-action";

// Returns whether |activity_string| refers to a supported Password Management
// App Extension. Supported extensions are listed in kAllPasswordManagerApps.
// The |exact_match| field defines whether an exact match of bundle_id is
// required to consider activity_string a match. To add more Password Manager
// extensions, add more entries to the static array.
bool IsPasswordManagerActivity(NSString* activity_string) {
  static struct {
    const char* bundle_id;
    bool exact_match;
  } kAllPasswordManagerApps[] = {
      // 1Password
      {"com.agilebits.onepassword-ios.extension", true},
      // LastPass
      {"com.lastpass.ilastpass.LastPassExt", true},
      // Dashlane
      {"com.dashlane.dashlanephonefinal.", false},
      // Enpass
      {"in.sinew.Walletx.WalletxExt", true},
      // Secrets touch
      {"com.outercorner.ios.Secrets.Search", true}};

  std::string activity = base::SysNSStringToUTF8(activity_string);
  for (const auto& app : kAllPasswordManagerApps) {
    std::string bundle_id(app.bundle_id);
    if (app.exact_match) {
      if (activity == bundle_id)
        return true;
    } else {
      if (activity.find(bundle_id) == 0)
        return true;
    }
  }
  return false;
}

}  // namespace

namespace activity_type_util {

struct PrefixTypeAssociation {
  activity_type_util::ActivityType type_;
  NSString* const prefix_;
  bool requiresExactMatch_;
};

const PrefixTypeAssociation prefixTypeAssociations[] = {
    {BOOKMARK, @"com.google.chrome.bookmarkActivity", true},
    {COPY, @"com.google.chrome.copyActivity", true},
    {NATIVE_FACEBOOK, @"com.apple.UIKit.activity.PostToFacebook", true},
    {NATIVE_MAIL, @"com.apple.UIKit.activity.Mail", true},
    {NATIVE_MESSAGE, @"com.apple.UIKit.activity.Message", true},
    {NATIVE_TWITTER, @"com.apple.UIKit.activity.PostToTwitter", true},
    {NATIVE_WEIBO, @"com.apple.UIKit.activity.PostToWeibo", true},
    {NATIVE_CLIPBOARD, @"com.apple.UIKit.activity.CopyToPasteboard", true},
    {PRINT, @"com.google.chrome.printActivity", true},
    {FIND_IN_PAGE, @"com.google.chrome.FindInPageActivityType", true},
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

ActivityType TypeFromString(NSString* activityString) {
  DCHECK(activityString);
  // Checks for the special case first so the more general patterns in
  // prefixTypeAssociations would not prematurely trapped them.
  NSRange found =
      [activityString rangeOfString:kFindLoginActionBundleSubstring];
  if (found.length)
    return APPEX_PASSWORD_MANAGEMENT;
  for (auto const& association : prefixTypeAssociations) {
    if (association.requiresExactMatch_) {
      if ([activityString isEqualToString:association.prefix_])
        return association.type_;
    } else {
      if ([activityString hasPrefix:association.prefix_])
        return association.type_;
    }
  }
  if (IsPasswordManagerActivity(activityString))
    return APPEX_PASSWORD_MANAGEMENT;
  return UNKNOWN;
}

NSString* CompletionMessageForActivity(ActivityType type) {
  // Some activities can be reported as completed even if not successful.
  // Make sure that the message is meaningful even if the activity completed
  // unsuccessfully.
  switch (type) {
    case COPY:
    case NATIVE_CLIPBOARD:
      return l10n_util::GetNSString(IDS_IOS_SHARE_TO_CLIPBOARD_SUCCESS);
    case APPEX_PASSWORD_MANAGEMENT:
      return l10n_util::GetNSString(IDS_IOS_APPEX_PASSWORD_FORM_FILLED_SUCCESS);
    default:
      return nil;
  }
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
    case FIND_IN_PAGE:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuFindInPage"));
      break;
    case PRINT:
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
    case APPEX_PASSWORD_MANAGEMENT:
      base::RecordAction(
          base::UserMetricsAction("MobileAppExFormFilledByPasswordManager"));
      break;
    case SEND_TAB_TO_SELF:
      base::RecordAction(
          base::UserMetricsAction("MobileShareMenuSendTabToSelf"));
      break;
  }
}

}  // activity_type_util
