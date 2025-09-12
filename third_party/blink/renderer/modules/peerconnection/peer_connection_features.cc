// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"

#include "base/feature_list.h"

namespace blink {

// When performing Encoded Transform in-process transfer optimization, set the
// encoded_audio_transformer_'s transform callback to directly call the new
// underlying source rather than bouncing via the RTCRtpSender or
// RTCRtpReceiver.
BASE_FEATURE(WebRtcEncodedTransformDirectCallback,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Keep the metadata of RTC encoded frames after they are neutered.
// If disabled, the metadata is reset to default or no values.
BASE_FEATURE(kWebRtcEncodedTransformRememberMetadata,
             "WebRtcEncodedTransformRememberMetadata",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Keep the type ("key" or "delta") of an RTCEncodedVideoFrame after it is
// neutered. If disabled, the type changes to "empty".
BASE_FEATURE(kWebRtcEncodedTransformRememberVideoFrameType,
             "WebRtcEncodedTransformRememberVideoFrameType",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature enables encrypting RTP header extensions using RFC 6904, if
// requested. Requesting should be done using the RTP header extension API;
// doing it via SDP munging is possible, but not recommended.
BASE_FEATURE(WebRtcEncryptedRtpHeaderExtensions,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This features enables the restriction that frames sent to an
// RTCRtpScriptTransformer's writable must come from the transformer's readable
// and must be written in the same order in which they are read. This feature
// does not affect streams created with the createEncodedStreams() method, which
// never applies this restriction.
BASE_FEATURE(WebRtcRtpScriptTransformerFrameRestrictions,
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature unumutes a track when a packet arrives instead of after
// setRemoteDescription.
BASE_FEATURE(WebRtcUnmuteTracksWhenPacketArrives,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace blink
