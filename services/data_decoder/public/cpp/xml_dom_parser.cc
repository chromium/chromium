// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>

#include "base/containers/span_rust.h"
#include "base/strings/string_view_rust.h"
#include "services/data_decoder/public/cpp/xml_dom.h"
#include "services/data_decoder/xml/cxx.rs.h"

namespace data_decoder::xml {

std::optional<Document> Document::FromBytes(base::span<const uint8_t> bytes) {
  auto root = ffi::decode_xml_bytes(base::SpanToRustSlice(bytes));
  return root ? std::optional(Document(std::move(root))) : std::nullopt;
}

std::optional<Document> Document::FromUtf8(std::string_view str) {
  auto root = ffi::decode_xml_str(base::StringViewToRustStrUTF8(str));
  return root ? std::optional(Document(std::move(root))) : std::nullopt;
}

}  // namespace data_decoder::xml
