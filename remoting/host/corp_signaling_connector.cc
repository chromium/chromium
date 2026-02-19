// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/corp_signaling_connector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "remoting/base/logging.h"

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

CorpSignalingConnector::CorpSignalingConnector(SignalStrategy* signal_strategy)
    : signal_strategy_(signal_strategy), backoff_(&kBackoffPolicy) {
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  signal_strategy_->AddListener(this);
}

CorpSignalingConnector::~CorpSignalingConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void CorpSignalingConnector::Start() {
  TryReconnect(base::TimeDelta());
}

void CorpSignalingConnector::OnSignalStrategyStateChange(
    SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state == SignalStrategy::CONNECTED) {
    HOST_LOG << "Corp signaling connected.";
    backoff_reset_timer_.Start(FROM_HERE, kBackoffResetDelay, &backoff_,
                               &net::BackoffEntry::Reset);
  } else if (state == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Corp signaling disconnected. Error: "
             << SignalStrategyErrorToString(signal_strategy_->GetError());
    backoff_reset_timer_.Stop();
    backoff_.InformOfRequest(false);
    TryReconnect(backoff_.GetTimeUntilRelease());
  }
}

void CorpSignalingConnector::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE &&
      signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Network state changed to online.";
    TryReconnect(kNetworkChangeDelay);
  }
}

void CorpSignalingConnector::TryReconnect(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.Start(FROM_HERE, delay, this, &CorpSignalingConnector::DoReconnect);
}

void CorpSignalingConnector::DoReconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (signal_strategy_->GetState() == SignalStrategy::DISCONNECTED) {
    HOST_LOG << "Attempting to reconnect corp signaling.";
    signal_strategy_->Connect();
  }
}

}  // namespace remoting
