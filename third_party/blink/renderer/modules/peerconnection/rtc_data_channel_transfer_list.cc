// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel_transfer_list.h"

#include "third_party/blink/renderer/modules/peerconnection//rtc_data_channel.h"

namespace blink {

const void* const RTCDataChannelTransferList::kTransferListKey = nullptr;

void RTCDataChannelTransferList::Trace(Visitor* visitor) const {
  visitor->Trace(data_channel_collection);
}

}  // namespace blink
