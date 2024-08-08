// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SCRIPT_TRANSFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SCRIPT_TRANSFORM_H_

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

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
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_SCRIPT_TRANSFORM_H_
