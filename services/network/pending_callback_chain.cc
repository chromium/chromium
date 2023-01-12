// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/pending_callback_chain.h"
#include "base/functional/bind.h"

namespace network {

PendingCallbackChain::PendingCallbackChain(net::CompletionOnceCallback complete)
    : complete_(std::move(complete)) {}

PendingCallbackChain::~PendingCallbackChain() {}

net::CompletionOnceCallback PendingCallbackChain::CreateCallback() {
  return base::BindOnce(&PendingCallbackChain::CallbackComplete, this);
}

void PendingCallbackChain::AddResult(int result) {
  if (result == net::ERR_IO_PENDING)
    num_waiting_++;
  else
    SetResult(result);
}

int PendingCallbackChain::GetResult() const {
  if (num_waiting_ > 0)
    return net::ERR_IO_PENDING;
  return final_result_;
}

void PendingCallbackChain::CallbackComplete(int result) {
  DCHECK_GT(num_waiting_, 0);
  SetResult(result);
  num_waiting_--;
  if (num_waiting_ == 0)
    std::move(complete_).Run(final_result_);
}

void PendingCallbackChain::SetResult(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (final_result_ == net::OK) {
    final_result_ = result;
  } else if (result != net::OK && result != final_result_) {
    // If we have two non-OK results, default to ERR_FAILED.
    final_result_ = net::ERR_FAILED;
  }
}

}  // namespace network
