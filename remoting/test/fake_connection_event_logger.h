// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_CONNECTION_EVENT_LOGGER_H_
#define REMOTING_TEST_FAKE_CONNECTION_EVENT_LOGGER_H_

#include <memory>
#include <ostream>

#include "remoting/protocol/audio_stub.h"
#include "remoting/protocol/client_stub.h"
#include "remoting/protocol/fake_connection_to_client.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace test {

class FakeConnectionEventLogger {
 public:
  explicit FakeConnectionEventLogger(
      protocol::FakeConnectionToClient* connection = nullptr);

  FakeConnectionEventLogger(const FakeConnectionEventLogger&) = delete;
  FakeConnectionEventLogger& operator=(const FakeConnectionEventLogger&) =
      delete;

  virtual ~FakeConnectionEventLogger();

  protocol::ClientStub* client_stub();
  protocol::HostStub* host_stub();
  protocol::AudioStub* audio_stub();
  protocol::VideoStub* video_stub();
  friend std::ostream& operator<<(std::ostream& os,
                                  const FakeConnectionEventLogger& logger);

 private:
  class CounterClientStub;
  class CounterHostStub;
  class CounterAudioStub;
  class CounterVideoStub;

  std::unique_ptr<CounterClientStub> client_stub_;
  std::unique_ptr<CounterHostStub> host_stub_;
  std::unique_ptr<CounterAudioStub> audio_stub_;
  std::unique_ptr<CounterVideoStub> video_stub_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_CONNECTION_EVENT_LOGGER_H_
