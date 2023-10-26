// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/message_port_mojom_traits.h"

#include "mojo/public/mojom/base/unguessable_token.mojom.h"

namespace mojo {

bool StructTraits<extensions::mojom::MessageDataView, extensions::Message>::
    Read(extensions::mojom::MessageDataView data, extensions::Message* out) {
  out->format = data.format();
  out->user_gesture = data.user_gesture();
  out->from_privileged_context = data.from_privileged_context();
  return data.ReadData(&out->data);
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
