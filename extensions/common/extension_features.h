// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
#define EXTENSIONS_COMMON_EXTENSION_FEATURES_H_

#include "base/feature_list.h"

namespace extensions_features {

extern const base::Feature kDisableMalwareExtensionsRemotely;
extern const base::Feature kDisablePolicyViolationExtensionsRemotely;
extern const base::Feature kDisablePotentiallyUwsExtensionsRemotely;
extern const base::Feature kSafeBrowsingCrxAllowlistShowWarnings;
extern const base::Feature kSafeBrowsingCrxAllowlistAutoDisable;

extern const base::Feature kForceWebRequestProxyForTest;

extern const base::Feature kAllowWithholdingExtensionPermissionsOnInstall;

extern const base::Feature kContentScriptsMatchOriginAsFallback;

extern const base::Feature kMv3ExtensionsSupported;

extern const base::Feature kReportKeepaliveUkm;

extern const base::Feature kStrictExtensionIsolation;

}  // namespace extensions_features

#endif  // EXTENSIONS_COMMON_EXTENSION_FEATURES_H_
