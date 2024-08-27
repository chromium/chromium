// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/fetch_utils.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-blink.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Must remain in sync with FetchKeepAliveRequestMetricType in
// tools/metrics/histograms/enums.xml.
// LINT.IfChange
enum class FetchKeepAliveRequestMetricType {
  kFetch = 0,
  kBeacon = 1,
  kPing = 2,
  kReporting = 3,
  kAttribution = 4,
  kBackgroundFetchIcon = 5,
  kMaxValue = kBackgroundFetchIcon,
};
// LINT.ThenChange(//content/browser/loader/keep_alive_url_loader.h)

bool IsHTTPWhitespace(UChar chr) {
  return chr == ' ' || chr == '\n' || chr == '\t' || chr == '\r';
}

}  // namespace

bool FetchUtils::IsForbiddenMethod(const String& method) {
  DCHECK(IsValidHTTPToken(method));
  return network::cors::IsForbiddenMethod(method.Latin1());
}

bool FetchUtils::IsForbiddenResponseHeaderName(const String& name) {
  // http://fetch.spec.whatwg.org/#forbidden-response-header-name
  // "A forbidden response header name is a header name that is one of:
  // `Set-Cookie`, `Set-Cookie2`"

  return EqualIgnoringASCIICase(name, "set-cookie") ||
         EqualIgnoringASCIICase(name, "set-cookie2");
}

AtomicString FetchUtils::NormalizeMethod(const AtomicString& method) {
  // https://fetch.spec.whatwg.org/#concept-method-normalize

  // We place GET and POST first because they are more commonly used than
  // others.
  const AtomicString* kMethods[] = {
      &http_names::kGET,  &http_names::kPOST,    &http_names::kDELETE,
      &http_names::kHEAD, &http_names::kOPTIONS, &http_names::kPUT,
  };

  for (auto* const known : kMethods) {
    if (EqualIgnoringASCIICase(method, *known)) {
      // Don't bother allocating a new string if it's already all
      // uppercase.
      return method == *known ? method : *known;
    }
  }
  return method;
}

String FetchUtils::NormalizeHeaderValue(const String& value) {
  // https://fetch.spec.whatwg.org/#concept-header-value-normalize
  // Strip leading and trailing whitespace from header value.
  // HTTP whitespace bytes are 0x09, 0x0A, 0x0D, and 0x20.

  return value.StripWhiteSpace(IsHTTPWhitespace);
}

// static
// We have this function at the bottom of this file because it confuses
// syntax highliting.
// TODO(kinuko): Deprecate this, we basically need to know the destination
// and if it's for favicon or not.
net::NetworkTrafficAnnotationTag FetchUtils::GetTrafficAnnotationTag(
    const network::ResourceRequest& request) {
  if (request.is_favicon) {
    return net::DefineNetworkTrafficAnnotation("favicon_loader", R"(
      semantics {
        sender: "Blink Resource Loader"
        description:
          "Chrome sends a request to download favicon for a URL."
        trigger:
          "Navigating to a URL."
        data: "None."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented."
      })");
  }
  switch (request.destination) {
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];

    case network::mojom::RequestDestination::kEmpty:
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kFont:
    case network::mojom::RequestDestination::kImage:
    case network::mojom::RequestDestination::kJson:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kReport:
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kSpeculationRules:
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
    case network::mojom::RequestDestination::kWebBundle:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kXslt:
    case network::mojom::RequestDestination::kDictionary:
      return net::DefineNetworkTrafficAnnotation("blink_resource_loader", R"(
      semantics {
        sender: "Blink Resource Loader"
        description:
          "Blink-initiated request, which includes all resources for "
          "normal page loads, chrome URLs, and downloads."
        trigger:
          "The user navigates to a URL or downloads a file. Also when a "
          "webpage, ServiceWorker, or chrome:// uses any network communication."
        data: "Anything the initiator wants to send."
        destination: OTHER
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "These requests cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented. Without these requests, Chrome will be unable "
          "to load any webpage."
      })");

    case network::mojom::RequestDestination::kEmbed:
    case network::mojom::RequestDestination::kObject:
      return net::DefineNetworkTrafficAnnotation(
          "blink_extension_resource_loader", R"(
        semantics {
          sender: "Blink Resource Loader"
          description:
            "Blink-initiated request for resources required for NaCl instances "
            "tagged with <embed> or <object>, or installed extensions."
          trigger:
            "An extension or NaCl instance may initiate a request at any time, "
            "even in the background."
          data: "Anything the initiator wants to send."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "These requests cannot be disabled in settings, but they are "
            "sent only if user installs extensions."
          chrome_policy {
            ExtensionInstallBlocklist {
              ExtensionInstallBlocklist: {
                entries: '*'
              }
            }
          }
        })");
  }

  return net::NetworkTrafficAnnotationTag::NotReached();
}

