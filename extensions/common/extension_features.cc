// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"

namespace extensions_features {

// Controls whether we redirect the NTP to the chrome://extensions page or show
// a middle slot promo, and which of the the three checkup banner messages
// (performance focused, privacy focused or neutral) to show.
const base::Feature kExtensionsCheckupTool{"ExtensionsCheckupTool",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
// Parameters for ExtensionsCheckupTool feature.
const char kExtensionsCheckupToolEntryPointParameter[] = "entry_point";
const char kExtensionsCheckupToolBannerMessageParameter[] =
    "banner_message_type";

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
const base::Feature kForceWebRequestProxyForTest{
    "ForceWebRequestProxyForTest", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace extensions_features
