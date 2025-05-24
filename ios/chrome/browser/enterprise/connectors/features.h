// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_FEATURES_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_FEATURES_H_

#import "base/feature_list.h"

namespace enterprise_connectors {

// Disables Enterprise Url filtering, ignoring the
// EnterpriseRealTimeUrlCheckMode policy.
BASE_DECLARE_FEATURE(kIOSEnterpriseRealtimeUrlFiltering);

}  // namespace enterprise_connectors

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_FEATURES_H_
