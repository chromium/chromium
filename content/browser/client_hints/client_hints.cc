// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {
uint8_t randomization_salt = 0;

constexpr size_t kMaxRandomNumbers = 21;

// Returns the randomization salt (weak and insecure) that should be used when
// adding noise to the network quality metrics. This is known only to the
// device, and is generated only once. This makes it possible to add the same
// amount of noise for a given origin.
uint8_t RandomizationSalt() {
  if (randomization_salt == 0)
    randomization_salt = base::RandInt(1, kMaxRandomNumbers);
  DCHECK_LE(1, randomization_salt);
  DCHECK_GE(kMaxRandomNumbers, randomization_salt);
  return randomization_salt;
}

double GetRandomMultiplier(const std::string& host) {
  // The random number should be a function of the hostname to reduce
  // cross-origin fingerprinting. The random number should also be a function
  // of randomized salt which is known only to the device. This prevents
  // origin from removing noise from the estimates.
  unsigned hash = std::hash<std::string>{}(host) + RandomizationSalt();
  double random_multiplier =
      0.9 + static_cast<double>((hash % kMaxRandomNumbers)) * 0.01;
  DCHECK_LE(0.90, random_multiplier);
  DCHECK_GE(1.10, random_multiplier);
  return random_multiplier;
}

unsigned long RoundRtt(const std::string& host,
                       const base::Optional<base::TimeDelta>& rtt) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityMsec = 50;
  static const double kMaxRttMsec = 3.0 * 1000;

  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  double rtt_msec = static_cast<double>(rtt.value().InMilliseconds());
  rtt_msec *= GetRandomMultiplier(host);
  rtt_msec = std::min(rtt_msec, kMaxRttMsec);

  DCHECK_LE(0, rtt_msec);
  DCHECK_GE(kMaxRttMsec, rtt_msec);

  // Round down to the nearest kBucketSize msec value.
  return std::round(rtt_msec / kGranularityMsec) * kGranularityMsec;
}

double RoundKbpsToMbps(const std::string& host,
                       const base::Optional<int32_t>& downlink_kbps) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityKbps = 50;
  static const double kMaxDownlinkKbps = 10.0 * 1000;

  // If downlink is unavailable, return the fastest value.
  double randomized_downlink_kbps = downlink_kbps.value_or(kMaxDownlinkKbps);
  randomized_downlink_kbps *= GetRandomMultiplier(host);

  randomized_downlink_kbps =
      std::min(randomized_downlink_kbps, kMaxDownlinkKbps);

  DCHECK_LE(0, randomized_downlink_kbps);
  DCHECK_GE(kMaxDownlinkKbps, randomized_downlink_kbps);
  // Round down to the nearest kGranularityKbps kbps value.
  double downlink_kbps_rounded =
      std::round(randomized_downlink_kbps / kGranularityKbps) *
      kGranularityKbps;

  // Convert from Kbps to Mbps.
  return downlink_kbps_rounded / 1000;
}

double GetDeviceScaleFactor() {
  double device_scale_factor = 1.0;
  if (display::Screen::GetScreen()) {
    device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  }
  DCHECK_LT(0.0, device_scale_factor);
  return device_scale_factor;
}

// Returns the zoom factor for a given |url|.
double GetZoomFactor(content::BrowserContext* context, const GURL& url) {
// Android does not have the concept of zooming in like desktop.
#if defined(OS_ANDROID)
  return 1.0;
#else

  double zoom_level = content::HostZoomMap::GetDefaultForBrowserContext(context)
                          ->GetZoomLevelForHostAndScheme(
                              url.scheme(), net::GetHostOrSpecFromURL(url));

  if (zoom_level == 0.0) {
    // Get default zoom level.
    zoom_level = content::HostZoomMap::GetDefaultForBrowserContext(context)
                     ->GetDefaultZoomLevel();
  }

  return blink::PageZoomLevelToZoomFactor(zoom_level);
#endif
}

// Returns a string corresponding to |value|. The returned string satisfies
// ABNF: 1*DIGIT [ "." 1*DIGIT ]
std::string DoubleToSpecCompliantString(double value) {
  DCHECK_LE(0.0, value);
  std::string result = base::NumberToString(value);
  DCHECK(!result.empty());
  if (value >= 1.0)
    return result;

  DCHECK_LE(0.0, value);
  DCHECK_GT(1.0, value);

  // Check if there is at least one character before period.
  if (result.at(0) != '.')
    return result;

  // '.' is the first character in |result|. Prefix one digit before the
  // period to make it spec compliant.
  return "0" + result;
}

