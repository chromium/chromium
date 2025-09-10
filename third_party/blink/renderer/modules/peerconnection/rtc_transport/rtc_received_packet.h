// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_RECEIVED_PACKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_RECEIVED_PACKET_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class MODULES_EXPORT RtcReceivedPacket final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RtcReceivedPacket(DOMArrayBuffer* data) : data_(data) {}

  DOMArrayBuffer* data();

  // ScriptWrappable impl
  void Trace(Visitor* visitor) const override;

 private:
  Member<DOMArrayBuffer> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_RECEIVED_PACKET_H_
