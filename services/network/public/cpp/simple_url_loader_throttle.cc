// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/simple_url_loader_throttle.h"

namespace network {

namespace {

// A SimpleURLLoaderBatcher::Delegate which always disallow batching.
class NoBatchingDelegate : public SimpleURLLoaderThrottle::Delegate {
 public:
  NoBatchingDelegate() = default;
  ~NoBatchingDelegate() override = default;

  bool ShouldThrottle() override { return false; }
};

}  // namespace

SimpleURLLoaderThrottle::Delegate::Delegate() = default;
SimpleURLLoaderThrottle::Delegate::~Delegate() = default;

// TODO(https://crbug.com/1293657): Create an appropriate delegate for each
// platform.
SimpleURLLoaderThrottle::SimpleURLLoaderThrottle()
    : delegate_(std::make_unique<NoBatchingDelegate>()) {}

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
