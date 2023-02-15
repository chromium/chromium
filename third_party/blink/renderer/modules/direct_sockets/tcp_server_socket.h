// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SERVER_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SERVER_SOCKET_H_

#include "third_party/blink/renderer/modules/direct_sockets/socket.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;
class TCPServerReadableStreamWrapper;
class TCPServerSocketOptions;

class MODULES_EXPORT TCPServerSocket final : public ScriptWrappable,
                                             public Socket {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // IDL definitions
  static TCPServerSocket* Create(ScriptState*,
                                 const String& local_address,
                                 const TCPServerSocketOptions*,
                                 ExceptionState&);

  // Socket:
  ScriptPromise close(ScriptState*, ExceptionState&) override;

 public:
  explicit TCPServerSocket(ScriptState*);
  ~TCPServerSocket() override;

  void Trace(Visitor*) const override;

  // Socket:
  void ContextDestroyed() override {}

 private:
  Member<TCPServerReadableStreamWrapper> readable_stream_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SERVER_SOCKET_H_
