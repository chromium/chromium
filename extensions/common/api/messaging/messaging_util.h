// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_UTIL_H_
#define EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_UTIL_H_

#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

class Extension;

namespace messaging_util {

// Returns the serialization format (JSON or Structured Clone) that should be
// used for a given message channel.
//
// This is determined statically based on the `channel_type` and extension
// context because the format can be determined from the channel:
// - Native Messaging (`kNative`): Always uses JSON. Native applications (e.g.,
//   Python/C++ hosts) exchange messages via stdin/stdout and standard JSON, and
//   cannot parse Blink's internal Structured Clone format. The extension side
//   of the native messaging channel therefore has to also use JSON.
// - Extension Messaging (`kSendMessage`, `kConnect`): Can use Structured Clone
//   (if enabled) to support rich data types (Blobs, Files, cycles) between V8
//   contexts. If using Structured Clone is not enabled, defaults to JSON.
//   `kSendRequest` is deprecated and is not supported to use Structured Clone.
// - Non-extension messaging: Defaults to JSON.
//
// Using this helper ensures consistency and prevents callers from manually
// selecting an incompatible format (e.g. sending Structured Clone data to a
// native host). It also makes it unnecessary to pass the serialization format
// over mojom.
mojom::SerializationFormat GetSerializationFormat(
    const Extension* extension,
    mojom::ChannelType channel_type);

}  // namespace messaging_util

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_MESSAGING_MESSAGING_UTIL_H_
