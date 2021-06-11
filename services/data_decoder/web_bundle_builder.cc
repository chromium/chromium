// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/web_bundle_builder.h"

#include "base/big_endian.h"
#include "components/cbor/writer.h"

namespace data_decoder {

namespace {

// TODO(myrzakereyms): replace this method with cbor::writer::GetNumUintBytes.
uint64_t GetNumUintBytes(uint64_t value) {
  if (value < 24) {
    return 0;
  } else if (value <= 0xFF) {
    return 1;
  } else if (value <= 0xFFFF) {
    return 2;
  } else if (value <= 0xFFFFFFFF) {
    return 4;
  }
  return 8;
}

uint64_t GetEncodedByteSizeOfString(uint64_t size) {
  return 1 + GetNumUintBytes(size);
}

uint64_t GetEncodedByteSizeOfHeaders(const WebBundleBuilder::Headers& headers) {
  uint64_t byte_size = 1 + GetNumUintBytes(headers.size());
  for (const auto& header : headers) {
    byte_size +=
        GetEncodedByteSizeOfString(header.first.size()) + header.first.size() +
        GetEncodedByteSizeOfString(header.second.size()) + header.second.size();
  }
  return byte_size;
}

uint64_t GetEncodedByteSizeOfResponse(const WebBundleBuilder::Headers& headers,
                                      uint64_t body_size) {
  uint64_t encoded_header_map_size = GetEncodedByteSizeOfHeaders(headers);
  return 1 /* size of header of array(2) */ +
         GetEncodedByteSizeOfString(encoded_header_map_size) +
         encoded_header_map_size + GetEncodedByteSizeOfString(body_size) +
         body_size;
}

cbor::Value CreateByteString(base::StringPiece s) {
  return cbor::Value(base::as_bytes(base::make_span(s)));
}

cbor::Value CreateHeaderMap(const WebBundleBuilder::Headers& headers) {
  cbor::Value::MapValue map;
  for (const auto& pair : headers)
    map.insert({CreateByteString(pair.first), CreateByteString(pair.second)});
  return cbor::Value(std::move(map));
}

std::vector<uint8_t> Encode(const cbor::Value& value) {
  return *cbor::Writer::Write(value);
}

int64_t EncodedLength(const cbor::Value& value) {
  return Encode(value).size();
}
}  // namespace

WebBundleBuilder::WebBundleBuilder(const std::string& fallback_url)
    : fallback_url_(fallback_url) {}

WebBundleBuilder::~WebBundleBuilder() = default;

void WebBundleBuilder::SetExchanges(
    std::vector<mojom::SerializedResourceInfoPtr> resources,
    std::vector<absl::optional<mojo_base::BigBuffer>> bodies) {
  CHECK_EQ(resources.size(), bodies.size());
  int64_t responses_offset = 1 + GetNumUintBytes(resources.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    const auto& info = resources[i];
    const auto& body = bodies[i];
    Headers headers = {{":status", "200"}, {"content-type", info->mime_type}};
    uint64_t response_length =
        GetEncodedByteSizeOfResponse(headers, body ? body->size() : 0);
    ResponseLocation location = {responses_offset,
                                 static_cast<int64_t>(response_length)};
    responses_offset += response_length;
    cbor::Value::ArrayValue response_array;
    response_array.emplace_back(Encode(CreateHeaderMap(headers)));
    response_array.emplace_back(CreateByteString(
        body ? base::StringPiece(reinterpret_cast<const char*>(body->data()),
                                 body->size())
             : ""));
    cbor::Value response(response_array);
    responses_.emplace_back(std::move(response));
    GURL url = info->url;
    GURL::Replacements replacements;
    replacements.ClearRef();
    url = url.ReplaceComponents(replacements);
    AddIndexEntry(url.spec(), "", {location});
  }
}

void WebBundleBuilder::AddIndexEntry(
    base::StringPiece url,
    base::StringPiece variants_value,
    std::vector<ResponseLocation> response_locations) {
  cbor::Value::ArrayValue index_value_array;
  index_value_array.emplace_back(CreateByteString(variants_value));
  for (const auto& location : response_locations) {
    index_value_array.emplace_back(location.offset);
    index_value_array.emplace_back(location.length);
  }
  index_.insert({cbor::Value(url), cbor::Value(index_value_array)});
}

void WebBundleBuilder::AddSection(base::StringPiece name, cbor::Value section) {
  section_lengths_.emplace_back(name);
  section_lengths_.emplace_back(EncodedLength(section));
  sections_.emplace_back(std::move(section));
}

std::vector<uint8_t> WebBundleBuilder::CreateBundle(
    std::vector<mojom::SerializedResourceInfoPtr> resources,
    std::vector<absl::optional<mojo_base::BigBuffer>> bodies) {
  SetExchanges(std::move(resources), std::move(bodies));
  AddSection("index", cbor::Value(index_));
  AddSection("responses", cbor::Value(responses_));
  return CreateTopLevel();
}

std::vector<uint8_t> WebBundleBuilder::CreateTopLevel() {
  cbor::Value::ArrayValue toplevel_array;
  toplevel_array.emplace_back(
      CreateByteString(u8"\U0001F310\U0001F4E6"));  // "🌐📦"
  toplevel_array.emplace_back(CreateByteString(base::StringPiece("b1\0\0", 4)));
  toplevel_array.emplace_back(cbor::Value(fallback_url_));
  toplevel_array.emplace_back(Encode(cbor::Value(section_lengths_)));
  toplevel_array.emplace_back(sections_);
  // Put a dummy 8-byte bytestring.
  toplevel_array.emplace_back(cbor::Value::BinaryValue(8, 0));

  std::vector<uint8_t> bundle = Encode(cbor::Value(toplevel_array));
  char encoded[8];
  base::WriteBigEndian(encoded, static_cast<uint64_t>(bundle.size()));
  // Overwrite the dummy bytestring with the actual size.
  memcpy(bundle.data() + bundle.size() - 8, encoded, 8);

  return bundle;
}
}  // namespace data_decoder
