// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_DATA_CONTROLS_UTILS_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_DATA_CONTROLS_UTILS_H_

#import <UIKit/UIKit.h>

#import <string_view>

#import "components/enterprise/data_controls/core/browser/data_controls_dialog.h"

namespace data_controls {

// IOS implementation of `DataControlsDialog`. The warning dialog or blocking
// toast shown to the user when a Data Controls policy is triggered.
struct WarningDialog {
  NSString* title;
  NSString* label;
  NSString* ok_button_id;
  NSString* cancel_button_id;
};

WarningDialog GetWarningDialog(DataControlsDialog::Type type,
                               std::string_view organization_domain);

}  // namespace data_controls

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_UTILS_DATA_CONTROLS_UTILS_H_
