// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/modules/media/web_media_player_util.h"

#include <math.h>
#include <stddef.h>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/task/bind_post_task.h"
#include "media/base/media_log.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/web/web_local_frame.h"
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

media::mojom::MediaURLScheme GetMediaURLScheme(const WebURL& url) {
  if (!url.GetParsed().scheme.is_valid())
    return media::mojom::MediaURLScheme::kMissing;
  if (url.ProtocolIs(url::kHttpScheme))
    return media::mojom::MediaURLScheme::kHttp;
  if (url.ProtocolIs(url::kHttpsScheme))
    return media::mojom::MediaURLScheme::kHttps;
  if (url.ProtocolIs(url::kFtpScheme))
    return media::mojom::MediaURLScheme::kFtp;
  if (url.ProtocolIs(url::kJavaScriptScheme))
    return media::mojom::MediaURLScheme::kJavascript;
  if (url.ProtocolIs(url::kFileScheme))
    return media::mojom::MediaURLScheme::kFile;
  if (url.ProtocolIs(url::kBlobScheme))
    return media::mojom::MediaURLScheme::kBlob;
  if (url.ProtocolIs(url::kDataScheme))
    return media::mojom::MediaURLScheme::kData;
  if (url.ProtocolIs(url::kFileSystemScheme))
    return media::mojom::MediaURLScheme::kFileSystem;
  if (url.ProtocolIs(url::kContentScheme))
    return media::mojom::MediaURLScheme::kContent;
  if (url.ProtocolIs(url::kContentIDScheme))
    return media::mojom::MediaURLScheme::kContentId;

  // Some internals pages and extension pages play media.
  KURL kurl(url);
  if (SchemeRegistry::IsWebUIScheme(kurl.Protocol()))
    return media::mojom::MediaURLScheme::kChrome;
  if (CommonSchemeRegistry::IsExtensionScheme(kurl.Protocol().Ascii()))
    return media::mojom::MediaURLScheme::kChromeExtension;

  return media::mojom::MediaURLScheme::kUnknown;
}

WebTimeRanges ConvertToWebTimeRanges(
    const media::Ranges<base::TimeDelta>& ranges) {
  WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); ++i) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

WebMediaPlayer::NetworkState PipelineErrorToNetworkState(
    media::PipelineStatus error) {
  switch (error.code()) {
    case media::PIPELINE_ERROR_NETWORK:
    case media::PIPELINE_ERROR_READ:
    case media::CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR:
      return WebMediaPlayer::kNetworkStateNetworkError;

    case media::PIPELINE_ERROR_INITIALIZATION_FAILED:
    case media::PIPELINE_ERROR_COULD_NOT_RENDER:
    case media::PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED:
    case media::DEMUXER_ERROR_COULD_NOT_OPEN:
    case media::DEMUXER_ERROR_COULD_NOT_PARSE:
    case media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case media::DEMUXER_ERROR_DETECTED_HLS:
    case media::DECODER_ERROR_NOT_SUPPORTED:
      return WebMediaPlayer::kNetworkStateFormatError;

    case media::PIPELINE_ERROR_DECODE:
    case media::PIPELINE_ERROR_ABORT:
    case media::PIPELINE_ERROR_INVALID_STATE:
    case media::PIPELINE_ERROR_HARDWARE_CONTEXT_RESET:
    case media::PIPELINE_ERROR_DISCONNECTED:
    case media::CHUNK_DEMUXER_ERROR_APPEND_FAILED:
    case media::CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR:
    case media::AUDIO_RENDERER_ERROR:
      return WebMediaPlayer::kNetworkStateDecodeError;

    case media::PIPELINE_OK:
      NOTREACHED_IN_MIGRATION() << "Unexpected status! " << error;
  }
  return WebMediaPlayer::kNetworkStateFormatError;
}

void ReportMetrics(WebMediaPlayer::LoadType load_type,
                   const WebURL& url,
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
      WTF::BindOnce(RunSetSinkIdCallback, std::move(callback)));
}

}  // namespace blink
