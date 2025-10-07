// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
#define REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "remoting/base/internal_headers.h"

namespace base {
class RunLoop;
}  // namespace base
namespace network {
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

namespace remoting {
class CorpMessagingClient;
class HttpStatus;

class CorpMessagingPlayground {
 public:
  CorpMessagingPlayground();
  ~CorpMessagingPlayground();

  CorpMessagingPlayground(const CorpMessagingPlayground&) = delete;
  CorpMessagingPlayground& operator=(const CorpMessagingPlayground&) = delete;

  void Start();

 private:
  class Core;

  void OnStreamOpened();
  void OnStreamClosed(const HttpStatus& status);
  void OnSimpleMessageReceived(const internal::SimpleMessageStruct& message);
  void OnCharacterInput(char c);
  void SendMessage(int count = 1);
  void StartPingPongMatch();
  void SendLargeMessage();

  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  std::unique_ptr<CorpMessagingClient> client_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<Core> core_;
  internal::EndpointIdStruct last_sender_id_;
  base::Time last_ping_sent_time_;
  base::TimeDelta ping_total_rtt_;
  base::WeakPtrFactory<CorpMessagingPlayground> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
