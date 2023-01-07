// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>

#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

namespace blink {

// Shall only be set once and never go back to NULL.
WebRtcLogMessageDelegate* g_webrtc_logging_delegate = nullptr;

void InitWebRtcLoggingDelegate(WebRtcLogMessageDelegate* delegate) {
  CHECK(!g_webrtc_logging_delegate);
  CHECK(delegate);

  g_webrtc_logging_delegate = delegate;
}

void InitWebRtcLogging() {
  // Log messages from Libjingle should not have timestamps.
  rtc::InitDiagnosticLoggingDelegateFunction(&WebRtcLogMessage);
}

void WebRtcLogMessage(const std::string& message) {
  VLOG(1) << message;
  if (g_webrtc_logging_delegate)
    g_webrtc_logging_delegate->LogMessage(message);
}

void WebRtcLog(const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string msg(base::StringPrintV(format, args));
  va_end(args);
  WebRtcLogMessage(msg);
}

void WebRtcLog(void* thiz, const char* format, ...) {
  va_list args;
  va_start(args, format);
  std::string msg(base::StringPrintV(format, args));
  va_end(args);
  base::StringAppendF(&msg, " [this=0x%" PRIXPTR "]",
                      reinterpret_cast<uintptr_t>(thiz));
  WebRtcLogMessage(msg);
}

void BLINK_PLATFORM_EXPORT WebRtcLog(const char* prefix,
                                     void* thiz,
                                     const char* format,
                                     ...) {
  va_list args;
  va_start(args, format);
  std::string msg(prefix);
  base::StringAppendV(&msg, format, args);
  va_end(args);
  base::StringAppendF(&msg, " [this=0x%" PRIXPTR "]",
                      reinterpret_cast<uintptr_t>(thiz));
  WebRtcLogMessage(msg);
}

}  // namespace blink
