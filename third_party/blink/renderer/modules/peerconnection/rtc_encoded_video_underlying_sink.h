// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_UNDERLYING_SINK_H_

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

class ExceptionState;
class RTCEncodedVideoStreamTransformer;

class MODULES_EXPORT RTCEncodedVideoUnderlyingSink final
    : public UnderlyingSinkBase {
 public:
  using TransformerCallback =
      base::RepeatingCallback<RTCEncodedVideoStreamTransformer*()>;
  RTCEncodedVideoUnderlyingSink(ScriptState*,
                                TransformerCallback,
                                webrtc::TransformableFrameInterface::Direction);

  // UnderlyingSinkBase
  ScriptPromise start(ScriptState*,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise write(ScriptState*,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise close(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;

  void Trace(Visitor*) const override;

 private:
  TransformerCallback transformer_callback_;
  webrtc::TransformableFrameInterface::Direction expected_direction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_VIDEO_UNDERLYING_SINK_H_
