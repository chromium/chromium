// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
#define EXTENSIONS_COMMON_EXTENSION_FEATURES_H_

#include "base/feature_list.h"

namespace extensions_features {

extern const base::Feature kDisableMalwareExtensionsRemotely;

// Extension check up related features.
extern const base::Feature kExtensionsCheckup;
extern const char kExtensionsCheckupEntryPointParameter[];
extern const char kExtensionsCheckupBannerMessageParameter[];
extern const char kStartupEntryPoint[];
extern const char kNtpPromoEntryPoint[];
extern const char kPerformanceMessage[];
extern const char kPrivacyMessage[];
extern const char kNeutralMessage[];

extern const base::Feature kForceWebRequestProxyForTest;

extern const base::Feature kAllowWithholdingExtensionPermissionsOnInstall;

extern const base::Feature kContentScriptsMatchOriginAsFallback;

extern const base::Feature kReportKeepaliveUkm;

extern const base::Feature kReturnScopesInGetAuthToken;

extern const base::Feature kSelectedUserIdInGetAuthToken;

extern const base::Feature kCorbCorsAllowlist;
extern const char kCorbCorsAllowlistParamName[];

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
