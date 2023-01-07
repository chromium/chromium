// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_FLAGS_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_FLAGS_INTERNAL_H_

#import "ios/web_view/public/cwv_flags.h"

NS_ASSUME_NONNULL_BEGIN

namespace ios_web_view {
// Flag name for whether or not sync sandbox is enabled.
extern const char kUseSyncSandboxFlagName[];

// Base flag name for whether or not wallet sandbox is enabled.
// Wallet sandbox flag has three choices (default, enabled, disabled). To turn
// this flag "on", the |kUseWalletSandboxFlagNameEnabled| needs to be enabled.
// Conversely, to turn this flag "off", the |kUseWalletSandboxFlagNameDisabled|
// needs to be enabled.
extern const char kUseWalletSandboxFlagName[];
extern const char kUseWalletSandboxFlagNameEnabled[];
extern const char kUseWalletSandboxFlagNameDisabled[];
}  // namespace

class PrefService;

@interface CWVFlags ()

- (instancetype)initWithPrefService:(PrefService*)prefService;

// Converts the configured flags into command line switches consumable by other
// features such as autofill and sync.
- (void)convertFlagsToCommandLineSwitches;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_FLAGS_INTERNAL_H_
