// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/origin.h"

namespace content {

namespace {
using ::network::mojom::WebClientHintsType;

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
                       const absl::optional<base::TimeDelta>& rtt) {
  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  // Limit the maximum reported value and the granularity to reduce
  // fingerprinting.
  constexpr base::TimeDelta kMaxRtt = base::Seconds(3);
  constexpr base::TimeDelta kGranularity = base::Milliseconds(50);

  const base::TimeDelta modified_rtt =
      std::min(rtt.value() * GetRandomMultiplier(host), kMaxRtt);
  DCHECK_GE(modified_rtt, base::TimeDelta());
  return modified_rtt.RoundToMultiple(kGranularity).InMilliseconds();
}

double RoundKbpsToMbps(const std::string& host,
                       const absl::optional<int32_t>& downlink_kbps) {
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
double GetZoomFactor(BrowserContext* context, const GURL& url) {
#if defined(OS_ANDROID)
  // On Android, use the default value when the AccessibilityPageZoom
  // feature is not enabled.
  if (!base::FeatureList::IsEnabled(features::kAccessibilityPageZoom))
    return 1.0;
#endif

  double zoom_level = HostZoomMap::GetDefaultForBrowserContext(context)
                          ->GetZoomLevelForHostAndScheme(
                              url.scheme(), net::GetHostOrSpecFromURL(url));

  if (zoom_level == 0.0) {
    // Get default zoom level.
    zoom_level = HostZoomMap::GetDefaultForBrowserContext(context)
                     ->GetDefaultZoomLevel();
  }

  return blink::PageZoomLevelToZoomFactor(zoom_level);
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
absl::optional<net::EffectiveConnectionType>
GetWebHoldbackEffectiveConnectionType() {
  if (!base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    return absl::nullopt;
  }
  std::string effective_connection_type_param =
      base::GetFieldTrialParamValueByFeature(
          features::kNetworkQualityEstimatorWebHoldback,
          "web_effective_connection_type_override");

  absl::optional<net::EffectiveConnectionType> effective_connection_type =
      net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
  DCHECK(effective_connection_type_param.empty() || effective_connection_type);

  if (!effective_connection_type)
    return absl::nullopt;
  DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type.value());
  return effective_connection_type;
}

void SetHeaderToDouble(net::HttpRequestHeaders* headers,
                       WebClientHintsType client_hint_type,
                       double value) {
  headers->SetHeader(network::GetClientHintToNameMap().at(client_hint_type),
                     DoubleToSpecCompliantString(value));
}

void SetHeaderToInt(net::HttpRequestHeaders* headers,
                    WebClientHintsType client_hint_type,
                    double value) {
  headers->SetHeader(network::GetClientHintToNameMap().at(client_hint_type),
                     base::NumberToString(std::round(value)));
}

void SetHeaderToString(net::HttpRequestHeaders* headers,
                       WebClientHintsType client_hint_type,
                       const std::string& value) {
  headers->SetHeader(network::GetClientHintToNameMap().at(client_hint_type),
                     value);
}

void RemoveClientHintHeader(WebClientHintsType client_hint_type,
                            net::HttpRequestHeaders* headers) {
  headers->RemoveHeader(network::GetClientHintToNameMap().at(client_hint_type));
}

void AddDeviceMemoryHeader(net::HttpRequestHeaders* headers,
                           bool use_deprecated_version = false) {
  DCHECK(headers);
  blink::ApproximatedDeviceMemory::Initialize();
  const float device_memory =
      blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
  DCHECK_LT(0.0, device_memory);
  SetHeaderToDouble(headers,
                    use_deprecated_version
                        ? WebClientHintsType::kDeviceMemory_DEPRECATED
                        : WebClientHintsType::kDeviceMemory,
                    device_memory);
}

void AddDPRHeader(net::HttpRequestHeaders* headers,
                  BrowserContext* context,
                  const GURL& url,
                  bool use_deprecated_version = false) {
  DCHECK(headers);
  DCHECK(context);
  double device_scale_factor = GetDeviceScaleFactor();
  double zoom_factor = GetZoomFactor(context, url);
  SetHeaderToDouble(headers,
                    use_deprecated_version ? WebClientHintsType::kDpr_DEPRECATED
                                           : WebClientHintsType::kDpr,
                    device_scale_factor * zoom_factor);
}

void AddViewportWidthHeader(net::HttpRequestHeaders* headers,
                            BrowserContext* context,
                            const GURL& url,
                            bool use_deprecated_version = false) {
  DCHECK(headers);
  DCHECK(context);
  // The default value on Android. See
  // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/viewportAndroid.css.
  double viewport_width = 980;

#if defined(OS_ANDROID)
  // On Android, use the default value when the AccessibilityPageZoom
  // feature is not enabled.
  if (!base::FeatureList::IsEnabled(features::kAccessibilityPageZoom)) {
    SetHeaderToInt(headers,
                   use_deprecated_version
                       ? WebClientHintsType::kViewportWidth_DEPRECATED
                       : WebClientHintsType::kViewportWidth,
                   viewport_width);
    return;
  }
#endif

  double device_scale_factor = GetDeviceScaleFactor();
  viewport_width = (display::Screen::GetScreen()
                        ->GetPrimaryDisplay()
                        .GetSizeInPixel()
                        .width()) /
                   GetZoomFactor(context, url) / device_scale_factor;
  DCHECK_LT(0, viewport_width);
  // TODO(yoav): Find out why this 0 check is needed...
  if (viewport_width > 0) {
    SetHeaderToInt(headers,
                   use_deprecated_version
                       ? WebClientHintsType::kViewportWidth_DEPRECATED
                       : WebClientHintsType::kViewportWidth,
                   viewport_width);
  }
}

void AddViewportHeightHeader(net::HttpRequestHeaders* headers,
                             BrowserContext* context,
                             const GURL& url) {
  DCHECK(headers);
  DCHECK(context);

  double overall_scale_factor =
      GetZoomFactor(context, url) * GetDeviceScaleFactor();
  double viewport_height = (display::Screen::GetScreen()
                                ->GetPrimaryDisplay()
                                .GetSizeInPixel()
                                .height()) /
                           overall_scale_factor;
#if defined(OS_ANDROID)
  // On Android, the viewport is scaled so the width is 980 and the height
  // maintains the same ratio.
  // TODO(1246208): Improve the usefulness of the viewport client hints for
  // navigation requests.
  double viewport_width = (display::Screen::GetScreen()
                               ->GetPrimaryDisplay()
                               .GetSizeInPixel()
                               .width()) /
                          overall_scale_factor;
  viewport_height *= 980.0 / viewport_width;
#endif  // OS_ANDROID

  DCHECK_LT(0, viewport_height);

  SetHeaderToInt(headers, network::mojom::WebClientHintsType::kViewportHeight,
                 viewport_height);
}

void AddRttHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);

