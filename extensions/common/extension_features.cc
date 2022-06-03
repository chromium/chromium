// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_features.h"
#include "base/feature_list.h"

namespace extensions_features {

// Controls whether we disable extensions that are marked as policy violation
// by the Omaha attribute.
const base::Feature kDisablePolicyViolationExtensionsRemotely{
    "DisablePolicyViolationExtensionsRemotely",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether we disable extensions that are marked as potentially
// unwanted by the Omaha attribute.
const base::Feature kDisablePotentiallyUwsExtensionsRemotely{
    "DisablePotentiallyUwsExtensionsRemotely",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether we show an install friction dialog when an Enhanced Safe
// Browsing user tries to install an extension that is not included in the
// Safe Browsing CRX allowlist. This feature also controls if we show a warning
// in 'chrome://extensions' for extensions not included in the allowlist.
const base::Feature kSafeBrowsingCrxAllowlistShowWarnings{
    "SafeBrowsingCrxAllowlistShowWarnings", base::FEATURE_ENABLED_BY_DEFAULT};

// Automatically disable extensions not included in the Safe Browsing CRX
// allowlist if the user has turned on Enhanced Safe Browsing (ESB). The
// extensions can be disabled at ESB opt-in time or when an extension is moved
// out of the allowlist.
const base::Feature kSafeBrowsingCrxAllowlistAutoDisable{
    "SafeBrowsingCrxAllowlistAutoDisable", base::FEATURE_DISABLED_BY_DEFAULT};

// Forces requests to go through WebRequestProxyingURLLoaderFactory.
const base::Feature kForceWebRequestProxyForTest{
    "ForceWebRequestProxyForTest", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the UI in the install prompt which lets a user choose to withhold
// requested host permissions by default.
const base::Feature kAllowWithholdingExtensionPermissionsOnInstall{
    "AllowWithholdingExtensionPermissionsOnInstall",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for the "match_origin_as_fallback" property in content
// scripts.
const base::Feature kContentScriptsMatchOriginAsFallback{
    "ContentScriptsMatchOriginAsFallback", base::FEATURE_DISABLED_BY_DEFAULT};

// Whether Manifest Version 3-based extensions are supported.
const base::Feature kMv3ExtensionsSupported{"Mv3ExtensionsSupported",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Reports Extensions.WebRequest.KeepaliveRequestFinished when enabled.
const base::Feature kReportKeepaliveUkm{"ReportKeepaliveUkm",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether every extension will require a locked process, preventing
// process sharing between extensions. See https://crbug.com/1209417.
const base::Feature kStrictExtensionIsolation{"StrictExtensionIsolation",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Whether extension contexts can use SharedArrayBuffers unconditionally (i.e.
// without requiring cross origin isolation).
// TODO(crbug.com/1184892): Flip this in M95.
const base::Feature kAllowSharedArrayBuffersUnconditionally{
    "AllowSharedArrayBuffersUnconditionally", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the CryptoToken component extension, which implements the deprecated
// U2F Security Key API. Once this flag is default disabled sites can continue
// to use CryptoToken via a Deprecation Trail with the same name.
// TODO(1224886): Delete together with CryptoToken code.
const base::Feature kU2FSecurityKeyAPI{"U2FSecurityKeyAPI",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Allows Manifest V3 (and greater) extensions to use web assembly. Note that
// this allows extensions to use remotely hosted web assembly which we don't
// want. This feature is intended for local development (by extension
// developers) only, and should never be flipped to ENABLED. This should be
// removed once web assembly support for manifest V3 is added. See
// crbug.com/1173354.
const base::Feature kAllowWasmInMV3{"AllowWasmInMV3",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, causes Manifest V3 (and greater) extensions to use structured
// cloning (instead of JSON serialization) for extension messaging, except when
// communicating with native messaging hosts.
const base::Feature kStructuredCloningForMV3Messaging{
    "StructuredCloningForMV3Messaging", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace extensions_features
