// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_MESSAGE_PORT_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_MESSAGE_PORT_MOJOM_TRAITS_H_

#include <string>

#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::MessageDataView, extensions::Message> {
  static const std::string& data(const extensions::Message& message) {
    return message.data;
  }

  static extensions::mojom::SerializationFormat format(
      const extensions::Message& message) {
    return message.format;
  }

  static bool user_gesture(const extensions::Message& message) {
    return message.user_gesture;
  }

  static bool from_privileged_context(const extensions::Message& message) {
    return message.from_privileged_context;
  }

  static bool Read(extensions::mojom::MessageDataView data,
                   extensions::Message* out);
};

template <>
struct StructTraits<extensions::mojom::PortIdDataView, extensions::PortId> {
  static const base::UnguessableToken& context_id(
      const extensions::PortId& port_id) {
    return port_id.context_id;
  }

  static int port_number(const extensions::PortId& port_id) {
    return port_id.port_number;
  }

  static bool is_opener(const extensions::PortId& port_id) {
    return port_id.is_opener;
  }

  static extensions::mojom::SerializationFormat serialization_format(
      const extensions::PortId& port_id) {
    return port_id.serialization_format;
  }

  static bool Read(extensions::mojom::PortIdDataView data,
                   extensions::PortId* out);
};

template <>
struct StructTraits<extensions::mojom::MessagingEndpointDataView,
                    extensions::MessagingEndpoint> {
  static std::optional<std::string> native_app_name(
      const extensions::MessagingEndpoint& endpoint) {
    return endpoint.native_app_name;
  }

  static std::optional<std::string> extension_id(
      const extensions::MessagingEndpoint& endpoint) {
    return endpoint.extension_id;
  }

  static extensions::MessagingEndpoint::Type type(
      const extensions::MessagingEndpoint& endpoint) {
    return endpoint.type;
  }

  static bool Read(extensions::mojom::MessagingEndpointDataView data,
                   extensions::MessagingEndpoint* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_MESSAGE_PORT_MOJOM_TRAITS_H_
