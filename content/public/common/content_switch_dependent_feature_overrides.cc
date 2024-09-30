// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_switch_dependent_feature_overrides.h"

#include "components/attribution_reporting/features.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/switches.h"

namespace content {

std::vector<base::FeatureList::FeatureOverrideInfo>
GetSwitchDependentFeatureOverrides(const base::CommandLine& command_line) {
  // Describes a switch-dependent override.
  struct SwitchDependentFeatureOverrideInfo {
    // Switch that the override depends upon. The override will be registered if
    // this switch is present.
    const char* switch_name;
    // Feature to override.
    const std::reference_wrapper<const base::Feature> feature;
    // State to override the feature with.
    base::FeatureList::OverrideState override_state;
  } override_info[] = {
      // Overrides for --enable-experimental-web-platform-features.
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(net::features::kCookieSameSiteConsidersRedirectChain),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kDocumentPolicyNegotiation),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
#if BUILDFLAG(ENABLE_REPORTING)
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(net::features::kDocumentReporting),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
#endif  // BUILDFLAG(ENABLE_REPORTING)
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kExperimentalContentSecurityPolicyFeatures),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(blink::features::kDocumentPictureInPictureAPI),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kOriginIsolationHeader),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(
           blink::features::kDocumentPolicyIncludeJSCallStacksInCrashReports),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kEnableCanvas2DLayers),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(blink::features::kCreateImageBitmapOrientationNone),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kCriticalClientHint),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(net::features::kSchemefulSameSite),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kBlockInsecurePrivateNetworkRequests),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kBlockInsecurePrivateNetworkRequestsFromPrivate),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kBlockInsecurePrivateNetworkRequestsFromUnknown),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(features::kPrivateNetworkAccessRespectPreflightResults),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(net::features::kThirdPartyStoragePartitioning),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalWebPlatformFeatures,
       std::cref(blink::features::kPartitionedPopins),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Overrides for --enable-experimental-cookie-features.
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kCookieSameSiteConsidersRedirectChain),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kSameSiteDefaultChecksMethodRigorously),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kEnablePortBoundCookies),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnableExperimentalCookieFeatures,
       std::cref(net::features::kEnableSchemeBoundCookies),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Test behavior for third-party cookie phaseout.
      {network::switches::kTestThirdPartyCookiePhaseout,
       std::cref(net::features::kForceThirdPartyCookieBlocking),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {network::switches::kTestThirdPartyCookiePhaseout,
       std::cref(net::features::kThirdPartyStoragePartitioning),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Overrides for --isolation-by-default.
      {switches::kIsolationByDefault,
       std::cref(features::kEmbeddingRequiresOptIn),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kIsolationByDefault,
       std::cref(network::features::kCrossOriginOpenerPolicyByDefault),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Overrides for headless
      {::switches::kHeadless, std::cref(blink::features::kPaintHolding),
       base::FeatureList::OVERRIDE_DISABLE_FEATURE},

      // Override for --reduce-user-agent-minor-version.
      {switches::kReduceUserAgentMinorVersion,
       std::cref(blink::features::kReduceUserAgentMinorVersion),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Override for --reduce-user-agent-platform-oscpu.
      {switches::kReduceUserAgentPlatformOsCpu,
       std::cref(blink::features::kReduceUserAgentPlatformOsCpu),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Override for --reduce-accept-language.
      {switches::kReduceAcceptLanguage,
       std::cref(network::features::kReduceAcceptLanguage),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},

      // Override for --privacy-sandbox-ads-apis. See also chrome layer
      // overrides.
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(features::kPrivacySandboxAdsAPIsOverride),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kInterestGroupStorage),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kFledge),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kBiddingAndScoringDebugReportingAPI),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kAllowURNsInIframes),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kBrowsingTopics),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(attribution_reporting::features::kConversionMeasurement),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(network::features::kAttributionReportingCrossAppWeb),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kFencedFrames),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kSharedStorageAPI),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
      {switches::kEnablePrivacySandboxAdsApis,
       std::cref(blink::features::kPrivateAggregationApi),
       base::FeatureList::OVERRIDE_ENABLE_FEATURE},
  };

  std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
  for (const auto& info : override_info) {
    if (command_line.HasSwitch(info.switch_name))
      overrides.emplace_back(std::make_pair(info.feature, info.override_state));
  }
  return overrides;
}

}  // namespace content