  absl::optional<net::EffectiveConnectionType> web_holdback_ect =
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
  SetHeaderToInt(headers, WebClientHintsType::kRtt_DEPRECATED,
                 RoundRtt(url.host(), http_rtt));
}

void AddDownlinkHeader(net::HttpRequestHeaders* headers,
                       network::NetworkQualityTracker* network_quality_tracker,
                       const GURL& url) {
  DCHECK(headers);
  absl::optional<net::EffectiveConnectionType> web_holdback_ect =
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

  SetHeaderToDouble(headers, WebClientHintsType::kDownlink_DEPRECATED,
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

  absl::optional<net::EffectiveConnectionType> web_holdback_ect =
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
      headers, WebClientHintsType::kEct_DEPRECATED,
      blink::kWebEffectiveConnectionTypeMapping[effective_connection_type]);
}

void AddPrefersColorSchemeHeader(net::HttpRequestHeaders* headers,
                                 FrameTreeNode* frame_tree_node) {
  if (!frame_tree_node)
    return;
  blink::mojom::PreferredColorScheme preferred_color_scheme =
      frame_tree_node->current_frame_host()->GetPreferredColorScheme();
  bool is_dark_mode =
      preferred_color_scheme == blink::mojom::PreferredColorScheme::kDark;
  SetHeaderToString(headers, WebClientHintsType::kPrefersColorScheme,
                    is_dark_mode ? "dark" : "light");
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
  return base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint);
}

