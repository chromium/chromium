// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_DIAGNOSTIC_LOGGING_RTC_DIAGNOSTIC_LOGGING_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_DIAGNOSTIC_LOGGING_RTC_DIAGNOSTIC_LOGGING_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class RTCDiagnosticLoggingOptions;
class ScriptState;

class MODULES_EXPORT RTCDiagnosticLogging final {
  STATIC_ONLY(RTCDiagnosticLogging);

 public:
  static String startDiagnosticLogging(ScriptState*,
                                       const RTCDiagnosticLoggingOptions*,
                                       ExceptionState&);
  static void stopDiagnosticLogging(ScriptState*, ExceptionState&);
  static void cancelDiagnosticLogging(ScriptState*, ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_DIAGNOSTIC_LOGGING_RTC_DIAGNOSTIC_LOGGING_H_
