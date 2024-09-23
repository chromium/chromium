// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_SWITCHES_H_
#define FUCHSIA_WEB_WEBENGINE_SWITCHES_H_

#include "build/chromecast_buildflags.h"

// Switches used by the ContextProviderImpl to configure each Context process.
namespace switches {

// Enables use of custom content directories under the fuchsia-dir:// scheme.
// Directories will be mounted under a directory in the browser's namespace.
extern const char kEnableContentDirectories[];

// Enables Widevine CDM support.
extern const char kEnableWidevine[];

// Indicates that the Context was created without a |data_directory|.
extern const char kIncognito[];

// Enables PlayReady CDM and specifies the corresponding key system string.
extern const char kPlayreadyKeySystem[];

// Enables publishing of a chromium.internal.DevToolsConnector service by
// WebEngine instances, to allow debug-mode DevTools usage, for testing.
extern const char kEnableRemoteDebugMode[];

// Specifies a custom UserAgent product & version to use.
extern const char kUserAgentProductAndVersion[];

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
// By default, an HTTPS page cannot run JavaScript, CSS or resources from HTTP
// URLs. This provides an override to get the old insecure behavior.
// TODO(crbug.com/40050660): Remove this switch when it is no longer
// necessary.
extern const char kAllowRunningInsecureContent[];

// Enables use of the fuchsia.legacymetrics.MetricsRecorder service for
// telemetry.
extern const char kUseLegacyMetricsService[];
#endif

// Specifies a comma-separated list of HTTP headers to exempt from CORS checks.
extern const char kCorsExemptHeaders[];

// Enables the Cast Streaming Receiver.
// TODO(crbug.com/40129708): Consider removing this flag when we have a better
// way of enabling this feature.
extern const char kEnableCastStreamingReceiver[];

// Data directory to be used for CDM user data.
extern const char kCdmDataDirectory[];

// Quota to apply to the CDM user data directory, in bytes.
extern const char kCdmDataQuotaBytes[];

// Soft quota to apply to the Context's persistent data directory, in bytes.
extern const char kDataQuotaBytes[];

// API Key used to access Google services.
extern const char kGoogleApiKey[];

// Used to tell the WebEngine executable to run the ContextProvider service.
extern const char kContextProvider[];

// Equivalent to Chrome's --proxy-bypass-list switch.
extern const char kProxyBypassList[];

// Uses a specified proxy server, overrides system settings.
extern const char kProxyServer[];

}  // namespace switches

#endif  // FUCHSIA_WEB_WEBENGINE_SWITCHES_H_
