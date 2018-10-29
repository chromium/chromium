// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_STREAM_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_STREAM_HOST_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream.h"

namespace blink {

class QuicStreamProxy;
class QuicTransportHost;

// This class is the host side correspondent to the QuicStreamProxy. See the
// QuicStreamProxy documentation for background. This class lives on the host
// thread and proxies calls between the QuicStreamProxy and the P2PQuicStream
// (which is single-threaded).
//
// The QuicStreamHost is owned by the QuicTransportHost and constructed when
// either a new local QUIC stream is created or when a remote QUIC stream has
// been created. The stream host will be deleted in the following circumstances:
// 1) Reset() is called.
// 2) OnRemoteReset() is indicated.
// 3) Finish() and OnRemoteFinish() have been called.
// The QuicStreamHost will instruct the QuicTransportHost to delete it when any
// condition has been met.
//
// Since the QuicStreamHost can be constructed from either the proxy or host
// thread, initialization happens in three steps:
// 1) QuicStreamHost is constructed.
// 2) set_proxy is called when a WeakPtr to the corresponding proxy-thread
//    object.
// 3) Initialize is called on the host thread.
class QuicStreamHost final : public base::SupportsWeakPtr<QuicStreamHost>,
                             public P2PQuicStream::Delegate {
 public:
  QuicStreamHost();
  ~QuicStreamHost() override;

  // Sets a WeakPtr to the corresponding QuicStreamProxy. This is valid on
  // either the proxy or host thread. Should happen right after construction.
  void set_proxy(base::WeakPtr<QuicStreamProxy> stream_proxy);

  // Initializes the QuicStreamHost. Must be called on the host thread.
  // |transport_host| must outlive this object.
  void Initialize(QuicTransportHost* transport_host, P2PQuicStream* p2p_stream);

  // The remaining methods can only be called from the host thread and must be
  // preceded by Initialize().

  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread() const;

  void Reset();
  void Finish();

 private:
  // Instruct the QuicTransportHost to remove and delete this stream host.
  void Delete();

  // P2PQuicStream::Delegate overrides.
  void OnRemoteReset() override;
  void OnRemoteFinish() override;

  // Up reference. Owned by QuicTransportProxy.
  QuicTransportHost* transport_host_ = nullptr;
  // Forward reference. Owned by P2PQuicTransport.
  P2PQuicStream* p2p_stream_ = nullptr;
  // Back reference. Owned by QuicTransportProxy.
  base::WeakPtr<QuicStreamProxy> stream_proxy_;

  // |readable_| transitions to false when OnRemoteFinish() is called.
  bool readable_ = true;
  // |writeable_| transitions to false when Finish() is called.
  bool writeable_ = true;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_STREAM_HOST_H_
