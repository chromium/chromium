// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBUI_REGIONAL_CAPABILITIES_INTERNALS_CONSTANTS_H_
#define COMPONENTS_WEBUI_REGIONAL_CAPABILITIES_INTERNALS_CONSTANTS_H_

namespace regional_capabilities {

inline constexpr char kChromeUIRegionalCapabilitiesInternalsHost[] =
    "regional-capabilities-internals";

// Internals data keys expected in the `resources/app.ts` file.
extern const char kActiveProgramNameKey[];
extern const char kActiveCountryCodeKey[];
extern const char kPrefsCountryCodeKey[];

}  // namespace regional_capabilities

#endif  // COMPONENTS_WEBUI_REGIONAL_CAPABILITIES_INTERNALS_CONSTANTS_H_
