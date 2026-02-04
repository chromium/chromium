// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_ENTERPRISE_ANALYSIS_FEATURES_H_
#define IOS_COMPONENTS_ENTERPRISE_ANALYSIS_FEATURES_H_

#import "base/feature_list.h"

namespace enterprise_connectors {

// Controls whether the enterprise DLP file download protection feature is
// enabled on iOS.
BASE_DECLARE_FEATURE(kEnableFileDownloadConnectorIOS);

}  // namespace enterprise_connectors

#endif  // IOS_COMPONENTS_ENTERPRISE_ANALYSIS_FEATURES_H_
