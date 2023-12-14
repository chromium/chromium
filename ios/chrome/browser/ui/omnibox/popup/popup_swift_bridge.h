// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_SWIFT_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_SWIFT_BRIDGE_H_

// Bridging header between Swift and Obj-C. These types/imports need to be pure
// Obj-C and have no C++ in them.

#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"

// Explicitly import the bridging header of the Swift dependencies, as the
// implicit import of these bridging header is deprecated and will be removed in
// a later version of Swift.
#import "ios/chrome/common/ui/colors/swift_bridge.h"
#import "ui/base/l10n/l10n_util_mac_bridge.h"

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_SWIFT_BRIDGE_H_
