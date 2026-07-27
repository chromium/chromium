// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
#define REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/ecdh_key_exchange.h"
#include "remoting/base/internal_headers.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/signaling/signaling_address.h"

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
  explicit CorpMessagingPlayground(std::string username);
  ~CorpMessagingPlayground();

  CorpMessagingPlayground(const CorpMessagingPlayground&) = delete;
  CorpMessagingPlayground& operator=(const CorpMessagingPlayground&) = delete;

  void Start();

 private:
  class Core;

  void OnStreamOpened();
  void OnStreamClosed(const HttpStatus& status);
  void OnSignalingAddressChanged(const SignalingAddress& local_address);
  void OnPeerMessageReceived(const SignalingAddress& sender_address,
                             const internal::PeerMessageStruct& message);
  void OnCharacterInput(char c);
  void SendMessage(int count = 1);
  void StartPingPongRally();
  void SendLargeMessage();
  void SendIqStanza();
  void OnKeyPairGenerated(scoped_refptr<RsaKeyPair> key_pair);
  void HandleIqStanza(const internal::IqStanzaStruct& iq_stanza);
  void HandleSystemTest(const internal::SystemTestStruct& system_test);
  void OnBurstCheckTimerFired();
  void ResetBurstState();

  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
  scoped_refptr<RsaKeyPair> key_pair_;
  std::unique_ptr<EcdhKeyExchange> key_exchange_;
  std::unique_ptr<EcdhKeyExchange::AesGcmCrypter> crypter_;
  std::unique_ptr<CorpMessagingClient> client_;
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<Core> core_;
  int shutdown_pipe_[2] = {-1, -1};
  std::string username_;
  std::string messaging_authz_token_;
  std::map<int, base::TimeTicks> ping_sent_times_;
  std::map<int, base::TimeDelta> ping_rtts_;
  base::CallbackListSubscription message_callback_subscription_;

  // Burst message related members.
  int expected_burst_count_ = 0;
  std::set<int> received_burst_indices_;
  base::TimeTicks burst_start_time_;
  base::RepeatingTimer burst_check_timer_;
  int burst_timer_check_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CorpMessagingPlayground> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_TEST_CORP_MESSAGING_PLAYGROUND_H_
