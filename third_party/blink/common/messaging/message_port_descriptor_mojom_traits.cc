// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/message_port_descriptor_mojom_traits.h"

#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"

namespace blink {

// Helper class that provides access to blink::MessagePortDescriptor internals.
class MessagePortSerializationAccess {
 public:
  static void Init(blink::MessagePortDescriptor* output,
                   mojo::ScopedMessagePipeHandle&& handle,
                   base::UnguessableToken id,
                   uint64_t sequence_number) {
    output->Init(std::move(handle), id, sequence_number);
  }

  static mojo::ScopedMessagePipeHandle TakeHandle(
      blink::MessagePortDescriptor& input) {
    return input.TakeHandle();
  }

  static base::UnguessableToken TakeId(blink::MessagePortDescriptor& input) {
    return input.TakeId();
  }

  static uint64_t TakeSequenceNumber(blink::MessagePortDescriptor& input) {
    return input.TakeSequenceNumber();
  }
};

}  // namespace blink

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

  blink::MessagePortSerializationAccess::Init(output, std::move(handle), id,
                                              sequence_number);
  return true;
}

// static
mojo::ScopedMessagePipeHandle StructTraits<
    blink::mojom::MessagePortDescriptorDataView,
    blink::MessagePortDescriptor>::pipe_handle(blink::MessagePortDescriptor&
                                                   input) {
  return blink::MessagePortSerializationAccess::TakeHandle(input);
}

// static
base::UnguessableToken StructTraits<
    blink::mojom::MessagePortDescriptorDataView,
    blink::MessagePortDescriptor>::id(blink::MessagePortDescriptor& input) {
  return blink::MessagePortSerializationAccess::TakeId(input);
}

// static
uint64_t StructTraits<blink::mojom::MessagePortDescriptorDataView,
                      blink::MessagePortDescriptor>::
    sequence_number(blink::MessagePortDescriptor& input) {
  return blink::MessagePortSerializationAccess::TakeSequenceNumber(input);
}

}  // namespace mojo