// Return the effective connection type value overridden for web APIs.
// If no override value has been set, a null value is returned.
base::Optional<net::EffectiveConnectionType>
GetWebHoldbackEffectiveConnectionType() {
  if (!base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    return base::nullopt;
  }
  std::string effective_connection_type_param =
      base::GetFieldTrialParamValueByFeature(
          features::kNetworkQualityEstimatorWebHoldback,
          "web_effective_connection_type_override");

  base::Optional<net::EffectiveConnectionType> effective_connection_type =
      net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
  DCHECK(effective_connection_type_param.empty() || effective_connection_type);

  if (!effective_connection_type)
    return base::nullopt;
  DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type.value());
  return effective_connection_type;
}

void SetHeaderToDouble(net::HttpRequestHeaders* headers,
                       blink::mojom::WebClientHintsType client_hint_type,
                       double value) {
  headers->SetHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)],
      DoubleToSpecCompliantString(value));
}

void SetHeaderToInt(net::HttpRequestHeaders* headers,
                    blink::mojom::WebClientHintsType client_hint_type,
                    double value) {
  headers->SetHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)],
      base::NumberToString(std::round(value)));
}

void SetHeaderToString(net::HttpRequestHeaders* headers,
                       blink::mojom::WebClientHintsType client_hint_type,
                       std::string value) {
  headers->SetHeader(
      blink::kClientHintsHeaderMapping[static_cast<int>(client_hint_type)],
      value);
}

void AddDeviceMemoryHeader(net::HttpRequestHeaders* headers) {
  DCHECK(headers);
  blink::ApproximatedDeviceMemory::Initialize();
  const float device_memory =
      blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
  DCHECK_LT(0.0, device_memory);
  SetHeaderToDouble(headers, blink::mojom::WebClientHintsType::kDeviceMemory,
                    device_memory);
}

void AddDPRHeader(net::HttpRequestHeaders* headers,
                  content::BrowserContext* context,
                  const GURL& url) {
  DCHECK(headers);
  DCHECK(context);
  double device_scale_factor = GetDeviceScaleFactor();
  double zoom_factor = GetZoomFactor(context, url);
  SetHeaderToDouble(headers, blink::mojom::WebClientHintsType::kDpr,
                    device_scale_factor * zoom_factor);
}

void AddViewportWidthHeader(net::HttpRequestHeaders* headers,
                            content::BrowserContext* context,
                            const GURL& url) {
  DCHECK(headers);
  DCHECK(context);
  // The default value on Android. See
  // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/viewportAndroid.css.
  double viewport_width = 980;

#if !defined(OS_ANDROID)
  double device_scale_factor = GetDeviceScaleFactor();
  viewport_width = (display::Screen::GetScreen()
                        ->GetPrimaryDisplay()
                        .GetSizeInPixel()
                        .width()) /
                   GetZoomFactor(context, url) / device_scale_factor;
#endif  // !OS_ANDROID
  DCHECK_LT(0, viewport_width);
  // TODO(yoav): Find out why this 0 check is needed...
  if (viewport_width > 0) {
    SetHeaderToInt(headers, blink::mojom::WebClientHintsType::kViewportWidth,
                   viewport_width);
  }
}

void AddRttHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);

  base::Optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  base::TimeDelta http_rtt;
  if (web_holdback_ect.has_value()) {
    http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
        web_holdback_ect.value());
  } else if (network_quality_tracker) {
    http_rtt = network_quality_tracker->GetHttpRTT();
  } else {
    http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }
  SetHeaderToInt(headers, blink::mojom::WebClientHintsType::kRtt,
                 RoundRtt(url.host(), http_rtt));
}

