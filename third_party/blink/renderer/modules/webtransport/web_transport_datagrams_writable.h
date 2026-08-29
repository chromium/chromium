// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_DATAGRAMS_WRITABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_DATAGRAMS_WRITABLE_H_

#include <stdint.h>

#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ExceptionState;
class ScriptState;
class UnderlyingSinkBase;
class WebTransport;
class WebTransportSendGroup;

class MODULES_EXPORT WebTransportDatagramsWritable final
    : public WritableStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  WebTransportDatagramsWritable(ScriptState*,
                                WebTransport*,
                                WebTransportSendGroup*,
                                int64_t send_order);
  ~WebTransportDatagramsWritable() override;

  void Init(ScriptState*, UnderlyingSinkBase*, ExceptionState&);

  WebTransportSendGroup* sendGroup() const { return send_group_.Get(); }
  void setSendGroup(WebTransportSendGroup*, ExceptionState&);

  int64_t sendOrder() const { return send_order_; }
  void setSendOrder(int64_t send_order) { send_order_ = send_order; }

  void Trace(Visitor*) const override;

 private:
  const Member<WebTransport> transport_;
  Member<WebTransportSendGroup> send_group_;
  int64_t send_order_;
};

template <>
struct DowncastTraits<WebTransportDatagramsWritable> {
  static bool AllowFrom(const WritableStream& stream) {
    return stream.GetWrapperTypeInfo() ==
           WebTransportDatagramsWritable::GetStaticWrapperTypeInfo();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_DATAGRAMS_WRITABLE_H_
