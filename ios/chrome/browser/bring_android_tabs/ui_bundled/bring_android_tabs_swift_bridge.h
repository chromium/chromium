// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_SWIFT_BRIDGE_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_SWIFT_BRIDGE_H_

// Bridging header between Swift and Obj-C. These types/imports need to be pure
// Obj-C and have no C++ in them.
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_prompt_view_controller_delegate.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"

// Explicitly import the bridging header of the Swift dependencies, as the
// implicit import of these bridging header is deprecated and will be removed in
// a later version of Swift.
#import "ui/base/l10n/l10n_util_mac_bridge.h"

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_SWIFT_BRIDGE_H_
