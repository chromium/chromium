// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_WEBMEDIAPLAYER_UTIL_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_WEBMEDIAPLAYER_UTIL_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/eme_constants.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/mojo/mojom/media_metrics_provider.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_encrypted_media_types.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_set_sink_id_callbacks.h"
#include "third_party/blink/public/platform/web_time_range.h"

namespace media {
class MediaLog;
}  // namespace media

namespace blink {

class WebLocalFrame;

// Translates a |url| into the appropriate URL scheme.
BLINK_MODULES_EXPORT media::mojom::MediaURLScheme GetMediaURLScheme(
    const WebURL& url);

BLINK_MODULES_EXPORT WebTimeRanges
ConvertToWebTimeRanges(const media::Ranges<base::TimeDelta>& ranges);

BLINK_MODULES_EXPORT WebMediaPlayer::NetworkState PipelineErrorToNetworkState(
    media::PipelineStatus error);

// Report various metrics to UMA.
BLINK_MODULES_EXPORT void ReportMetrics(WebMediaPlayer::LoadType load_type,
                                        const WebURL& url,
                                        const WebLocalFrame& frame,
                                        media::MediaLog* media_log);

// Wraps a WebSetSinkIdCompleteCallback into a
// media::OutputDeviceStatusCB and binds it to the current thread
BLINK_MODULES_EXPORT media::OutputDeviceStatusCB ConvertToOutputDeviceStatusCB(
    WebSetSinkIdCompleteCallback completion_callback);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIA_WEBMEDIAPLAYER_UTIL_H_
