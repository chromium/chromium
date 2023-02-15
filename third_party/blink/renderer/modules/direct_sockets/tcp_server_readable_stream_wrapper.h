// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SERVER_READABLE_STREAM_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SERVER_READABLE_STREAM_WRAPPER_H_

#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class MODULES_EXPORT TCPServerReadableStreamWrapper
    : public GarbageCollected<TCPServerReadableStreamWrapper>,
      public ReadableStreamDefaultWrapper {
 public:
  explicit TCPServerReadableStreamWrapper(ScriptState*);

  // ReadableStreamDefaultWrapper:
  void Pull() override;
  void CloseStream() override;
  void ErrorStream(int32_t error_code) override;
  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_TCP_SERVER_READABLE_STREAM_WRAPPER_H_