// static
void FetchUtils::LogFetchKeepAliveRequestMetric(
    const mojom::blink::RequestContextType& request_context_type,
    const FetchKeepAliveRequestState& request_state,
    bool is_context_detached) {
  FetchKeepAliveRequestMetricType sample_type;
  switch (request_context_type) {
    case mojom::blink::RequestContextType::FETCH:
      sample_type = FetchKeepAliveRequestMetricType::kFetch;
      break;
    case mojom::blink::RequestContextType::BEACON:
      sample_type = FetchKeepAliveRequestMetricType::kBeacon;
      break;
    case mojom::blink::RequestContextType::PING:
      sample_type = FetchKeepAliveRequestMetricType::kPing;
      break;
    case mojom::blink::RequestContextType::CSP_REPORT:
      sample_type = FetchKeepAliveRequestMetricType::kReporting;
      break;
    case mojom::blink::RequestContextType::ATTRIBUTION_SRC:
      sample_type = FetchKeepAliveRequestMetricType::kAttribution;
      break;
    case mojom::blink::RequestContextType::IMAGE:
      sample_type = FetchKeepAliveRequestMetricType::kBackgroundFetchIcon;
      break;
    case mojom::blink::RequestContextType::UNSPECIFIED:
    case mojom::blink::RequestContextType::AUDIO:
    case mojom::blink::RequestContextType::DOWNLOAD:
    case mojom::blink::RequestContextType::EMBED:
    case mojom::blink::RequestContextType::EVENT_SOURCE:
    case mojom::blink::RequestContextType::FAVICON:
    case mojom::blink::RequestContextType::FONT:
    case mojom::blink::RequestContextType::FORM:
    case mojom::blink::RequestContextType::FRAME:
    case mojom::blink::RequestContextType::HYPERLINK:
    case mojom::blink::RequestContextType::IFRAME:
    case mojom::blink::RequestContextType::IMAGE_SET:
    case mojom::blink::RequestContextType::INTERNAL:
    case mojom::blink::RequestContextType::JSON:
    case mojom::blink::RequestContextType::LOCATION:
    case mojom::blink::RequestContextType::MANIFEST:
    case mojom::blink::RequestContextType::OBJECT:
    case mojom::blink::RequestContextType::PLUGIN:
    case mojom::blink::RequestContextType::PREFETCH:
    case mojom::blink::RequestContextType::SCRIPT:
    case mojom::blink::RequestContextType::SERVICE_WORKER:
    case mojom::blink::RequestContextType::SHARED_WORKER:
    case mojom::blink::RequestContextType::SPECULATION_RULES:
    case mojom::blink::RequestContextType::SUBRESOURCE:
    case mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE:
    case mojom::blink::RequestContextType::STYLE:
    case mojom::blink::RequestContextType::TRACK:
    case mojom::blink::RequestContextType::VIDEO:
    case mojom::blink::RequestContextType::WORKER:
    case mojom::blink::RequestContextType::XML_HTTP_REQUEST:
    case mojom::blink::RequestContextType::XSLT:
      NOTREACHED();
  }

  std::string_view request_state_name;
  switch (request_state) {
    case FetchKeepAliveRequestState::kTotal:
      request_state_name = "Total";
      break;
    case FetchKeepAliveRequestState::kStarted:
      request_state_name = "Started";
      base::UmaHistogramBoolean(
          "FetchKeepAlive.Requests2.Started.IsContextDetached.Renderer",
          is_context_detached);
      break;
    case FetchKeepAliveRequestState::kSucceeded:
      request_state_name = "Succeeded";
      base::UmaHistogramBoolean(
          "FetchKeepAlive.Requests2.Succeeded.IsContextDetached.Renderer",
          is_context_detached);
      break;
    case FetchKeepAliveRequestState::kFailed:
      request_state_name = "Failed";
      break;
  }
  CHECK(!request_state_name.empty());

  base::UmaHistogramEnumeration(base::StrCat({"FetchKeepAlive.Requests2.",
                                              request_state_name, ".Renderer"}),
                                sample_type);
}

void FetchUtils::LogFetchKeepAliveRequestSentToServiceMetric(
    const network::ResourceRequest& resource_request) {
  auto resource_type =
      static_cast<mojom::blink::ResourceType>(resource_request.resource_type);
  FetchKeepAliveRequestMetricType sample_type;
  // See also blink::UpgradeResourceRequestForLoader().
  switch (resource_type) {
    case mojom::blink::ResourceType::kXhr:
      sample_type = FetchKeepAliveRequestMetricType::kFetch;
      break;
    // Includes BEACON/PING/ATTRIBUTION_SRC types
    case mojom::blink::ResourceType::kPing:
      sample_type = FetchKeepAliveRequestMetricType::kPing;
      break;
    case mojom::blink::ResourceType::kCspReport:
      sample_type = FetchKeepAliveRequestMetricType::kReporting;
      break;
    case mojom::blink::ResourceType::kImage:
      sample_type = FetchKeepAliveRequestMetricType::kBackgroundFetchIcon;
      break;
    case mojom::blink::ResourceType::kMainFrame:
    case mojom::blink::ResourceType::kSubFrame:
    case mojom::blink::ResourceType::kStylesheet:
    case mojom::blink::ResourceType::kScript:
    case mojom::blink::ResourceType::kFontResource:
    case mojom::blink::ResourceType::kSubResource:
    case mojom::blink::ResourceType::kObject:
    case mojom::blink::ResourceType::kMedia:
    case mojom::blink::ResourceType::kWorker:
    case mojom::blink::ResourceType::kSharedWorker:
    case mojom::blink::ResourceType::kPrefetch:
    case mojom::blink::ResourceType::kFavicon:
    case mojom::blink::ResourceType::kServiceWorker:
    case mojom::blink::ResourceType::kPluginResource:
    case mojom::blink::ResourceType::kNavigationPreloadMainFrame:
    case mojom::blink::ResourceType::kNavigationPreloadSubFrame:
    case mojom::blink::ResourceType::kJson:
      NOTREACHED();
  }

  base::UmaHistogramEnumeration(
      "FetchKeepAlive.Requests2.SentToService.Renderer", sample_type);
}

}  // namespace blink
