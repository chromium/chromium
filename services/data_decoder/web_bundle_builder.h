// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_WEB_BUNDLE_BUILDER_H_
#define SERVICES_DATA_DECODER_WEB_BUNDLE_BUILDER_H_

#include <vector>

#include "base/files/file.h"
#include "base/strings/string_piece.h"
#include "components/cbor/values.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/data_decoder/public/mojom/resource_snapshot_for_web_bundle.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace data_decoder {

class WebBundleBuilder {
 public:
  using Headers = std::vector<std::pair<std::string, std::string>>;
  struct ResponseLocation {
    // /components/cbor uses int64_t for integer types.
    int64_t offset;
    int64_t length;
  };

  explicit WebBundleBuilder(const std::string& fallback_url);
  ~WebBundleBuilder();

  WebBundleBuilder(const WebBundleBuilder&) = delete;
  WebBundleBuilder& operator=(const WebBundleBuilder&) = delete;

  std::vector<uint8_t> CreateBundle(
      std::vector<mojom::SerializedResourceInfoPtr> resources,
      std::vector<absl::optional<mojo_base::BigBuffer>> bodies);

 private:
  void SetExchanges(std::vector<mojom::SerializedResourceInfoPtr> resources,
                    std::vector<absl::optional<mojo_base::BigBuffer>> bodies);
  void AddIndexEntry(base::StringPiece url,
                     base::StringPiece variants_value,
                     std::vector<ResponseLocation> response_locations);
  void AddSection(base::StringPiece name, cbor::Value section);
  void WriteBundleLength(uint8_t bundle_length);
  std::vector<uint8_t> CreateTopLevel();

  std::string fallback_url_;
  cbor::Value::ArrayValue section_lengths_;
  cbor::Value::ArrayValue sections_;
  cbor::Value::MapValue index_;
  cbor::Value::ArrayValue responses_;
};
}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_WEB_BUNDLE_BUILDER_H_
