// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TEST_ECHO_EXTENSION_H_
#define REMOTING_HOST_TEST_ECHO_EXTENSION_H_

#include "remoting/host/host_extension.h"

namespace remoting {

class TestEchoExtension : public HostExtension {
 public:
  TestEchoExtension();

  TestEchoExtension(const TestEchoExtension&) = delete;
  TestEchoExtension& operator=(const TestEchoExtension&) = delete;

  ~TestEchoExtension() override;

  // HostExtension interface.
  std::string capability() const override;
  std::unique_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionDetails* client_session_details,
      protocol::ClientStub* client_stub) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_TEST_ECHO_EXTENSION_H_
