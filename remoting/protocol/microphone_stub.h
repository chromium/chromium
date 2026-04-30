// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MICROPHONE_STUB_H_
#define REMOTING_PROTOCOL_MICROPHONE_STUB_H_

namespace remoting::protocol {

class MicrophoneControl;

class MicrophoneStub {
 public:
  virtual ~MicrophoneStub() = default;

  // Sends a microphone control message.
  virtual void ControlMicrophone(const MicrophoneControl& control) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MICROPHONE_STUB_H_