void AddUAHeader(net::HttpRequestHeaders* headers,
                 WebClientHintsType type,
                 const std::string& value) {
  SetHeaderToString(headers, type, value);
}

// Creates a serialized string header value out of the input type, using
// structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
template <typename T>
const std::string SerializeHeaderString(const T& value) {
  return net::structured_headers::SerializeItem(
             net::structured_headers::Item(value))
      .value_or(std::string());
}

bool IsPermissionsPolicyForClientHintsEnabled() {
  return base::FeatureList::IsEnabled(features::kFeaturePolicyForClientHints);
}

bool IsSameOrigin(const GURL& url1, const GURL& url2) {
  return url::Origin::Create(url1).IsSameOriginWith(url::Origin::Create(url2));
}

// Returns true iff the `url` is embedded inside a frame that has the
// Sec-CH-UA-Reduced client hint and thus, is enrolled in the
// UserAgentReduction Origin Trial.
//
// TODO(crbug.com/1258063): Remove when the UserAgentReduction Origin Trial is
// finished.
bool IsUserAgentReductionEnabledForEmbeddedFrame(
    const GURL& url,
    const GURL& main_frame_url,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate) {
  bool is_embedder_ua_reduced = false;
  RenderFrameHostImpl* current = frame_tree_node->current_frame_host();
  while (current) {
    const GURL& current_url = (current == frame_tree_node->current_frame_host())
                                  ? url
                                  : current->GetLastCommittedURL();

    // Don't use Sec-CH-UA-Reduced from third-party origins if third-party
    // cookies are blocked, so that we don't reveal any more user data than
    // is allowed by the cookie settings.
    if (IsSameOrigin(current_url, main_frame_url) ||
        !delegate->AreThirdPartyCookiesBlocked(current_url)) {
      blink::EnabledClientHints current_url_hints;
      delegate->GetAllowedClientHintsFromSource(current_url,
                                                &current_url_hints);
      if ((is_embedder_ua_reduced =
               base::Contains(current_url_hints.GetEnabledHints(),
                              WebClientHintsType::kUAReduced))) {
        break;
      }
    }

    current = current->GetParent();
  }
  return is_embedder_ua_reduced;
}

// TODO(crbug.com/1258063): Delete this function when the UserAgentReduction
// Origin Trial is finished.
void RemoveAllClientHintsExceptUaReduced(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    std::vector<WebClientHintsType>* accept_ch,
    GURL* main_frame_url,
    GURL const** third_party_url) {
  const url::Origin request_origin = url::Origin::Create(url);
  RenderFrameHostImpl* main_frame =
      frame_tree_node->frame_tree()->GetMainFrame();

  for (auto it = accept_ch->begin(); it != accept_ch->end();) {
    if (*it == WebClientHintsType::kUAReduced) {
      ++it;
    } else {
      it = accept_ch->erase(it);
    }
  }

  if (!request_origin.IsSameOriginWith(main_frame->GetLastCommittedOrigin())) {
    // If third-party cookeis are blocked, we will not persist the
    // Sec-CH-UA-Reduced client hint in a third-party context.
    if (delegate->AreThirdPartyCookiesBlocked(url)) {
      accept_ch->clear();
      return;
    }
    // Third-party contexts need the correct main frame URL and third-party
    // URL in order to validate the Origin Trial token correctly, if present.
    *main_frame_url = main_frame->GetLastCommittedURL();
    *third_party_url = &url;
  }
}

