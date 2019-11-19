// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/first_run/first_run_configuration.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/ui/first_run/first_run_histograms.h"
#import "ios/chrome/browser/ui/settings/sync/utils/sync_util.h"
#import "ios/chrome/browser/ui/settings/utils/settings_utils.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/web/public/thread/web_thread.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kChromeFirstRunUIWillFinishNotification =
    @"kChromeFirstRunUIWillFinishNotification";

NSString* const kChromeFirstRunUIDidFinishNotification =
    @"kChromeFirstRunUIDidFinishNotification";

namespace {

NSString* RemoveLastWord(NSString* text) {
  __block NSRange range = NSMakeRange(0, [text length]);
  NSStringEnumerationOptions options = NSStringEnumerationByWords |
                                       NSStringEnumerationReverse |
                                       NSStringEnumerationSubstringNotRequired;

  // Enumerate backwards through the words in |text| to get the range of the
  // last word.
  [text
      enumerateSubstringsInRange:range
                         options:options
                      usingBlock:^(NSString* substring, NSRange substringRange,
                                   NSRange enclosingRange, BOOL* stop) {
                        range = substringRange;
                        *stop = YES;
                      }];
  return [text substringToIndex:range.location];
}

NSString* InsertNewlineBeforeNthToLastWord(NSString* text, int index) {
  __block NSRange range = NSMakeRange(0, [text length]);
  __block int count = 0;
  NSStringEnumerationOptions options = NSStringEnumerationByWords |
                                       NSStringEnumerationReverse |
                                       NSStringEnumerationSubstringNotRequired;
  [text
      enumerateSubstringsInRange:range
                         options:options
                      usingBlock:^(NSString* substring, NSRange substringRange,
                                   NSRange enclosingRange, BOOL* stop) {
                        range = substringRange;
                        count++;
                        *stop = count == index;
                      }];
  NSMutableString* textWithNewline = [text mutableCopy];
  [textWithNewline insertString:@"\n" atIndex:range.location];
  return textWithNewline;
}

// Trampoline method for Bind to create the sentinel file.
void CreateSentinel() {
  base::File::Error file_error;
  FirstRun::SentinelResult sentinel_created =
      FirstRun::CreateSentinel(&file_error);
  UMA_HISTOGRAM_ENUMERATION("FirstRun.Sentinel.Created", sentinel_created,
                            FirstRun::SentinelResult::SENTINEL_RESULT_MAX);
  if (sentinel_created == FirstRun::SentinelResult::SENTINEL_RESULT_FILE_ERROR)
    UMA_HISTOGRAM_ENUMERATION("FirstRun.Sentinel.CreatedFileError", -file_error,
                              -base::File::FILE_ERROR_MAX);
}

// Helper function for recording first run metrics.
void RecordFirstRunMetricsInternal(ios::ChromeBrowserState* browserState,
                                   bool sign_in_attempted,
                                   bool has_sso_accounts) {
  first_run::SignInStatus sign_in_status;
  bool user_signed_in = IdentityManagerFactory::GetForBrowserState(browserState)
                            ->HasPrimaryAccount();
  if (user_signed_in) {
    sign_in_status = has_sso_accounts
                         ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SUCCESSFUL
                         : first_run::SIGNIN_SUCCESSFUL;
  } else {
    if (sign_in_attempted) {
      sign_in_status = has_sso_accounts
                           ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_GIVEUP
                           : first_run::SIGNIN_SKIPPED_GIVEUP;
    } else {
      sign_in_status = has_sso_accounts
                           ? first_run::HAS_SSO_ACCOUNT_SIGNIN_SKIPPED_QUICK
                           : first_run::SIGNIN_SKIPPED_QUICK;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("FirstRun.SignIn", sign_in_status,
                            first_run::SIGNIN_SIZE);
}

}  // namespace

BOOL FixOrphanWord(UILabel* label) {
  // Calculate the height of the label's text.
  NSString* text = label.text;
  CGSize textSize =
      [text cr_boundingSizeWithSize:label.frame.size font:label.font];
  CGFloat textHeight = AlignValueToPixel(textSize.height);

  // Remove the last word and calculate the height of the new text.
  NSString* textMinusLastWord = RemoveLastWord(text);
  CGSize minusLastWordSize =
      [textMinusLastWord cr_boundingSizeWithSize:label.frame.size
                                            font:label.font];
  CGFloat minusLastWordHeight = AlignValueToPixel(minusLastWordSize.height);

  // Check if removing the last word results in a smaller height.
  if (minusLastWordHeight < textHeight) {
    // The last word was the only word on its line. Add a newline before the
    // second to last word.
    label.text = InsertNewlineBeforeNthToLastWord(text, 2);
    return true;
  }
  return false;
}

void WriteFirstRunSentinelAndRecordMetrics(
    ios::ChromeBrowserState* browserState,
    BOOL sign_in_attempted,
    BOOL has_sso_account) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CreateSentinel));
  RecordFirstRunMetricsInternal(browserState, sign_in_attempted,
                                has_sso_account);
}

void FinishFirstRun(ios::ChromeBrowserState* browserState,
                    web::WebState* web_state,
                    FirstRunConfiguration* config,
                    id<SyncPresenter> presenter) {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIWillFinishNotification
                    object:nil];
  WriteFirstRunSentinelAndRecordMetrics(browserState, config.signInAttempted,
                                        config.hasSSOAccount);

  // Display the sync errors infobar.
  DisplaySyncErrors(browserState, web_state, presenter);
}

void RecordProductTourTimingMetrics(NSString* timer_name,
                                    base::TimeTicks start_time) {
  base::TimeDelta delta = base::TimeTicks::Now() - start_time;
  NSString* histogramName =
      [NSString stringWithFormat:@"ProductTour.IOSScreens%@", timer_name];
  UMA_HISTOGRAM_CUSTOM_TIMES_FIRST_RUN(base::SysNSStringToUTF8(histogramName),
                                       delta,
                                       base::TimeDelta::FromMilliseconds(10),
                                       base::TimeDelta::FromMinutes(3), 50);
}

void FirstRunDismissed() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kChromeFirstRunUIDidFinishNotification
                    object:nil];
}
