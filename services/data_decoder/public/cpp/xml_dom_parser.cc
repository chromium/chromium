// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/containers/span_rust.h"
#include "base/strings/string_view_rust.h"
#include "services/data_decoder/public/cpp/xml_dom.h"
#include "services/data_decoder/xml/cxx.rs.h"

namespace data_decoder::xml {

base::expected<Document, std::string> Document::FromBytes(
    base::span<const uint8_t> bytes) {
  std::string err;
  auto root = ffi::decode_xml_bytes(base::SpanToRustSlice(bytes), err);
  if (root) {
    return Document(std::move(root));
  }
  return base::unexpected(err);
}

base::expected<Document, std::string> Document::FromUtf8(std::string_view str) {
  std::string err;
  auto root = ffi::decode_xml_str(base::StringViewToRustStrUTF8(str), err);
  if (root) {
    return Document(std::move(root));
  }
  return base::unexpected(err);
}

}  // namespace data_decoder::xml