// Captures the state used in applying client hints.
struct ClientHintsExtendedData {
  ClientHintsExtendedData(const GURL& url,
                          FrameTreeNode* frame_tree_node,
                          ClientHintsControllerDelegate* delegate)
      : resource_origin(url::Origin::Create(url)) {
    // If the current frame is the main frame, the URL wasn't committed yet, so
    // in order to get the main frame URL, we should use the provided URL
    // instead. Otherwise, the current frame is an iframe and the main frame URL
    // was committed, so we can safely get it from it. Similarly, an
    // in-navigation main frame doesn't yet have a permissions policy.
    is_main_frame = !frame_tree_node || frame_tree_node->IsMainFrame();
    if (is_main_frame) {
      main_frame_url = url;
      is_1p_origin = true;
    } else {
      RenderFrameHostImpl* main_frame =
          frame_tree_node->frame_tree()->GetMainFrame();
      main_frame_url = main_frame->GetLastCommittedURL();
      permissions_policy = blink::PermissionsPolicy::CopyStateFrom(
          main_frame->permissions_policy());
      is_1p_origin = resource_origin.IsSameOriginWith(
          main_frame->GetLastCommittedOrigin());
    }

    delegate->GetAllowedClientHintsFromSource(main_frame_url, &hints);

    // If this is not a top-level frame, then check if any of the ancestors
    // in the path that led to this request have Sec-CH-UA-Reduced set.
    // TODO(crbug.com/1258063): Remove once the UserAgentReduction Origin Trial
    // is finished.
    if (frame_tree_node && !is_main_frame) {
      is_embedder_ua_reduced = IsUserAgentReductionEnabledForEmbeddedFrame(
          url, main_frame_url, frame_tree_node, delegate);
    }
  }

  blink::EnabledClientHints hints;
  // If true, one of the ancestor requests in the path to this request had
  // Sec-CH-UA-Reduced in their Accept-CH cache.  Only applies to embedded
  // requests (top-level requests will always set this to false).
  //
  // If an embedder of a request has Sec-CH-UA-Reduced, it means it will
  // receive the reduced User-Agent header, so we want to also send the reduced
  // User-Agent for the embedded request as well.
  bool is_embedder_ua_reduced = false;
  url::Origin resource_origin;
  bool is_main_frame = false;
  GURL main_frame_url;
  std::unique_ptr<blink::PermissionsPolicy> permissions_policy;
  bool is_1p_origin = false;
};

bool IsClientHintAllowed(const ClientHintsExtendedData& data,
                         WebClientHintsType type) {
  if (!IsPermissionsPolicyForClientHintsEnabled() || data.is_main_frame)
    return data.is_1p_origin;
  return (data.is_embedder_ua_reduced &&
          type == WebClientHintsType::kUAReduced) ||
         (data.permissions_policy &&
          data.permissions_policy->IsFeatureEnabledForOrigin(
              blink::GetClientHintToPolicyFeatureMap().at(type),
              data.resource_origin));
}

bool ShouldAddClientHint(const ClientHintsExtendedData& data,
                         WebClientHintsType type) {
  if (!blink::IsClientHintSentByDefault(type) && !data.hints.IsEnabled(type) &&
      !data.is_embedder_ua_reduced)
    return false;
  return IsClientHintAllowed(data, type);
}

bool IsJavascriptEnabled(FrameTreeNode* frame_tree_node) {
  return WebContents::FromRenderFrameHost(frame_tree_node->current_frame_host())
      ->GetOrCreateWebPreferences()
      .javascript_enabled;
}

// Captures when UpdateNavigationRequestClientUaHeadersImpl() is being called.
enum class ClientUaHeaderCallType {
  // The call is happening during creation of the NavigationRequest.
  kDuringCreation,

  // The call is happening after creation of the NavigationRequest.
  kAfterCreated,
};

