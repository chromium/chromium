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
BASE_FEATURE(kWebRtcEncodedTransformDirectCallback,
             "WebRtcEncodedTransformDirectCallback",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This features enables the restriction that frames sent to an
// RTCRtpScriptTransformer's writable must come from the transformer's readable
// and must be written in the same order in which they are read. This feature
// does not affect streams created with the createEncodedStreams() method, which
// never applies this restriction.
BASE_FEATURE(kWebRtcRtpScriptTransformerFrameRestrictions,
             "WebRtcRtpScriptTransformerFrameRestrictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace blink
