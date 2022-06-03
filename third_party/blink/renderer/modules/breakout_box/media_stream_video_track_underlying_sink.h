// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class WritableStreamTransferringOptimizer;

class MODULES_EXPORT MediaStreamVideoTrackUnderlyingSink
    : public UnderlyingSinkBase {
 public:
  explicit MediaStreamVideoTrackUnderlyingSink(
      scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker);

  // UnderlyingSinkBase overrides.
  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override;
  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override;
  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override;
  ScriptPromise close(ScriptState* script_state,
                      ExceptionState& exception_state) override;

  std::unique_ptr<WritableStreamTransferringOptimizer>
  GetTransferringOptimizer();

 private:
  void Disconnect();
  const scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker_;
  bool is_connected_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_
