// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_LOGGING_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_LOGGING_H_

#include <string>

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// This interface is implemented by a handler in the embedder and used for
// initializing the logging and passing log messages to the handler. The
// purpose is to forward mainly libjingle log messages to embedder (besides
// the ordinary logging stream) that will be used for diagnostic purposes.
class BLINK_PLATFORM_EXPORT WebRtcLogMessageDelegate {
 public:
  // Pass a diagnostic WebRTC log message.
  virtual void LogMessage(const std::string& message) = 0;

 protected:
  virtual ~WebRtcLogMessageDelegate() {}
};

// Must only be called once, and |delegate| must be non-null.
BLINK_PLATFORM_EXPORT void InitWebRtcLoggingDelegate(
    WebRtcLogMessageDelegate* delegate);

// Called to start the diagnostic WebRTC log.
BLINK_PLATFORM_EXPORT void InitWebRtcLogging();

// This function will add |message| to the diagnostic WebRTC log, if started.
// Otherwise it will be ignored. Note that this log may be uploaded to a
// server by the embedder - no sensitive information should be logged. May be
// called on any thread.
// TODO(grunell): Create a macro for adding log messages. Messages should
// probably also go to the ordinary logging stream.
void BLINK_PLATFORM_EXPORT WebRtcLogMessage(const std::string& message);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_WEBRTC_WEBRTC_LOGGING_H_
