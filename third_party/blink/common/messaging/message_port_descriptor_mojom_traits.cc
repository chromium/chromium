// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_descriptor_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::MessagePortDescriptorDataView,
                  blink::MessagePortDescriptor>::
    Read(blink::mojom::MessagePortDescriptorDataView data,
         blink::MessagePortDescriptor* output) {
  mojo::ScopedMessagePipeHandle handle = data.TakePipeHandle();
  uint64_t sequence_number = data.sequence_number();
  base::UnguessableToken id;
  if (!data.ReadId(&id))
    return false;

  output->InitializeFromSerializedValues(std::move(handle), id,
                                         sequence_number);
  return true;
}

// static
mojo::ScopedMessagePipeHandle StructTraits<
    blink::mojom::MessagePortDescriptorDataView,
    blink::MessagePortDescriptor>::pipe_handle(blink::MessagePortDescriptor&
                                                   input) {
  return input.TakeHandleForSerialization();
}

// static
base::UnguessableToken StructTraits<
    blink::mojom::MessagePortDescriptorDataView,
    blink::MessagePortDescriptor>::id(blink::MessagePortDescriptor& input) {
  return input.TakeIdForSerialization();
}

// static
uint64_t StructTraits<blink::mojom::MessagePortDescriptorDataView,
                      blink::MessagePortDescriptor>::
    sequence_number(blink::MessagePortDescriptor& input) {
  return input.TakeSequenceNumberForSerialization();
}

}  // namespace mojo
