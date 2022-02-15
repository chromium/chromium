// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_url_loader_throttle.h"

#include "base/memory/raw_ptr.h"
#include "net/base/network_change_notifier.h"

namespace network {

namespace {

// A SimpleURLLoaderBatcher::Delegate which tries to batch loaders when
// the default network is inactive (e.g. in low power state).
class BatchingDelegate
    : public SimpleURLLoaderThrottle::Delegate,
      public net::NetworkChangeNotifier::DefaultNetworkActiveObserver {
 public:
  explicit BatchingDelegate(SimpleURLLoaderThrottle* owner) : owner_(owner) {}
  ~BatchingDelegate() override {
    if (is_observing_default_network_)
      net::NetworkChangeNotifier::RemoveDefaultNetworkActiveObserver(this);
  }

  bool ShouldThrottle() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (net::NetworkChangeNotifier::IsDefaultNetworkActive())
      return false;

    if (!is_observing_default_network_) {
      net::NetworkChangeNotifier::AddDefaultNetworkActiveObserver(this);
      is_observing_default_network_ = true;
    }

    return true;
  }

 private:
  // net::NetworkChangeNotifier::DefaultNetworkActiveObserver method:
  void OnDefaultNetworkActive() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(is_observing_default_network_);

    owner_->OnReadyToStart();

    net::NetworkChangeNotifier::RemoveDefaultNetworkActiveObserver(this);
    is_observing_default_network_ = false;
  }

  // Must outlive `this`.
  raw_ptr<SimpleURLLoaderThrottle> const owner_;

  bool is_observing_default_network_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

SimpleURLLoaderThrottle::Delegate::Delegate() = default;
SimpleURLLoaderThrottle::Delegate::~Delegate() = default;

SimpleURLLoaderThrottle::SimpleURLLoaderThrottle()
    : delegate_(std::make_unique<BatchingDelegate>(this)) {}

SimpleURLLoaderThrottle::~SimpleURLLoaderThrottle() = default;

void SimpleURLLoaderThrottle::NotifyWhenReady(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(!timeout_timer_.IsRunning());

  if (!delegate_->ShouldThrottle()) {
    std::move(callback).Run();
    return;
  }

  callback_ = std::move(callback);
  // Unretained is safe because `this` owns `timeout_timer_`.
  timeout_timer_.Start(FROM_HERE, timeout_,
                       base::BindOnce(&SimpleURLLoaderThrottle::OnTimeout,
                                      base::Unretained(this)));
}

void SimpleURLLoaderThrottle::OnReadyToStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_);
  timeout_timer_.Stop();
  std::move(callback_).Run();
}

void SimpleURLLoaderThrottle::OnTimeout() {
  OnReadyToStart();
}

void SimpleURLLoaderThrottle::SetDelegateForTesting(
    std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void SimpleURLLoaderThrottle::SetTimeoutForTesting(base::TimeDelta timeout) {
  DCHECK(!timeout_timer_.IsRunning());
  timeout_ = timeout;
}

}  // namespace network
