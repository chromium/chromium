// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/gcd_state_updater.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/strings/stringize_macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

const int64_t kTimerIntervalMinMs = 1000;
const int64_t kTimerIntervalMaxMs = 5 * 60 * 1000;  // 5 minutes

}  // namespace

GcdStateUpdater::GcdStateUpdater(
    const base::Closure& on_update_successful_callback,
    const base::Closure& on_unknown_host_id_error,
    SignalStrategy* signal_strategy,
    std::unique_ptr<GcdRestClient> gcd_rest_client)
    : on_update_successful_callback_(on_update_successful_callback),
      on_unknown_host_id_error_(on_unknown_host_id_error),
      signal_strategy_(signal_strategy),
      gcd_rest_client_(std::move(gcd_rest_client)) {
  DCHECK(signal_strategy_);
  DCHECK(thread_checker_.CalledOnValidThread());

  signal_strategy_->AddListener(this);

  // Update state if the |signal_strategy_| is already connected.
  OnSignalStrategyStateChange(signal_strategy_->GetState());
}

GcdStateUpdater::~GcdStateUpdater() {
  DCHECK(thread_checker_.CalledOnValidThread());
  signal_strategy_->RemoveListener(this);
}

void GcdStateUpdater::SetHostOfflineReason(
    const std::string& host_offline_reason,
    const base::TimeDelta& timeout,
    const base::Callback<void(bool success)>& ack_callback) {
  // TODO(jrw): Implement this.  Refer to
  // XmppHeartbeatSender::SetHostOfflineReason.
  NOTIMPLEMENTED();
}

void GcdStateUpdater::OnSignalStrategyStateChange(SignalStrategy::State state) {
  if (state == SignalStrategy::CONNECTED) {
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromMilliseconds(kTimerIntervalMinMs),
                 base::TimeDelta::FromMilliseconds(kTimerIntervalMaxMs),
                 base::Bind(&GcdStateUpdater::MaybeSendStateUpdate,
                            base::Unretained(this)));
  } else if (state == SignalStrategy::DISCONNECTED) {
    timer_.Stop();
  }
}

bool GcdStateUpdater::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  // Ignore all XMPP stanzas.
  return false;
}

void GcdStateUpdater::OnPatchStateResult(GcdRestClient::Result result) {
  if (!timer_.IsRunning()) {
    return;
  }

  if (result == GcdRestClient::NETWORK_ERROR ||
      pending_request_jid_ != signal_strategy_->GetLocalAddress().id()) {
    // Continue exponential backoff.
    return;
  }

  timer_.Stop();
  if (result == GcdRestClient::SUCCESS) {
    if (!on_update_successful_callback_.is_null()) {
      on_unknown_host_id_error_.Reset();
      std::move(on_update_successful_callback_).Run();
    }
  } else if (result == GcdRestClient::NO_SUCH_HOST) {
    if (!on_unknown_host_id_error_.is_null()) {
      on_update_successful_callback_.Reset();
      std::move(on_unknown_host_id_error_).Run();
    }
  } else {
    // For any other error, do nothing since there's no way to handle
    // it and the error will already have been logged at this point.
  }
}

void GcdStateUpdater::MaybeSendStateUpdate() {
  DCHECK_EQ(signal_strategy_->GetState(), SignalStrategy::CONNECTED);

  // Don't send a request if there is already another request pending.
  // This avoids having multiple outstanding requests, which would be
  // a problem since there's no guarantee that the reqests will
  // complete in order.
  if (gcd_rest_client_->HasPendingRequest()) {
    return;
  }

  // Construct an update to the remote state.
  std::unique_ptr<base::DictionaryValue> patch(new base::DictionaryValue);
  std::unique_ptr<base::DictionaryValue> base_state(new base::DictionaryValue);
  pending_request_jid_ = signal_strategy_->GetLocalAddress().id();
  base_state->SetString("_jabberId", pending_request_jid_);
  base_state->SetString("_hostVersion", STRINGIZE(VERSION));
  patch->Set("base", std::move(base_state));

  // Send the update to GCD.
  gcd_rest_client_->PatchState(
      std::move(patch),
      base::Bind(&GcdStateUpdater::OnPatchStateResult, base::Unretained(this)));
}

}  // namespace remoting
