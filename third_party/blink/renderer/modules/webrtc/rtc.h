// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBRTC_RTC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBRTC_RTC_H_

#include "third_party/blink/public/mojom/rtc_logging/rtc_logging.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class RTCDiagnosticLoggingOptions;
class ScriptState;

class MODULES_EXPORT RTC final : public ScriptWrappable,
                                 public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static RTC* rtc(Navigator&);

  explicit RTC(Navigator&);

  // rtc.idl
  ScriptPromise<IDLString> startDiagnosticLogging(ScriptState*,
                                                  RTCDiagnosticLoggingOptions*);
  ScriptPromise<IDLUndefined> finishDiagnosticLogging(ScriptState*);
  ScriptPromise<IDLUndefined> cancelDiagnosticLogging(ScriptState*);

  static constexpr size_t kMaxMetadataSize = 5;
  static constexpr size_t kMaxMetadataLength = 100;

  void Trace(Visitor*) const override;

 private:
  mojom::blink::RTCLoggingDispatcher& GetDiagnosticLoggingDispatcher();

  HeapMojoRemote<mojom::blink::RTCLoggingDispatcher>
      diagnostic_logging_dispatcher_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBRTC_RTC_H_
