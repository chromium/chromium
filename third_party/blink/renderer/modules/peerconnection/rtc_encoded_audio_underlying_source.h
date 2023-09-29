// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SOURCE_H_

#include "base/gtest_prod_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace webrtc {
class TransformableAudioFrameInterface;
}  // namespace webrtc

namespace blink {

class MODULES_EXPORT RTCEncodedAudioUnderlyingSource
    : public UnderlyingSourceBase {
 public:
  explicit RTCEncodedAudioUnderlyingSource(
      ScriptState*,
      WTF::CrossThreadOnceClosure disconnect_callback);

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*,
                       ScriptValue reason,
                       ExceptionState&) override;

  void OnFrameFromSource(
      std::unique_ptr<webrtc::TransformableAudioFrameInterface>);
  void Close();

  // Called on any thread to indicate the source is being transferred to an
  // UnderlyingSource on a different thread to this.
  void OnSourceTransferStarted();

  void Trace(Visitor*) const override;

 private:
  // Implements the handling of this stream being transferred to another
  // context, called on the thread upon which the instance was created.
  void OnSourceTransferStartedOnTaskRunner();

  FRIEND_TEST_ALL_PREFIXES(RTCEncodedAudioUnderlyingSourceTest,
                           QueuedFramesAreDroppedWhenOverflow);
  static const int kMinQueueDesiredSize;

  const Member<ScriptState> script_state_;
  WTF::CrossThreadOnceClosure disconnect_callback_;
  // Count of frames dropped due to the queue being full, for logging.
  int dropped_frames_ = 0;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SOURCE_H_
