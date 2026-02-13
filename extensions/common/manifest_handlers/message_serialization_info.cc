// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/message_serialization_info.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

MessageSerializationInfo::MessageSerializationInfo(
    bool opts_in_structured_clone)
    : opts_in_structured_clone(opts_in_structured_clone) {}

MessageSerializationInfo::~MessageSerializationInfo() = default;

// static
bool MessageSerializationInfo::UsesStructuredClone(const Extension* extension) {
  MessageSerializationInfo* info = static_cast<MessageSerializationInfo*>(
      extension->GetManifestData(manifest_keys::kMessageSerialization));
  bool is_opted_in = info && info->opts_in_structured_clone;
  return is_opted_in &&
         base::FeatureList::IsEnabled(
             extensions_features::kStructuredCloningForMessaging);
}

MessageSerializationHandler::MessageSerializationHandler() = default;

MessageSerializationHandler::~MessageSerializationHandler() = default;

bool MessageSerializationHandler::Parse(Extension* extension,
                                        std::u16string* error) {
  const base::Value* serialization_value =
      extension->manifest()->FindPath(manifest_keys::kMessageSerialization);

  if (!serialization_value) {
    return true;
  }

  if (!serialization_value->is_string()) {
    *error = base::UTF8ToUTF16(
        std::string_view(manifest_errors::kInvalidMessageSerialization));
    return false;
  }

  const std::string& serialization_str = serialization_value->GetString();
  bool opts_in_structured_clone = false;
  if (serialization_str ==
      manifest_values::kMessageSerializationStructuredClone) {
    opts_in_structured_clone = true;
  } else if (serialization_str == manifest_values::kMessageSerializationJson) {
    opts_in_structured_clone = false;
  } else {
    *error = base::UTF8ToUTF16(
        std::string_view(manifest_errors::kInvalidMessageSerialization));
    return false;
  }

  extension->SetManifestData(
      manifest_keys::kMessageSerialization,
      std::make_unique<MessageSerializationInfo>(opts_in_structured_clone));
  return true;
}

base::span<const char* const> MessageSerializationHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kMessageSerialization};
  return kKeys;
}

}  // namespace extensions
