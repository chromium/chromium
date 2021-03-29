// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_HOST_ADDRESS_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_HOST_ADDRESS_REQUEST_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "net/base/ip_address.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/rtc_base/async_resolver_interface.h"

namespace blink {

class P2PSocketDispatcher;

// P2PAsyncAddressResolver performs DNS hostname resolution. It's used
// to resolve addresses of STUN and relay servers.
class P2PAsyncAddressResolver
    : public base::RefCountedThreadSafe<P2PAsyncAddressResolver> {
 public:
  using DoneCallback = base::OnceCallback<void(const Vector<net::IPAddress>&)>;

  P2PAsyncAddressResolver(P2PSocketDispatcher* dispatcher);
  // Start address resolve process.
  void Start(const rtc::SocketAddress& addr, DoneCallback done_callback);
  // Clients must unregister before exiting for cleanup.
  void Cancel();

 private:
  enum State {
    STATE_CREATED,
    STATE_SENT,
    STATE_FINISHED,
  };

  friend class P2PSocketDispatcher;

  friend class base::RefCountedThreadSafe<P2PAsyncAddressResolver>;

  virtual ~P2PAsyncAddressResolver();

  void OnResponse(const Vector<net::IPAddress>& address);

  P2PSocketDispatcher* dispatcher_;
  THREAD_CHECKER(thread_checker_);

  // State must be accessed from delegate thread only.
  State state_;
  DoneCallback done_callback_;

  DISALLOW_COPY_AND_ASSIGN(P2PAsyncAddressResolver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_P2P_HOST_ADDRESS_REQUEST_H_
