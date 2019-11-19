// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_GCD_STATE_UPDATER_H_
#define REMOTING_HOST_GCD_STATE_UPDATER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/backoff_timer.h"
#include "remoting/host/gcd_rest_client.h"
#include "remoting/signaling/signal_strategy.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

// An object that uses GcdRestClient to send status updates to GCD
// when the XMPP connection state changes.
class GcdStateUpdater : public SignalStrategy::Listener {
 public:
  // |signal_strategy| must outlive this object. Updates will start
  // when the supplied SignalStrategy enters the CONNECTED state.
  //
  // |on_update_successful_callback| is invoked after the first successful
  // update.
  GcdStateUpdater(const base::Closure& on_update_successful_callback,
                  const base::Closure& on_unknown_host_id_error,
                  SignalStrategy* signal_strategy,
                  std::unique_ptr<GcdRestClient> gcd_client);
  ~GcdStateUpdater() override;

  // See XmppHeartbeatSender::SetHostOfflineReason.
  void SetHostOfflineReason(
      const std::string& host_offline_reason,
      const base::TimeDelta& timeout,
      const base::Callback<void(bool success)>& ack_callback);

 private:
  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(const jingle_xmpp::XmlElement* stanza) override;

  void OnPatchStateResult(GcdRestClient::Result result);
  void MaybeSendStateUpdate();

  base::Closure on_update_successful_callback_;
  base::Closure on_unknown_host_id_error_;
  SignalStrategy* signal_strategy_;
  std::unique_ptr<GcdRestClient> gcd_rest_client_;
  BackoffTimer timer_;
  base::ThreadChecker thread_checker_;
  std::string pending_request_jid_;

  DISALLOW_COPY_AND_ASSIGN(GcdStateUpdater);
};

}  // namespace remoting

#endif  // REMOTING_HOST_GCD_STATE_UPDATER_H_
