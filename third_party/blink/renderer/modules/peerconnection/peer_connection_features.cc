// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_features.h"

#include "base/feature_list.h"

namespace blink {

// Allows Encoded Transforms to be enabled on a per-Transceiver basis within
// PeerConnections created without the encodedInsertableStreams parameter.
BASE_FEATURE(kWebRtcEncodedTransformsPerStreamCreation,
             "WebRtcEncodedTransformsPerStreamCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace blink
