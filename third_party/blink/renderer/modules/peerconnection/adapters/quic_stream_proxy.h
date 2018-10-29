// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_STREAM_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_STREAM_PROXY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"

namespace blink {

class QuicStreamHost;
class QuicTransportProxy;

// This class allows interactions with a QUIC stream that runs on a thread
// different from which it is controlled. All interactions with the QUIC
// implementation happen asynchronously.
//
// The QuicStreamProxy is owned by the QuicTransportProxy and constructed when
// either a new local QUIC stream is created or when a remote QUIC stream has
// been created. The stream proxy will be deleted in the following
// circumstances:
// 1) Reset() is called.
// 2) OnRemoteReset() is indicated.
// 3) Finish() and OnRemoteFinish() have been called.
// The client is responsible for knowing when any of these conditions have been
// met and clearing its reference accordingly.
//
// Since the QuicStreamProxy can be constructed from either the proxy or host
// thread, initialization happens in four steps:
// 1) QuicStreamProxy is constructed.
// 2) set_host is called with a WeakPtr to the corresponding host-thread object.
// 3) Initialize is called on the proxy thread.
// 4) set_delegate is called on the proxy thread.
class QuicStreamProxy final : public base::SupportsWeakPtr<QuicStreamProxy> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the remote side resets the stream.
    virtual void OnRemoteReset() {}
    // Called when the remote side finishes the stream.
    virtual void OnRemoteFinish() {}
  };

  QuicStreamProxy();
  ~QuicStreamProxy();

  // Sets a WeakPtr to the corresponding QuicStreamHost. This is valid on either
  // the proxy or host thread. Should happen right after construction.
  void set_host(base::WeakPtr<QuicStreamHost> stream_host);

  // Initializes the QuicStreamProxy. Must be called on the proxy thread.
  // |transport_proxy| must outlive this object.
  void Initialize(QuicTransportProxy* transport_proxy);

  // Sets the delegate for receiving remote callbacks.
  void set_delegate(Delegate* delegate);

  // The remaining methods can only be called from the proxy thread and must
  // be preceded by Initialize().

  scoped_refptr<base::SingleThreadTaskRunner> host_thread() const;

  void Reset();
  void Finish();

 private:
  // Instruct the QuicTransportProxy to remove and delete this stream proxy.
  void Delete();

  // Callbacks from QuicStreamHost.
  friend class QuicStreamHost;
  void OnRemoteReset();
  void OnRemoteFinish();

  // Up reference. Owned by the QuicTransportProxy client.
  QuicTransportProxy* transport_proxy_ = nullptr;
  // Forward reference. Owned by the QuicTransportHost.
  base::WeakPtr<QuicStreamHost> stream_host_;
  // Back reference. Owned by the RTCQuicTransport.
  Delegate* delegate_ = nullptr;

  // |readable_| transitions to false when OnRemoteFinish() is called.
  bool readable_ = true;
  // |writeable_| transitions to false when Finish() is called.
  bool writeable_ = true;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_STREAM_PROXY_H_
