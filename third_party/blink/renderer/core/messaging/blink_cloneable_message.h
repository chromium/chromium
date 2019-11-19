// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_CLONEABLE_MESSAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_CLONEABLE_MESSAGE_H_

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "v8/include/v8-inspector.h"

namespace blink {

// This struct represents messages as they are posted over a broadcast channel.
// This type can be serialized as a blink::mojom::CloneableMessage struct.
// This is the renderer-side equivalent of blink::MessagePortMessage, where this
// struct uses blink types, while the other struct uses std:: types.
struct CORE_EXPORT BlinkCloneableMessage {
  BlinkCloneableMessage();
  ~BlinkCloneableMessage();

  BlinkCloneableMessage(BlinkCloneableMessage&&);
  BlinkCloneableMessage& operator=(BlinkCloneableMessage&&);

  scoped_refptr<blink::SerializedScriptValue> message;
  scoped_refptr<const blink::SecurityOrigin> sender_origin;
  v8_inspector::V8StackTraceId sender_stack_trace_id;
  base::Optional<base::UnguessableToken> locked_agent_cluster_id;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlinkCloneableMessage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_CLONEABLE_MESSAGE_H_
