// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediaplayer_util.h"

#include <math.h>
#include <stddef.h>
#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_log.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_media_player_encrypted_media_client.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

std::string LoadTypeToString(blink::WebMediaPlayer::LoadType load_type) {
  switch (load_type) {
    case blink::WebMediaPlayer::kLoadTypeURL:
      return "SRC";
    case blink::WebMediaPlayer::kLoadTypeMediaSource:
      return "MSE";
    case blink::WebMediaPlayer::kLoadTypeMediaStream:
      return "MS";
  }

  NOTREACHED();
  return "Unknown";
}

}  // namespace

namespace media {

mojom::MediaURLScheme GetMediaURLScheme(const GURL& url) {
  if (!url.has_scheme())
    return mojom::MediaURLScheme::kMissing;
  if (url.SchemeIs(url::kHttpScheme))
    return mojom::MediaURLScheme::kHttp;
  if (url.SchemeIs(url::kHttpsScheme))
    return mojom::MediaURLScheme::kHttps;
  if (url.SchemeIs(url::kFtpScheme))
    return mojom::MediaURLScheme::kFtp;
  if (url.SchemeIs(url::kJavaScriptScheme))
    return mojom::MediaURLScheme::kJavascript;
  if (url.SchemeIsFile())
    return mojom::MediaURLScheme::kFile;
  if (url.SchemeIsBlob())
    return mojom::MediaURLScheme::kBlob;
  if (url.SchemeIs(url::kDataScheme))
    return mojom::MediaURLScheme::kData;
  if (url.SchemeIsFileSystem())
    return mojom::MediaURLScheme::kFileSystem;
  if (url.SchemeIs(url::kContentScheme))
    return mojom::MediaURLScheme::kContent;
  if (url.SchemeIs(url::kContentIDScheme))
    return mojom::MediaURLScheme::kContentId;

  // Some internals pages and extension pages play media.
  if (url.SchemeIs("chrome"))
    return mojom::MediaURLScheme::kChrome;
  if (url.SchemeIs("chrome-extension"))
    return mojom::MediaURLScheme::kChromeExtension;

  return mojom::MediaURLScheme::kUnknown;
}

blink::WebTimeRanges ConvertToWebTimeRanges(
    const Ranges<base::TimeDelta>& ranges) {
  blink::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); ++i) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

blink::WebMediaPlayer::NetworkState PipelineErrorToNetworkState(
    PipelineStatus error) {
  switch (error) {
    case PIPELINE_ERROR_NETWORK:
    case PIPELINE_ERROR_READ:
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR:
      return blink::WebMediaPlayer::kNetworkStateNetworkError;

    case PIPELINE_ERROR_INITIALIZATION_FAILED:
    case PIPELINE_ERROR_COULD_NOT_RENDER:
    case PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED:
    case DEMUXER_ERROR_COULD_NOT_OPEN:
    case DEMUXER_ERROR_COULD_NOT_PARSE:
    case DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case DEMUXER_ERROR_DETECTED_HLS:
    case DECODER_ERROR_NOT_SUPPORTED:
      return blink::WebMediaPlayer::kNetworkStateFormatError;

    case PIPELINE_ERROR_DECODE:
    case PIPELINE_ERROR_ABORT:
    case PIPELINE_ERROR_INVALID_STATE:
    case CHUNK_DEMUXER_ERROR_APPEND_FAILED:
    case CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR:
    case AUDIO_RENDERER_ERROR:
      return blink::WebMediaPlayer::kNetworkStateDecodeError;

    case PIPELINE_OK:
      NOTREACHED() << "Unexpected status! " << error;
  }
  return blink::WebMediaPlayer::kNetworkStateFormatError;
}

