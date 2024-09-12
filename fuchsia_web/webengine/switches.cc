// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/switches.h"

#include "build/chromecast_buildflags.h"

namespace switches {

const char kEnableContentDirectories[] = "enable-content-directories";
const char kEnableWidevine[] = "enable-widevine";
const char kIncognito[] = "incognito";
const char kPlayreadyKeySystem[] = "playready-key-system";
const char kEnableRemoteDebugMode[] = "remote-debug-mode";
const char kUserAgentProductAndVersion[] = "user-agent-product";
const char kCorsExemptHeaders[] = "cors-exempt-headers";
const char kEnableCastStreamingReceiver[] = "enable-cast-streaming-receiver";
const char kCdmDataDirectory[] = "cdm-data-directory";
const char kCdmDataQuotaBytes[] = "cdm-data-quota-bytes";
const char kDataQuotaBytes[] = "data-quota-bytes";
const char kGoogleApiKey[] = "google-api-key";
const char kContextProvider[] = "context-provider";
const char kProxyBypassList[] = "proxy-bypass-list";
const char kProxyServer[] = "proxy-server";

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
const char kAllowRunningInsecureContent[] = "allow-running-insecure-content";
const char kUseLegacyMetricsService[] = "use-legacy-metrics-service";
#endif

}  // namespace switches