// Implementation of UpdateNavigationRequestClientUaHeaders().
void UpdateNavigationRequestClientUaHeadersImpl(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    ClientUaHeaderCallType call_type,
    net::HttpRequestHeaders* headers) {
  absl::optional<blink::UserAgentMetadata> ua_metadata;
  bool disable_due_to_custom_ua = false;
  if (override_ua) {
    NavigatorDelegate* nav_delegate =
        frame_tree_node ? frame_tree_node->navigator().GetDelegate() : nullptr;
    ua_metadata =
        nav_delegate ? nav_delegate->GetUserAgentOverride().ua_metadata_override
                     : absl::nullopt;
    // If a custom UA override is set, but no value is provided for UA client
    // hints, disable them.
    disable_due_to_custom_ua = !ua_metadata.has_value();
  }

  if (frame_tree_node &&
      devtools_instrumentation::ApplyUserAgentMetadataOverrides(frame_tree_node,
                                                                &ua_metadata)) {
    // Likewise, if devtools says to override client hints but provides no
    // value, disable them. This overwrites previous decision from UI.
    disable_due_to_custom_ua = !ua_metadata.has_value();
  }

  if (!disable_due_to_custom_ua) {
    if (!ua_metadata.has_value())
      ua_metadata = delegate->GetUserAgentMetadata();

    ClientHintsExtendedData data(url, frame_tree_node, delegate);

    // The `Sec-CH-UA` client hint is attached to all outgoing requests. This is
    // (intentionally) different than other client hints.
    // It's barred behind ShouldAddClientHints to make sure it's controlled by
    // Permissions Policy.
    //
    // https://wicg.github.io/client-hints-infrastructure/#abstract-opdef-append-client-hints-to-request
    if (ShouldAddClientHint(data, WebClientHintsType::kUA)) {
      AddUAHeader(headers, WebClientHintsType::kUA,
                  ua_metadata->SerializeBrandVersionList());
    }
    // The `Sec-CH-UA-Mobile client hint was also deemed "low entropy" and can
    // safely be sent with every request. Similarly to UA, ShouldAddClientHints
    // makes sure it's controlled by Permissions Policy.
    if (ShouldAddClientHint(data, WebClientHintsType::kUAMobile)) {
      AddUAHeader(headers, WebClientHintsType::kUAMobile,
                  SerializeHeaderString(ua_metadata->mobile));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAFullVersion)) {
      AddUAHeader(headers, WebClientHintsType::kUAFullVersion,
                  SerializeHeaderString(ua_metadata->full_version));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAArch)) {
      AddUAHeader(headers, WebClientHintsType::kUAArch,
                  SerializeHeaderString(ua_metadata->architecture));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAPlatform)) {
      AddUAHeader(headers, WebClientHintsType::kUAPlatform,
                  SerializeHeaderString(ua_metadata->platform));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAPlatformVersion)) {
      AddUAHeader(headers, WebClientHintsType::kUAPlatformVersion,
                  SerializeHeaderString(ua_metadata->platform_version));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAModel)) {
      AddUAHeader(headers, WebClientHintsType::kUAModel,
                  SerializeHeaderString(ua_metadata->model));
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kUABitness)) {
      AddUAHeader(headers, WebClientHintsType::kUABitness,
                  SerializeHeaderString(ua_metadata->bitness));
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kUAReduced)) {
      AddUAHeader(headers, WebClientHintsType::kUAReduced,
                  SerializeHeaderString(true));
    }
  } else if (call_type == ClientUaHeaderCallType::kAfterCreated) {
    RemoveClientHintHeader(WebClientHintsType::kUA, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAMobile, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAFullVersion, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAArch, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAPlatform, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAPlatformVersion, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAModel, headers);
    RemoveClientHintHeader(WebClientHintsType::kUABitness, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAReduced, headers);
  }
}

}  // namespace

bool ShouldAddClientHints(const GURL& url,
                          FrameTreeNode* frame_tree_node,
                          ClientHintsControllerDelegate* delegate) {
  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  return IsValidURLForClientHints(url) && delegate->IsJavaScriptAllowed(url) &&
         (!frame_tree_node || IsJavascriptEnabled(frame_tree_node));
}

unsigned long RoundRttForTesting(const std::string& host,
                                 const absl::optional<base::TimeDelta>& rtt) {
  return RoundRtt(host, rtt);
}

double RoundKbpsToMbpsForTesting(const std::string& host,
                                 const absl::optional<int32_t>& downlink_kbps) {
  return RoundKbpsToMbps(host, downlink_kbps);
}

void UpdateNavigationRequestClientUaHeaders(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers) {
  DCHECK(frame_tree_node);
  if (!UserAgentClientHintEnabled() ||
      !ShouldAddClientHints(url, frame_tree_node, delegate)) {
    return;
  }

  UpdateNavigationRequestClientUaHeadersImpl(
      url, delegate, override_ua, frame_tree_node,
      ClientUaHeaderCallType::kAfterCreated, headers);
}

