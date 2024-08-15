// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SCRIPT_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SCRIPT_TRANSFORM_H_

#include <optional>

#include "base/thread_annotations.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transformer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"

namespace blink {

class ExceptionState;
class ScriptState;

class MODULES_EXPORT RTCRtpScriptTransform : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCRtpScriptTransform* Create(ScriptState*,
                                       DedicatedWorker* worker,
                                       ExceptionState&);
  static RTCRtpScriptTransform* Create(ScriptState*,
                                       DedicatedWorker* worker,
                                       const ScriptValue& message,
                                       ExceptionState&);
  static RTCRtpScriptTransform* Create(ScriptState*,
                                       DedicatedWorker* worker,
                                       const ScriptValue& message,
                                       HeapVector<ScriptValue>& transfer,
                                       ExceptionState&);

  RTCRtpScriptTransform() = default;
  ~RTCRtpScriptTransform() override = default;

  // Called on the main thread when the RTCTransformEvent is created.
  void SetTransformer(
      CrossThreadWeakHandle<RTCRtpScriptTransformer>,
      scoped_refptr<base::SingleThreadTaskRunner> transformer_task_runner);

  // Called on the main thread in the setter of the transform in RTCRtpSender
  // or RTCRtpReceiver.
  void CreateUnderlyingSourceAndSetAudioTransformer(
      WTF::CrossThreadOnceClosure disconnect_callback_source,
      scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
          encoded_audio_transformer);
  void CreateUnderlyingSourceAndSetVideoTransformer(
      WTF::CrossThreadOnceClosure disconnect_callback_source,
      scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
          encoded_video_transformer);

  void Attach() { is_attached_ = true; }
  bool IsAttached() { return is_attached_; }

 private:
  void CreateUnderlyingSource(
      WTF::CrossThreadOnceClosure disconnect_callback_source,
      String kind) EXCLUSIVE_LOCKS_REQUIRED(transformer_lock_);

  void CreateUnderlyingSourceInternal(
      const String& kind,
      WTF::CrossThreadOnceClosure disconnect_callback_source)
      EXCLUSIVE_LOCKS_REQUIRED(transformer_lock_);

  base::Lock transformer_lock_;
  std::optional<CrossThreadWeakHandle<RTCRtpScriptTransformer>> transformer_
      GUARDED_BY(transformer_lock_);

  scoped_refptr<base::SingleThreadTaskRunner> transformer_task_runner_;

  // These fields are used to store the callbacks only if the transformer has
  // not been created/set yet. The callbacks will be invoked once the
  // transformer becomes available.
  WTF::CrossThreadOnceClosure disconnect_callback_source_;

  scoped_refptr<blink::RTCEncodedAudioStreamTransformer::Broker>
      encoded_audio_transformer_;
  scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
      encoded_video_transformer_;

  String kind_;
  bool is_attached_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SCRIPT_TRANSFORM_H_
