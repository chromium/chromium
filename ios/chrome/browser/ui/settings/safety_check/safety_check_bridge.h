// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_BRIDGE_H_

// Bridging header between Swift and Obj-C. These types/includes need to be pure
// Obj-C and have no C++ in them.

#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_consumer.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_service_delegate.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

// These are all just #defines so they can be included w/out any problem.
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"

#include "ui/base/l10n/l10n_util_mac_bridge.h"

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_BRIDGE_H_
