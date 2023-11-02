// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PENDING_CALLBACK_CHAIN_H_
#define SERVICES_NETWORK_PENDING_CALLBACK_CHAIN_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"

namespace network {

// Helper class to keep track of multiple functions which may return
// net::ERR_IO_PENDING and call a net::CompletionOnceCallback, or return another
// net error synchronously, and not call the completion callback. If there are
// one or more pending results added, the original completion callback will not
// be called until all those results have completed. If there are multiple error
// results that are different, net::ERR_FAILED will be used.
class COMPONENT_EXPORT(NETWORK_SERVICE) PendingCallbackChain
    : public base::RefCounted<PendingCallbackChain> {
 public:
  explicit PendingCallbackChain(net::CompletionOnceCallback complete);

  // Creates a callback that can be called to decrement the wait count if a
  // net::ERR_IO_PENDING result is added with AddResult().
  net::CompletionOnceCallback CreateCallback();

  // Adds a result to the chain. If the result is net::ERR_IO_PENDING,
  // a corresponding callback will need to be called before the original
  // completion callback is called.
  void AddResult(int result);

  // Gets the current result of the chain. This will be net::ERR_IO_PENDING if
  // there are any pending results.
  int GetResult() const;

 private:
  friend class base::RefCounted<PendingCallbackChain>;
  ~PendingCallbackChain();

  void CallbackComplete(int result);
  void SetResult(int result);

  int num_waiting_ = 0;
  int final_result_ = net::OK;
  net::CompletionOnceCallback complete_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PENDING_CALLBACK_CHAIN_H_
