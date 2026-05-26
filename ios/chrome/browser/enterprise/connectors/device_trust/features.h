// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_FEATURES_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_connectors::features {

// Enables the Device Trust Connector flow on iOS.
BASE_DECLARE_FEATURE(kEnableIOSDeviceTrustConnector);

}  // namespace enterprise_connectors::features

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_FEATURES_H_
