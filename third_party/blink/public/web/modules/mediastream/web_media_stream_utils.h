// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_UTILS_H_

#include <memory>

#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/platform/web_common.h"

namespace blink {

class WebMediaStreamSink;
class WebMediaStreamTrack;

// Requests that a refresh frame be sent "soon" (e.g., to resolve picture loss
// or quality issues).
//
// TODO(crbug.com/704136): Move these helper functions out of the Blink
// public API. Note for while moving it: there is an existing
// media_stream_utils.h on renderer/modules/mediastream.
BLINK_MODULES_EXPORT void RequestRefreshFrameFromVideoTrack(
    const WebMediaStreamTrack& video_track);

// Calls to these methods must be done on the main render thread.
// Note that |callback| for frame delivery happens on the IO thread.
// Warning: Calling RemoveSinkFromMediaStreamTrack does not immediately stop
// frame delivery through the |callback|, since frames are being delivered on
// a different thread.
// |is_sink_secure| indicates if |sink| meets output protection requirement.
// Generally, this should be false unless you know what you are doing.
BLINK_MODULES_EXPORT void AddSinkToMediaStreamTrack(
    const WebMediaStreamTrack& track,
    WebMediaStreamSink* sink,
    const VideoCaptureDeliverFrameCB& callback,
    bool is_sink_secure);
BLINK_MODULES_EXPORT void RemoveSinkFromMediaStreamTrack(
    const WebMediaStreamTrack& track,
    WebMediaStreamSink* sink);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_UTILS_H_
