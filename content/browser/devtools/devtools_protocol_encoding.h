// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_

#include <memory>
#include "third_party/inspector_protocol/crdtp/json.h"

// Convenience adaptations of the conversion functions
// crdtp::json::ConvertCBORToJSON crdtp::json::ConvertJDONToCBOR.
// These adaptations use an implementation of crdtp::json::Platform that
// uses base/strings/string_number_conversions.h.
namespace content {

crdtp::Status ConvertCBORToJSON(crdtp::span<uint8_t> cbor, std::string* json);

crdtp::Status ConvertJSONToCBOR(crdtp::span<uint8_t> json, std::string* cbor);

crdtp::Status ConvertJSONToCBOR(crdtp::span<uint8_t> json,
                                std::vector<uint8_t>* cbor);
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_ENCODING_H_
