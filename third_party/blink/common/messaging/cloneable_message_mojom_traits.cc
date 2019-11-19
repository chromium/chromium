// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"

#include "base/containers/span.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

mojo_base::BigBufferView StructTraits<
    blink::mojom::CloneableMessage::DataView,
    blink::CloneableMessage>::encoded_message(blink::CloneableMessage& input) {
  return mojo_base::BigBufferView(input.encoded_message);
}

bool StructTraits<blink::mojom::CloneableMessage::DataView,
                  blink::CloneableMessage>::
    Read(blink::mojom::CloneableMessage::DataView data,
         blink::CloneableMessage* out) {
  mojo_base::BigBufferView message_view;
  base::Optional<base::UnguessableToken> locked_agent_cluster_id;
  if (!data.ReadEncodedMessage(&message_view) || !data.ReadBlobs(&out->blobs) ||
      !data.ReadLockedAgentClusterId(&locked_agent_cluster_id) ||
      !data.ReadSenderOrigin(&out->sender_origin) ||
      !data.ReadNativeFileSystemTokens(&out->native_file_system_tokens)) {
    return false;
  }

  auto message_bytes = message_view.data();
  out->owned_encoded_message = {message_bytes.begin(), message_bytes.end()};
  out->encoded_message = out->owned_encoded_message;
  out->stack_trace_id = data.stack_trace_id();
  out->stack_trace_debugger_id_first = data.stack_trace_debugger_id_first();
  out->stack_trace_debugger_id_second = data.stack_trace_debugger_id_second();
  out->stack_trace_should_pause = data.stack_trace_should_pause();
  out->locked_agent_cluster_id = locked_agent_cluster_id;
  return true;
}

}  // namespace mojo
