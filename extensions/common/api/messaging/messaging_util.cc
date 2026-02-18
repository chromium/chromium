// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/messaging/messaging_util.h"

#include "base/feature_list.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/message_serialization_info.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions::messaging_util {

mojom::SerializationFormat GetSerializationFormat(
    const Extension* extension,
    mojom::ChannelType channel_type) {
  if (!extension) {
    // TODO(crbug.com/40321352): Document when this is possible.
    return mojom::SerializationFormat::kJson;
  }

  switch (channel_type) {
    case mojom::ChannelType::kSendMessage:
    case mojom::ChannelType::kConnect:
      if (MessageSerializationInfo::UsesStructuredClone(extension)) {
        return mojom::SerializationFormat::kStructuredClone;
      }
      return mojom::SerializationFormat::kJson;
    case mojom::ChannelType::kNative:
      // Native messaging hosts (external processes) generally only support JSON
      // parsing. They do not have access to the Blink's engine to deserialize
      // Structured Clone data.
    case mojom::ChannelType::kSendRequest:
      // `kSendRequest` is a deprecated channel type that is not supported to
      // use Structured Clone data.
      return mojom::SerializationFormat::kJson;
    default:
      NOTREACHED();
  }
}

}  // namespace extensions::messaging_util
