// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYSTEM_FLAGS_H_
#define IOS_CHROME_BROWSER_SYSTEM_FLAGS_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"

// This file can be empty. Its purpose is to contain the flags living in the
// System Settings, used for testing/debugging. No base::Feature (or check for
// them) should be added.

namespace experimental_flags {

enum WhatsNewPromoStatus {
  WHATS_NEW_DEFAULT = 0,          // Not forced to enable a promo.
  WHATS_NEW_TEST_COMMAND_TIP,     // Test Tip that runs a command.
  WHATS_NEW_MOVE_TO_DOCK_TIP,     // Force enable Move To Dock Tip promo.
  WHATS_NEW_REVIEW_UPDATED_TOS,   // Force enable Review Updated ToS promo.
  WHATS_NEW_DEFAULT_BROWSER_TIP,  // Force enable Set Default Browser promo.
  WHATS_NEW_PROMO_STATUS_COUNT,   // Count of Whats New Promo Statuses.
};

// Whether the First Run UI will be always be displayed.
bool AlwaysDisplayFirstRun();

// Returns the host name for an alternative Origin Server host for use by
// |BrandCode| startup ping. Returns empty string if there is no alternative
// host specified.
NSString* GetOriginServerHost();

// Returns the promo force enabled, as determined by the experimental flags.
// If |WHATS_NEW_DEFAULT| is returned, no promo is force enabled.
WhatsNewPromoStatus GetWhatsNewPromoStatus();

// Returns the URL for the alternative Discover Feed server.
NSString* GetAlternateDiscoverFeedServerURL();

// Returns true if the prefs for the notice card views count and clicks count
// should be reset to zero on feed start.
// TODO(crbug.com/1189232): Remove after launch.
bool ShouldResetNoticeCardOnFeedStart();

// Returns true if the count of showing the First Follow modal should be reset
// to zero.
// TODO(crbug.com/1312124): Remove after launch.
bool ShouldResetFirstFollowCount();

// Should be called after the count has been reset so that the resetting flag
// can be turned off.
// TODO(crbug.com/1312124): Remove after launch.
void DidResetFirstFollowCount();

// Whether memory debugging tools are enabled.
bool IsMemoryDebuggingEnabled();

// Whether startup crash is enabled.
bool IsStartupCrashEnabled();

// Whether the 3rd party keyboard omnibox workaround is enabled.
bool IsThirdPartyKeyboardWorkaroundEnabled();

// Whether the application group sandbox must be cleared before starting.
// Calling this method will reset the flag to false, so the sandbox is cleared
// only once.
bool MustClearApplicationGroupSandbox();

// Whether the DCheckIsFatal feature should be disabled.
bool AreDCHECKCrashesDisabled();

}  // namespace experimental_flags

#endif  // IOS_CHROME_BROWSER_SYSTEM_FLAGS_H_