namespace {

void AddRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode* frame_tree_node,
    const blink::ParsedPermissionsPolicy& container_policy) {
  ClientHintsExtendedData data(url, frame_tree_node, delegate);

  // If there is a container policy, use the same logic as when a new frame is
  // committed to combine with the parent policy.
  if (!container_policy.empty()) {
    data.permissions_policy = blink::PermissionsPolicy::CreateFromParentPolicy(
        data.permissions_policy.get(), container_policy,
        url::Origin::Create(url));
  }

  // Add Headers
  if (ShouldAddClientHint(data, WebClientHintsType::kDeviceMemory_DEPRECATED)) {
    AddDeviceMemoryHeader(headers, /*use_deprecated_version*/ true);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDeviceMemory)) {
    AddDeviceMemoryHeader(headers);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDpr_DEPRECATED)) {
    AddDPRHeader(headers, context, url, /*use_deprecated_version*/ true);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDpr)) {
    AddDPRHeader(headers, context, url);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kViewportWidth_DEPRECATED)) {
    AddViewportWidthHeader(headers, context, url,
                           /*use_deprecated_version*/ true);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kViewportWidth)) {
    AddViewportWidthHeader(headers, context, url);
  }
  if (ShouldAddClientHint(
          data, network::mojom::WebClientHintsType::kViewportHeight)) {
    AddViewportHeightHeader(headers, context, url);
  }
  network::NetworkQualityTracker* network_quality_tracker =
      delegate->GetNetworkQualityTracker();
  if (ShouldAddClientHint(data, WebClientHintsType::kRtt_DEPRECATED)) {
    AddRttHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDownlink_DEPRECATED)) {
    AddDownlinkHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kEct_DEPRECATED)) {
    AddEctHeader(headers, network_quality_tracker, url);
  }

  if (UserAgentClientHintEnabled()) {
    UpdateNavigationRequestClientUaHeadersImpl(
        url, delegate, is_ua_override_on, frame_tree_node,
        ClientUaHeaderCallType::kDuringCreation, headers);
  }

  if (ShouldAddClientHint(data, WebClientHintsType::kPrefersColorScheme)) {
    AddPrefersColorSchemeHeader(headers, frame_tree_node);
  }

  // Static assert that triggers if a new client hint header is added. If a
  // new client hint header is added, the following assertion should be updated.
  // If possible, logic should be added above so that the request headers for
  // the newly added client hint can be added to the request.
  static_assert(
      network::mojom::WebClientHintsType::kViewportWidth ==
          network::mojom::WebClientHintsType::kMaxValue,
      "Consider adding client hint request headers from the browser process");

  // TODO(crbug.com/735518): If the request is redirected, the client hint
  // headers stay attached to the redirected request. Consider removing/adding
  // the client hints headers if the request is redirected with a change in
  // scheme or a change in the origin.
}

}  // namespace

void AddPrefetchNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    bool is_javascript_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));
  DCHECK(context);

  // Since prefetch navigation doesn't have a related frame tree node,
  // |is_javascript_enabled| is passed in to get whether a typical frame tree
  // node would support javascript.
  if (!is_javascript_enabled || !ShouldAddClientHints(url, nullptr, delegate)) {
    return;
  }

  AddRequestClientHintsHeaders(url, headers, context, delegate,
                               is_ua_override_on, nullptr, {});
}

void AddNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode* frame_tree_node,
    const blink::ParsedPermissionsPolicy& container_policy) {
  DCHECK(frame_tree_node);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));
  DCHECK(context);

  if (!ShouldAddClientHints(url, frame_tree_node, delegate)) {
    return;
  }

  AddRequestClientHintsHeaders(url, headers, context, delegate,
                               is_ua_override_on, frame_tree_node,
                               container_policy);
}

