// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/web_request/web_request_resource_type.h"

#include <string_view>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace extensions {

namespace {

constexpr struct {
  const char* const name;
  const WebRequestResourceType type;
} kResourceTypes[] = {
    {"main_frame", WebRequestResourceType::MAIN_FRAME},
    {"sub_frame", WebRequestResourceType::SUB_FRAME},
    {"stylesheet", WebRequestResourceType::STYLESHEET},
    {"script", WebRequestResourceType::SCRIPT},
    {"image", WebRequestResourceType::IMAGE},
    {"font", WebRequestResourceType::FONT},
    {"object", WebRequestResourceType::OBJECT},
    {"xmlhttprequest", WebRequestResourceType::XHR},
    {"ping", WebRequestResourceType::PING},
    {"csp_report", WebRequestResourceType::CSP_REPORT},
    {"media", WebRequestResourceType::MEDIA},
    {"websocket", WebRequestResourceType::WEB_SOCKET},
    {"webtransport", WebRequestResourceType::WEB_TRANSPORT},
    {"webbundle", WebRequestResourceType::WEBBUNDLE},
    {"other", WebRequestResourceType::OTHER},
};

constexpr size_t kResourceTypesLength = std::size(kResourceTypes);

static_assert(kResourceTypesLength ==
                  base::strict_cast<size_t>(WebRequestResourceType::OTHER) + 1,
              "Each WebRequestResourceType should have a string name.");

}  // namespace

WebRequestResourceType ToWebRequestResourceType(
    const network::ResourceRequest& request,
    bool is_download) {
  if (request.url.SchemeIsWSOrWSS()) {
    return WebRequestResourceType::WEB_SOCKET;
  }
  if (is_download) {
    return WebRequestResourceType::OTHER;
  }
  if (request.is_fetch_like_api) {
    // This must be checked before `request.keepalive` check below, because
    // currently Fetch keepAlive is not reported as ping.
    // See https://crbug.com/611453 for more details.
    return WebRequestResourceType::XHR;
  }

  switch (request.destination) {
    case network::mojom::RequestDestination::kDocument:
      return WebRequestResourceType::MAIN_FRAME;
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
      return WebRequestResourceType::SUB_FRAME;
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kXslt:
      return WebRequestResourceType::STYLESHEET;
    case network::mojom::RequestDestination::kScript:
    // TODO(crbug.com/41484304): Consider adding a new
    // webRequest.ResourceType for JSON requests modules.
    case network::mojom::RequestDestination::kJson:
      return WebRequestResourceType::SCRIPT;
    case network::mojom::RequestDestination::kImage:
      return WebRequestResourceType::IMAGE;
    case network::mojom::RequestDestination::kFont:
      return WebRequestResourceType::FONT;
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kEmbed:
      return WebRequestResourceType::OBJECT;
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
      return WebRequestResourceType::MEDIA;
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
      return WebRequestResourceType::SCRIPT;
    case network::mojom::RequestDestination::kReport:
      return WebRequestResourceType::CSP_REPORT;
    case network::mojom::RequestDestination::kEmpty:
      // https://fetch.spec.whatwg.org/#concept-request-destination
      if (request.keepalive) {
        return WebRequestResourceType::PING;
      }
      return WebRequestResourceType::OTHER;
    case network::mojom::RequestDestination::kWebBundle:
      return WebRequestResourceType::WEBBUNDLE;
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kWebIdentity:
    // The compression dictionary has not been exposed to extensions yet.
    // We could do so if the need arises.
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kSpeculationRules:
      return WebRequestResourceType::OTHER;
  }
  NOTREACHED_IN_MIGRATION();
  return WebRequestResourceType::OTHER;
}

const char* WebRequestResourceTypeToString(WebRequestResourceType type) {
  size_t index = base::strict_cast<size_t>(type);
  DCHECK_LT(index, kResourceTypesLength);
  DCHECK_EQ(kResourceTypes[index].type, type);
  return kResourceTypes[index].name;
}

bool ParseWebRequestResourceType(std::string_view text,
                                 WebRequestResourceType* type) {
  for (size_t i = 0; i < kResourceTypesLength; ++i) {
    if (text == kResourceTypes[i].name) {
      *type = kResourceTypes[i].type;
      DCHECK_EQ(static_cast<WebRequestResourceType>(i), *type);
      return true;
    }
  }
  return false;
}

}  // namespace extensions