void AddDownlinkHeader(net::HttpRequestHeaders* headers,
                       network::NetworkQualityTracker* network_quality_tracker,
                       const GURL& url) {
  DCHECK(headers);
  base::Optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  int32_t downlink_throughput_kbps;

  if (web_holdback_ect.has_value()) {
    downlink_throughput_kbps =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
            web_holdback_ect.value());
  } else if (network_quality_tracker) {
    downlink_throughput_kbps =
        network_quality_tracker->GetDownstreamThroughputKbps();
  } else {
    downlink_throughput_kbps =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  SetHeaderToDouble(headers, blink::mojom::WebClientHintsType::kDownlink,
                    RoundKbpsToMbps(url.host(), downlink_throughput_kbps));
}

void AddEctHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));

  base::Optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  int effective_connection_type;
  if (web_holdback_ect.has_value()) {
    effective_connection_type = web_holdback_ect.value();
  } else if (network_quality_tracker) {
    effective_connection_type =
        static_cast<int>(network_quality_tracker->GetEffectiveConnectionType());
  } else {
    effective_connection_type =
        static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  SetHeaderToString(
      headers, blink::mojom::WebClientHintsType::kEct,
      blink::kWebEffectiveConnectionTypeMapping[effective_connection_type]);
}

void AddLangHeader(net::HttpRequestHeaders* headers,
                   content::ClientHintsControllerDelegate* delegate) {
  SetHeaderToString(
      headers, blink::mojom::WebClientHintsType::kLang,
      blink::SerializeLangClientHint(delegate->GetAcceptLanguageString()));
}

bool IsValidURLForClientHints(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS() ||
      (url.SchemeIs(url::kHttpScheme) && !net::IsLocalhost(url)))
    return false;

  DCHECK(url.SchemeIs(url::kHttpsScheme) ||
         (url.SchemeIs(url::kHttpScheme) && net::IsLocalhost(url)));
  return true;
}

bool UserAgentClientHintEnabled() {
  return base::FeatureList::IsEnabled(features::kUserAgentClientHint) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

void AddUAHeader(net::HttpRequestHeaders* headers,
                 blink::mojom::WebClientHintsType type,
                 std::string value) {
  SetHeaderToString(headers, type, value);
}

bool IsFeaturePolicyForClientHintsEnabled() {
  return base::FeatureList::IsEnabled(features::kFeaturePolicyForClientHints) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableExperimentalWebPlatformFeatures);
}

bool ShouldAddClientHint(
    const blink::WebEnabledClientHints& main_frame_client_hints,
    blink::FeaturePolicy* feature_policy,
    const url::Origin& resource_origin,
    blink::mojom::WebClientHintsType type,
    blink::mojom::FeaturePolicyFeature feature) {
  if (!main_frame_client_hints.IsEnabled(type))
    return false;
  if (!IsFeaturePolicyForClientHintsEnabled())
    return true;
  return feature_policy &&
         feature_policy->IsFeatureEnabledForOrigin(feature, resource_origin);
}

}  // namespace

