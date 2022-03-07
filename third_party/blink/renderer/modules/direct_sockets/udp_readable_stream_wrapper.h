// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_READABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_READABLE_STREAM_WRAPPER_H_

#include "base/callback_forward.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ReadableStream;

class MODULES_EXPORT UDPReadableStreamWrapper final
    : public GarbageCollected<UDPReadableStreamWrapper> {
 public:
  UDPReadableStreamWrapper(ScriptState* script_state,
                           const Member<UDPSocketMojoRemote> udp_socket,
                           base::OnceClosure on_close);
  ~UDPReadableStreamWrapper();

  ReadableStream* Readable() const { return readable_; }

  // Forwards incoming datagrams to UnderlyingSource::AcceptDatagram.
  void AcceptDatagram(base::span<const uint8_t> data,
                      const net::IPEndPoint& src_addr);

  // Called by UDPSocket::DoClose(). Forwards close request to
  // UnderlyingSource::Close().
  void Close();

  void Trace(Visitor* visitor) const;

 private:
  class UnderlyingSource;
  friend class UnderlyingSource;

  // Called by UnderlyingSource::Close() or UnderlyingSource::cancel()
  // (depending on whether the close request came from the reader or from the
  // socket itself). Executes close callback (on_close_).
  void CloseInternal();

  // Fetch kNumAdditionalDiagrams on every AcceptDatagram call.
  constexpr static const uint32_t kNumAdditionalDatagrams = 5;

  const Member<ScriptState> script_state_;

  const Member<UDPSocketMojoRemote> udp_socket_;
  base::OnceClosure on_close_;

  Member<UDPReadableStreamWrapper::UnderlyingSource> source_;
  Member<ReadableStream> readable_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_UDP_READABLE_STREAM_WRAPPER_H_
