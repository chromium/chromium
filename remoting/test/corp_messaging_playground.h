// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
#define REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_

#include <memory>
#include <set>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/rsa_key_pair.h"

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
  explicit CorpMessagingPlayground(const std::string& username);
  ~CorpMessagingPlayground();

  CorpMessagingPlayground(const CorpMessagingPlayground&) = delete;
  CorpMessagingPlayground& operator=(const CorpMessagingPlayground&) = delete;

  void Start();

 private:
  class Core;

  void OnStreamOpened();
  void OnStreamClosed(const HttpStatus& status);
  void OnPeerMessageReceived(const internal::PeerMessageStruct& message);
  void OnCharacterInput(char c);
  void SendMessage(int count = 1);
  void StartPingPongRally();
  void SendLargeMessage();
  void OnBurstCheckTimerFired();
  void ResetBurstState();

  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  scoped_refptr<RsaKeyPair> key_pair_{RsaKeyPair::Generate()};
  std::unique_ptr<CorpMessagingClient> client_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<Core> core_;
  std::string messaging_authz_token_;
  base::Time last_ping_sent_time_;
  base::TimeDelta ping_total_rtt_;

  // Burst message related members.
  int expected_burst_count_ = 0;
  std::set<int> received_burst_indices_;
  base::TimeTicks burst_start_time_;
  base::RepeatingTimer burst_check_timer_;
  int burst_timer_check_count_ = 0;

  base::WeakPtrFactory<CorpMessagingPlayground> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
