// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ftl_signaling_connector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/logging.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

namespace {

constexpr base::TimeDelta kBackoffResetDelay = base::Seconds(30);
constexpr base::TimeDelta kNetworkChangeDelay = base::Seconds(5);

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms. (1s)
    1000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,

    // Maximum amount of time we are willing to delay our request in ms. (1m)
    60000,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    false,
};

const char* SignalStrategyErrorToString(SignalStrategy::Error error) {
  switch (error) {
    case SignalStrategy::OK:
      return "OK";
    case SignalStrategy::AUTHENTICATION_FAILED:
      return "AUTHENTICATION_FAILED";
    case SignalStrategy::NETWORK_ERROR:
      return "NETWORK_ERROR";
    case SignalStrategy::PROTOCOL_ERROR:
      return "PROTOCOL_ERROR";
  }
  return "";
}

}  // namespace

FtlSignalingConnector::FtlSignalingConnector(
    SignalStrategy* signal_strategy,
    base::OnceClosure auth_failed_callback)
    : signal_strategy_(signal_strategy),
      auth_failed_callback_(std::move(auth_failed_callback)),
      backoff_(&kBackoffPolicy) {
  DCHECK(signal_strategy_);
  DCHECK(auth_failed_callback_);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  signal_strategy_->AddListener(this);
}

FtlSignalingConnector::~FtlSignalingConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void FtlSignalingConnector::Start() {
  TryReconnect(base::TimeDelta());
}

void FtlSignalingConnector::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state == SignalStrategy::CONNECTED) {
    HOST_LOG << "Signaling connected. New JID: "
             << signal_strategy_->GetLocalAddress().id();
    backoff_reset_timer_.Start(FROM_HERE, kBackoffResetDelay, &backoff_,
                               &net::BackoffEntry::Reset);
  } else if (state == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Signaling disconnected. error="
             << SignalStrategyErrorToString(signal_strategy_->GetError());
    backoff_reset_timer_.AbandonAndStop();
    backoff_.InformOfRequest(false);
    if (signal_strategy_->IsSignInError() &&
        signal_strategy_->GetError() == SignalStrategy::AUTHENTICATION_FAILED) {
      if (auth_failed_callback_) {
        std::move(auth_failed_callback_).Run();
      }
      return;
    }
    TryReconnect(backoff_.GetTimeUntilRelease());
  }
}

bool FtlSignalingConnector::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void FtlSignalingConnector::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Network state changed to online.";
    TryReconnect(kNetworkChangeDelay);
  }
}

void FtlSignalingConnector::TryReconnect(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(FROM_HERE, delay, this, &FtlSignalingConnector::DoReconnect);
}

void FtlSignalingConnector::DoReconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Attempting to reconnect signaling.";
    signal_strategy_->Connect();
  }
}

}  // namespace remoting
