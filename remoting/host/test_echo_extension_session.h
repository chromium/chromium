// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TEST_ECHO_EXTENSION_SESSION_H_
#define REMOTING_HOST_TEST_ECHO_EXTENSION_SESSION_H_

#include "remoting/host/host_extension_session.h"

namespace remoting {

class TestEchoExtensionSession : public HostExtensionSession {
 public:
  TestEchoExtensionSession();
  ~TestEchoExtensionSession() override;

  // HostExtensionSession interface.
  bool OnExtensionMessage(ClientSessionDetails* client_session_details,
                          protocol::ClientStub* client_stub,
                          const protocol::ExtensionMessage& message) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_TEST_ECHO_EXTENSION_SESSION_H_
