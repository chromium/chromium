// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/features.h"

namespace webrtc::features {

// Boosts the thread types of IO threads of the utility processes on the WebRTC
// media path: the video capture utility process's IO thread (to
// base::ThreadType::kPresentation) for the lifetime of the process (see
// content/utility/utility_main.cc) and the network utility process's IO thread
// (to base::ThreadType::kAudioProcessing) through a lease held only while at
// least one active peer-to-peer connection exists (see
// network::NetworkService::OnPeerToPeerConnectionsCountChange()). This is a
// narrowly-scoped alternative to features::kIOThreadInteractiveThreadType,
// which boosts all IO threads.
BASE_FEATURE(kWebRTCBoostMediaIOThreads, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace webrtc::features