absl::optional<std::vector<WebClientHintsType>>
ParseAndPersistAcceptCHForNavigation(
    const GURL& url,
    const network::mojom::ParsedHeadersPtr& parsed_headers,
    const net::HttpResponseHeaders* response_headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode* frame_tree_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);
  DCHECK(parsed_headers);

  if (!parsed_headers->accept_ch)
    return absl::nullopt;

  if (!IsValidURLForClientHints(url))
    return absl::nullopt;

  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  if (!delegate->IsJavaScriptAllowed(url) ||
      !IsJavascriptEnabled(frame_tree_node)) {
    return absl::nullopt;
  }

  std::vector<WebClientHintsType> accept_ch = parsed_headers->accept_ch.value();
  GURL main_frame_url = url;
  GURL const* third_party_url = nullptr;
  // Only the main frame should parse accept-CH, except for the temporary
  // Sec-CH-UA-Reduced client hint (used for the User-Agent reduction origin
  // trial).
  //
  // Note that if Sec-CH-UA-Reduced is persisted for an embedded frame, it
  // means a subsequent top-level navigation will read Sec-CH-UA-Reduced from
  // the Accept-CH cache and send a reduced User-Agent string.
  //
  // TODO(crbug.com/1258063): Delete this call when the UserAgentReduction
  // Origin Trial is finished.
  if (!frame_tree_node->IsMainFrame()) {
    RemoveAllClientHintsExceptUaReduced(url, frame_tree_node, delegate,
                                        &accept_ch, &main_frame_url,
                                        &third_party_url);
    if (accept_ch.empty()) {
      // There are is no Sec-CH-UA-Reduced in Accept-CH for the embedded frame,
      // so nothing should be persisted.
      return absl::nullopt;
    }
  }

  blink::EnabledClientHints enabled_hints;
  for (const WebClientHintsType type : accept_ch) {
    enabled_hints.SetIsEnabled(main_frame_url, third_party_url,
                               response_headers, type, true);
  }

  const std::vector<WebClientHintsType> persisted_hints =
      enabled_hints.GetEnabledHints();
  PersistAcceptCH(url, delegate, persisted_hints,
                  &parsed_headers->accept_ch_lifetime);
  return persisted_hints;
}

void PersistAcceptCH(const GURL& url,
                     ClientHintsControllerDelegate* delegate,
                     const std::vector<WebClientHintsType>& hints,
                     base::TimeDelta* persist_duration) {
  DCHECK(delegate);

  // TODO(https://crbug.com/1243060): Remove the checking and persistence of the
  // expiration time.
  const bool use_persist_duration =
      persist_duration && !IsPermissionsPolicyForClientHintsEnabled();

  if (use_persist_duration && persist_duration->is_zero())
    return;

  // JSON cannot store "non-finite" values (i.e. NaN or infinite) so
  // base::TimeDelta::Max cannot be used. As this will be removed once the
  // FeaturePolicyForClientHints feature is shipped, a reasonably large value
  // was chosen instead.
  base::TimeDelta duration =
      use_persist_duration ? *persist_duration : base::Days(1000000);

  delegate->PersistClientHints(url::Origin::Create(url), hints,
                               std::move(duration));
}

CONTENT_EXPORT std::vector<WebClientHintsType> LookupAcceptCHForCommit(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode* frame_tree_node) {
  std::vector<WebClientHintsType> result;
  if (!ShouldAddClientHints(url, frame_tree_node, delegate)) {
    return result;
  }

  const ClientHintsExtendedData data(url, frame_tree_node, delegate);
  std::vector<WebClientHintsType> hints = data.hints.GetEnabledHints();
  if (data.is_embedder_ua_reduced &&
      !base::Contains(hints, WebClientHintsType::kUAReduced)) {
    hints.push_back(WebClientHintsType::kUAReduced);
  }
  return hints;
}

bool AreCriticalHintsMissing(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    const std::vector<WebClientHintsType>& critical_hints) {
  ClientHintsExtendedData data(url, frame_tree_node, delegate);

  // Note: these only check for per-hint origin/permissions policy settings, not
  // origin-level or "browser-level" policies like disabiling JS or other
  // features.
  for (auto hint : critical_hints) {
    if (IsClientHintAllowed(data, hint) && !ShouldAddClientHint(data, hint))
      return true;
  }

  return false;
}

}  // namespace content
