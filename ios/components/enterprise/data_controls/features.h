// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_FEATURES_H_
#define IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_FEATURES_H_

#import "base/feature_list.h"

namespace data_controls {

// When this flag is enabled and the DataControlsRules enterprise policy is
// enabled, Enterprise admins can apply clipboard restrictions to Chrome users
// on iOS.
BASE_DECLARE_FEATURE(kEnableClipboardDataControlsIOS);

}  // namespace data_controls

#endif  // IOS_COMPONENTS_ENTERPRISE_DATA_CONTROLS_FEATURES_H_