void ReportMetrics(blink::WebMediaPlayer::LoadType load_type,
                   const GURL& url,
                   const blink::WebLocalFrame& frame,
                   MediaLog* media_log) {
  DCHECK(media_log);

  // Report URL scheme, such as http, https, file, blob etc. Only do this for
  // URL based loads, otherwise it's not very useful.
  if (load_type == blink::WebMediaPlayer::kLoadTypeURL)
    UMA_HISTOGRAM_ENUMERATION("Media.URLScheme2", GetMediaURLScheme(url));

  // Report load type, such as URL, MediaSource or MediaStream.
  UMA_HISTOGRAM_ENUMERATION("Media.LoadType", load_type,
                            blink::WebMediaPlayer::kLoadTypeMax + 1);

  // Report load type separately for ad frames.
  if (frame.IsAdSubframe()) {
    UMA_HISTOGRAM_ENUMERATION("Ads.Media.LoadType", load_type,
                              blink::WebMediaPlayer::kLoadTypeMax + 1);
  }

  // Report the origin from where the media player is created.
  media_log->RecordRapporWithSecurityOrigin("Media.OriginUrl." +
                                            LoadTypeToString(load_type));

  // For MSE, also report usage by secure/insecure origin.
  if (load_type == blink::WebMediaPlayer::kLoadTypeMediaSource) {
    if (frame.GetSecurityOrigin().IsPotentiallyTrustworthy()) {
      media_log->RecordRapporWithSecurityOrigin("Media.OriginUrl.MSE.Secure");
    } else {
      media_log->RecordRapporWithSecurityOrigin("Media.OriginUrl.MSE.Insecure");
    }
  }
}

void ReportPipelineError(blink::WebMediaPlayer::LoadType load_type,
                         PipelineStatus error,
                         MediaLog* media_log) {
  DCHECK_NE(PIPELINE_OK, error);

  // Report the origin from where the media player is created.
  media_log->RecordRapporWithSecurityOrigin(
      "Media.OriginUrl." + LoadTypeToString(load_type) + ".PipelineError");
}

EmeInitDataType ConvertToEmeInitDataType(
    blink::WebEncryptedMediaInitDataType init_data_type) {
  switch (init_data_type) {
    case blink::WebEncryptedMediaInitDataType::kWebm:
      return EmeInitDataType::WEBM;
    case blink::WebEncryptedMediaInitDataType::kCenc:
      return EmeInitDataType::CENC;
    case blink::WebEncryptedMediaInitDataType::kKeyids:
      return EmeInitDataType::KEYIDS;
    case blink::WebEncryptedMediaInitDataType::kUnknown:
      return EmeInitDataType::UNKNOWN;
  }

  NOTREACHED();
  return EmeInitDataType::UNKNOWN;
}

blink::WebEncryptedMediaInitDataType ConvertToWebInitDataType(
    EmeInitDataType init_data_type) {
  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      return blink::WebEncryptedMediaInitDataType::kWebm;
    case EmeInitDataType::CENC:
      return blink::WebEncryptedMediaInitDataType::kCenc;
    case EmeInitDataType::KEYIDS:
      return blink::WebEncryptedMediaInitDataType::kKeyids;
    case EmeInitDataType::UNKNOWN:
      return blink::WebEncryptedMediaInitDataType::kUnknown;
  }

  NOTREACHED();
  return blink::WebEncryptedMediaInitDataType::kUnknown;
}

namespace {

void RunSetSinkIdCallback(
    std::unique_ptr<blink::WebSetSinkIdCallbacks> web_callbacks,
    OutputDeviceStatus result) {
  switch (result) {
    case OUTPUT_DEVICE_STATUS_OK:
      web_callbacks->OnSuccess();
      break;
    case OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND:
      web_callbacks->OnError(blink::WebSetSinkIdError::kNotFound);
      break;
    case OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED:
      web_callbacks->OnError(blink::WebSetSinkIdError::kNotAuthorized);
      break;
    case OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT:
    case OUTPUT_DEVICE_STATUS_ERROR_INTERNAL:
      web_callbacks->OnError(blink::WebSetSinkIdError::kAborted);
      break;
  }
}

}  // namespace

OutputDeviceStatusCB ConvertToOutputDeviceStatusCB(
    std::unique_ptr<blink::WebSetSinkIdCallbacks> callbacks) {
  return media::BindToCurrentLoop(
      base::BindOnce(RunSetSinkIdCallback, std::move(callbacks)));
}

}  // namespace media
