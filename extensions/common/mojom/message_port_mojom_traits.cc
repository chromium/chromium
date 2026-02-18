// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/message_port_mojom_traits.h"

#include "extensions/common/api/messaging/message.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/mojom/base/unguessable_token.mojom.h"
#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"

namespace mojo {

// static
extensions::mojom::MessageDataDataView::Tag UnionTraits<
    extensions::mojom::MessageDataDataView,
    extensions::MessageData>::GetTag(const extensions::MessageData& data) {
  if (std::holds_alternative<std::string>(data)) {
    return extensions::mojom::MessageDataDataView::Tag::kJson;
  }
  return extensions::mojom::MessageDataDataView::Tag::kStructuredMessage;
}

// static
bool UnionTraits<
    extensions::mojom::MessageDataDataView,
    extensions::MessageData>::Read(extensions::mojom::MessageDataDataView data,
                                   extensions::MessageData* out) {
  switch (data.tag()) {
    case extensions::mojom::MessageDataDataView::Tag::kJson: {
      std::string json;
      if (!data.ReadJson(&json)) {
        return false;
      }
      *out = std::move(json);
      return true;
    }
    case extensions::mojom::MessageDataDataView::Tag::kStructuredMessage: {
      extensions::StructuredCloneMessageData structured_message;
      if (!data.ReadStructuredMessage(&structured_message)) {
        return false;
      }
      *out = std::move(structured_message);
      return true;
    }
  }
  return false;
}

bool StructTraits<extensions::mojom::MessageDataView, extensions::Message>::
    Read(extensions::mojom::MessageDataView data, extensions::Message* out) {
  extensions::MessageData message_data;
  if (!data.ReadData(&message_data)) {
    return false;
  }

  *out = extensions::Message(std::move(message_data), data.user_gesture(),
                             data.from_privileged_context());
  return true;
}

bool StructTraits<extensions::mojom::PortIdDataView, extensions::PortId>::Read(
    extensions::mojom::PortIdDataView data,
    extensions::PortId* out) {
  out->serialization_format = data.serialization_format();
  out->is_opener = data.is_opener();
  out->port_number = data.port_number();
  return data.ReadContextId(&out->context_id);
}

bool StructTraits<extensions::mojom::MessagingEndpointDataView,
                  extensions::MessagingEndpoint>::
    Read(extensions::mojom::MessagingEndpointDataView data,
         extensions::MessagingEndpoint* out) {
  out->type = data.type();
  return data.ReadExtensionId(&out->extension_id) &&
         data.ReadNativeAppName(&out->native_app_name);
}

}  // namespace mojo
