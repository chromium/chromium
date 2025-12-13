// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_WEB_RTC_CROSS_THREAD_COPIER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_WEB_RTC_CROSS_THREAD_COPIER_H_

// This file defines specializations for the CrossThreadCopier that allow WebRTC
// types to be passed across threads using their copy constructors.

#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/transport/network_types.h"
#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/p2p/base/transport_description.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

// TODO(crbug.com/460743390): Delete this file after CrossThreadCopier removal.

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_WEB_RTC_CROSS_THREAD_COPIER_H_
