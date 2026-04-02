// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_SEND_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_SEND_GROUP_H_

#include <stdint.h>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ScriptState;
class WebTransport;
class WebTransportSendStreamStats;

// Implements the WebTransportSendGroup interface.
// https://w3c.github.io/webtransport/#webtransportsendgroup
//
// A send group allows grouping multiple WebTransport streams for scheduling
// purposes. Streams within the same group are prioritized by sendOrder, while
// different groups are multiplexed fairly (round-robin).
class MODULES_EXPORT WebTransportSendGroup final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // WebTransportSendGroup is not constructed from JavaScript. It is only
  // created via WebTransport::createSendGroup().
  WebTransportSendGroup(WebTransport* transport, uint32_t group_id);
  ~WebTransportSendGroup() override;

  // IDL implementation.
  ScriptPromise<WebTransportSendStreamStats> getStats(ScriptState*);

  // Returns the internal group identifier, used for mapping to QUICHE's
  // SendGroupId when setting stream priorities.
  uint32_t group_id() const { return group_id_; }

  void Trace(Visitor* visitor) const override;

 private:
  const Member<WebTransport> transport_;
  const uint32_t group_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_SEND_GROUP_H_
