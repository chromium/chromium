// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_MESSAGE_SERIALIZATION_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_MESSAGE_SERIALIZATION_INFO_H_

#include <string>

#include "base/check.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the parsed message serialization info.
struct MessageSerializationInfo : public Extension::ManifestData {
  explicit MessageSerializationInfo(bool opts_in_structured_clone);
  ~MessageSerializationInfo() override;

  // Returns true if the extension should use structured clone message
  // serialization (the extension has opted in and the feature is enabled).
  static bool UsesStructuredClone(const Extension* extension);

  // Whether the extension has indicated in its manifest that it would like to
  // use structured clone for its message serialization. Use
  // `UsesStructuredClone()` to determine if an extension should.
  bool opts_in_structured_clone;
};

// Parses the "message_serialization" manifest key.
class MessageSerializationHandler : public ManifestHandler {
 public:
  MessageSerializationHandler();
  MessageSerializationHandler(const MessageSerializationHandler&) = delete;
  MessageSerializationHandler& operator=(const MessageSerializationHandler&) =
      delete;
  ~MessageSerializationHandler() override;

  // ManifestHandler:
  bool Parse(Extension* extension, std::u16string* error) override;
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_MESSAGE_SERIALIZATION_INFO_H_
