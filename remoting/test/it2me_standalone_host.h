// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_IT2ME_STANDALONE_HOST_H_
#define REMOTING_TEST_IT2ME_STANDALONE_HOST_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/it2me_desktop_environment.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/test/fake_connection_event_logger.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {
class ChromotingHostContext;
class ClientSession;

namespace protocol {
class SessionConfig;
}  // namespace protocol

namespace test {

// A container for an it2me host by using FakeConnectionEventLogger to do
// capturing without needing a client to connect to.
class It2MeStandaloneHost {
 public:
  It2MeStandaloneHost();
  ~It2MeStandaloneHost();

  // Block current thread forever.
  void Run();

  void StartOutputTimer();

 private:
  void Connect();

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::unique_ptr<ChromotingHostContext> context_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
  It2MeDesktopEnvironmentFactory factory_;
  LocalSessionPoliciesProvider local_session_policies_provider_;
  protocol::FakeConnectionToClient connection_;
  std::string session_jid_;
  std::unique_ptr<protocol::SessionConfig> config_;
  FakeConnectionEventLogger event_logger_;
  testing::NiceMock<MockClientSessionEventHandler> handler_;
  std::unique_ptr<ClientSession> session_;
  base::RepeatingTimer timer_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_IT2ME_STANDALONE_HOST_H_