namespace content {

unsigned long RoundRttForTesting(const std::string& host,
                                 const base::Optional<base::TimeDelta>& rtt) {
  return RoundRtt(host, rtt);
}

double RoundKbpsToMbpsForTesting(const std::string& host,
                                 const base::Optional<int32_t>& downlink_kbps) {
  return RoundKbpsToMbps(host, downlink_kbps);
}

void AddNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    bool javascript_enabled,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode* frame_tree_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));
  DCHECK(context);
  RenderFrameHostImpl* main_frame =
      frame_tree_node->frame_tree()->GetMainFrame();

  if (!IsValidURLForClientHints(url))
    return;

  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  if (!delegate->IsJavaScriptAllowed(url) || !javascript_enabled)
    return;

  blink::WebEnabledClientHints web_client_hints;

  // If the current frame is the main frame, the URL wasn't committed yet, so in
  // order to get the main frame URL, we should use the provided URL instead.
  // Otherwise, the current frame is an iframe and the main frame URL was
  // committed, so we can safely get it from it.
  GURL main_frame_url =
      frame_tree_node->IsMainFrame() ? url : main_frame->GetLastCommittedURL();

  delegate->GetAllowedClientHintsFromSource(main_frame_url, &web_client_hints);

  url::Origin resource_origin = url::Origin::Create(url);
  blink::FeaturePolicy* feature_policy = main_frame->feature_policy();

  // Add Headers
  if (ShouldAddClientHint(
          web_client_hints, feature_policy, resource_origin,
          blink::mojom::WebClientHintsType::kDeviceMemory,
          blink::mojom::FeaturePolicyFeature::kClientHintDeviceMemory)) {
    AddDeviceMemoryHeader(headers);
  }
  if (ShouldAddClientHint(web_client_hints, feature_policy, resource_origin,
                          blink::mojom::WebClientHintsType::kDpr,
                          blink::mojom::FeaturePolicyFeature::kClientHintDPR)) {
    AddDPRHeader(headers, context, url);
  }
  if (ShouldAddClientHint(
          web_client_hints, feature_policy, resource_origin,
          blink::mojom::WebClientHintsType::kViewportWidth,
          blink::mojom::FeaturePolicyFeature::kClientHintViewportWidth)) {
    AddViewportWidthHeader(headers, context, url);
  }
  network::NetworkQualityTracker* network_quality_tracker =
      delegate->GetNetworkQualityTracker();
  if (ShouldAddClientHint(web_client_hints, feature_policy, resource_origin,
                          blink::mojom::WebClientHintsType::kRtt,
                          blink::mojom::FeaturePolicyFeature::kClientHintRTT)) {
    AddRttHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(
          web_client_hints, feature_policy, resource_origin,
          blink::mojom::WebClientHintsType::kDownlink,
          blink::mojom::FeaturePolicyFeature::kClientHintDownlink)) {
    AddDownlinkHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(web_client_hints, feature_policy, resource_origin,
                          blink::mojom::WebClientHintsType::kEct,
                          blink::mojom::FeaturePolicyFeature::kClientHintECT)) {
    AddEctHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(
          web_client_hints, feature_policy, resource_origin,
          blink::mojom::WebClientHintsType::kLang,
          blink::mojom::FeaturePolicyFeature::kClientHintLang)) {
    AddLangHeader(headers, delegate);
  }

  if (UserAgentClientHintEnabled()) {
    blink::UserAgentMetadata ua = delegate->GetUserAgentMetadata();

    // The `Sec-CH-UA` client hint is attached to all outgoing requests. The
    // opt-in controls the header's value, not its presence. This is
    // (intentionally) different than other client hints.
    //
    // https://tools.ietf.org/html/draft-west-ua-client-hints-00#section-2.4
    std::string version =
        web_client_hints.IsEnabled(blink::mojom::WebClientHintsType::kUA)
            ? ua.full_version
            : ua.major_version;
    AddUAHeader(headers, blink::mojom::WebClientHintsType::kUA,
                version.empty() ? ua.brand
                                : base::StringPrintf("%s %s", ua.brand.c_str(),
                                                     version.c_str()));

    if (ShouldAddClientHint(
            web_client_hints, feature_policy, resource_origin,
            blink::mojom::WebClientHintsType::kUAArch,
            blink::mojom::FeaturePolicyFeature::kClientHintUAArch)) {
      AddUAHeader(headers, blink::mojom::WebClientHintsType::kUAArch,
                  ua.architecture);
    }

    if (ShouldAddClientHint(
            web_client_hints, feature_policy, resource_origin,
            blink::mojom::WebClientHintsType::kUAPlatform,
            blink::mojom::FeaturePolicyFeature::kClientHintUAPlatform)) {
      AddUAHeader(headers, blink::mojom::WebClientHintsType::kUAPlatform,
                  ua.platform);
    }

    if (ShouldAddClientHint(
            web_client_hints, feature_policy, resource_origin,
            blink::mojom::WebClientHintsType::kUAModel,
            blink::mojom::FeaturePolicyFeature::kClientHintUAModel)) {
      AddUAHeader(headers, blink::mojom::WebClientHintsType::kUAModel,
                  ua.model);
    }
  }

  // Static assert that triggers if a new client hint header is added. If a
  // new client hint header is added, the following assertion should be updated.
  // If possible, logic should be added above so that the request headers for
  // the newly added client hint can be added to the request.
  static_assert(
      blink::mojom::WebClientHintsType::kUAModel ==
          blink::mojom::WebClientHintsType::kMaxValue,
      "Consider adding client hint request headers from the browser process");

  // TODO(crbug.com/735518): If the request is redirected, the client hint
  // headers stay attached to the redirected request. Consider removing/adding
  // the client hints headers if the request is redirected with a change in
  // scheme or a change in the origin.
}

}  // namespace content
