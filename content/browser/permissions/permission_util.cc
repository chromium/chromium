// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_util.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

using blink::mojom::PermissionDescriptorPtr;

namespace content {

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr const char* kIsFileURLHistogram =
    "Permissions.GetLastCommittedOriginAsURL.IsFileURL";
#endif

// Returns true if the opaque-origin document in `render_frame_host` may use the
// permissions of its precursor origin. See https://crbug.com/576129749.
//
// Due to dependency issues, this function is duplicated from
// components/permissions/permission_util.cc.
bool ShouldUsePrecursorOrigin(RenderFrameHost* render_frame_host) {
  const url::SchemeHostPort& precursor =
      render_frame_host->GetLastCommittedOrigin()
          .GetTupleOrPrecursorTupleIfOpaque();
  return precursor.IsValid() &&
         precursor ==
             url::SchemeHostPort(render_frame_host->GetLastCommittedURL()) &&
         render_frame_host->IsSandboxed(
             network::mojom::WebSandboxFlags::kOrigin);
}

}  // namespace

PermissionOption PermissionUtil::ToPermissionOption(
    blink::mojom::PermissionStatus permission_status) {
  switch (permission_status) {
    case blink::mojom::PermissionStatus::GRANTED:
      return PermissionOption::kAllowed;
    case blink::mojom::PermissionStatus::DENIED:
      return PermissionOption::kDenied;
    case blink::mojom::PermissionStatus::ASK:
      return PermissionOption::kAsk;
  }
  NOTREACHED();
}

blink::mojom::PermissionStatusWithDetailsPtr
PermissionUtil::ToPermissionStatusWithDetails(
    blink::mojom::PermissionName permission_name,
    PermissionResult result) {
  blink::mojom::PermissionStatus status = result.status;
  blink::mojom::PermissionDetailsPtr details;

  if (permission_name == blink::mojom::PermissionName::GEOLOCATION) {
    GeolocationSetting* geolocation_setting =
        result.retrieved_permission_setting
            ? std::get_if<GeolocationSetting>(
                  &result.retrieved_permission_setting.value())
            : nullptr;
    if (status == blink::mojom::PermissionStatus::GRANTED &&
        geolocation_setting) {
      details = blink::mojom::PermissionDetails::NewGeolocationAccuracy(
          geolocation_setting->precise == PermissionOption::kAllowed
              ? blink::mojom::GeolocationAccuracy::kPrecise
              : blink::mojom::GeolocationAccuracy::kApproximate);
    }
  }
  return blink::mojom::PermissionStatusWithDetails::New(status,
                                                        std::move(details));
}

// Due to dependency issues, this method is duplicated from
// components/permissions/permission_util.cc.
GURL PermissionUtil::GetLastCommittedOriginAsURL(
    content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);

#if BUILDFLAG(IS_ANDROID)
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // If `allow_universal_access_from_file_urls` flag is enabled, a file:/// can
  // change its url via history.pushState/replaceState to any other url,
  // including about:blank. To avoid user confusion we should always use a
  // visible url, in other words `GetLastCommittedURL`.
  if (web_contents->GetOrCreateWebPreferences()
          .allow_universal_access_from_file_urls &&
      render_frame_host->GetLastCommittedOrigin().GetURL().SchemeIsFile()) {
    base::UmaHistogramBoolean(kIsFileURLHistogram, true);
    return render_frame_host->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  } else {
    base::UmaHistogramBoolean(kIsFileURLHistogram, false);
  }
#endif

  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  // An opaque origin has no permissions of its own. For historical reasons,
  // some top-level documents with an opaque origin may use the permissions of
  // their precursor origin instead.
  // TODO(https://crbug.com/571135408): Revisit this logic.
  if (origin.opaque() && render_frame_host->IsInPrimaryMainFrame() &&
      ShouldUsePrecursorOrigin(render_frame_host)) {
    return origin.GetTupleOrPrecursorTupleIfOpaque().GetURL();
  }

  return origin.GetURL();
}

bool PermissionUtil::IsDevicePermission(
    const blink::mojom::PermissionDescriptorPtr& descriptor) {
  return descriptor->name == blink::mojom::PermissionName::VIDEO_CAPTURE ||
         descriptor->name == blink::mojom::PermissionName::AUDIO_CAPTURE ||
         descriptor->name == blink::mojom::PermissionName::GEOLOCATION ||
         descriptor->name == blink::mojom::PermissionName::AR ||
         descriptor->name == blink::mojom::PermissionName::VR ||
         descriptor->name == blink::mojom::PermissionName::HAND_TRACKING;
}

bool PermissionUtil::IsEmbeddablePermission(
    const blink::mojom::PermissionDescriptorPtr& descriptor) {
  return descriptor->name == blink::mojom::PermissionName::VIDEO_CAPTURE ||
         descriptor->name == blink::mojom::PermissionName::AUDIO_CAPTURE ||
         descriptor->name == blink::mojom::PermissionName::GEOLOCATION ||
         descriptor->name == blink::mojom::PermissionName::WEB_APP_INSTALLATION;
}

}  // namespace content
