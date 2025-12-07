// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/media_player_util.h"

#include <math.h>
#include <stddef.h>

#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "media/base/media_log.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-blink.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {

void RunSetSinkIdCallback(blink::WebSetSinkIdCompleteCallback callback,
                          media::OutputDeviceStatus result) {
  switch (result) {
    case media::OUTPUT_DEVICE_STATUS_OK:
      std::move(callback).Run(/*error =*/std::nullopt);
      break;
    case media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      std::move(callback).Run(blink::WebSetSinkIdError::kNotFound);
      break;
    case media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      std::move(callback).Run(blink::WebSetSinkIdError::kNotAuthorized);
      break;
    case media::OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
    case media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL:
      std::move(callback).Run(blink::WebSetSinkIdError::kAborted);
      break;
  }
}

}  // namespace

namespace blink {

media::mojom::blink::MediaURLScheme GetMediaURLScheme(const KURL& url) {
  if (!url.GetParsed().scheme.is_valid()) {
    return media::mojom::blink::MediaURLScheme::kMissing;
  }
  if (url.ProtocolIs(url::kHttpScheme)) {
    return media::mojom::blink::MediaURLScheme::kHttp;
  }
  if (url.ProtocolIs(url::kHttpsScheme)) {
    return media::mojom::blink::MediaURLScheme::kHttps;
  }
  if (url.ProtocolIs(url::kFtpScheme)) {
    return media::mojom::blink::MediaURLScheme::kFtp;
  }
  if (url.ProtocolIs(url::kJavaScriptScheme)) {
    return media::mojom::blink::MediaURLScheme::kJavascript;
  }
  if (url.ProtocolIs(url::kFileScheme)) {
    return media::mojom::blink::MediaURLScheme::kFile;
  }
  if (url.ProtocolIs(url::kBlobScheme)) {
    return media::mojom::blink::MediaURLScheme::kBlob;
  }
  if (url.ProtocolIs(url::kDataScheme)) {
    return media::mojom::blink::MediaURLScheme::kData;
  }
  if (url.ProtocolIs(url::kFileSystemScheme)) {
    return media::mojom::blink::MediaURLScheme::kFileSystem;
  }
  if (url.ProtocolIs(url::kContentScheme)) {
    return media::mojom::blink::MediaURLScheme::kContent;
  }
  if (url.ProtocolIs(url::kContentIDScheme)) {
    return media::mojom::blink::MediaURLScheme::kContentId;
  }

  // Some internals pages and extension pages play media.
  if (SchemeRegistry::IsWebUIScheme(url.Protocol())) {
    return media::mojom::blink::MediaURLScheme::kChrome;
  }
  if (CommonSchemeRegistry::IsExtensionScheme(url.Protocol().Ascii())) {
    return media::mojom::blink::MediaURLScheme::kChromeExtension;
  }

  return media::mojom::blink::MediaURLScheme::kUnknown;
}

void ReportMetrics(WebMediaPlayer::LoadType load_type,
                   const KURL& url,
                   media::MediaLog* media_log) {
  DCHECK(media_log);

  // Report URL scheme, such as http, https, file, blob etc. Only do this for
  // URL based loads, otherwise it's not very useful.
  if (load_type == WebMediaPlayer::kLoadTypeURL) {
    UMA_HISTOGRAM_ENUMERATION("Media.URLScheme2", GetMediaURLScheme(url));
  }

  // Report load type, such as URL, MediaSource or MediaStream.
  UMA_HISTOGRAM_ENUMERATION("Media.LoadType", load_type,
                            WebMediaPlayer::kLoadTypeMax + 1);
}

media::OutputDeviceStatusCB ConvertToOutputDeviceStatusCB(
    WebSetSinkIdCompleteCallback callback) {
  return base::BindPostTaskToCurrentDefault(
      blink::BindOnce(RunSetSinkIdCallback, std::move(callback)));
}

}  // namespace blink
