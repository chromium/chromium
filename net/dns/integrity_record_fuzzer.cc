// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/strings/string_piece.h"
#include "net/dns/record_rdata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

namespace {

base::StringPiece MakeStringPiece(const std::vector<uint8_t>& vec) {
  return base::StringPiece(reinterpret_cast<const char*>(vec.data()),
                           vec.size());
}

// For arbitrary data, check that parse(data).serialize() == data.
void ParseThenSerializeProperty(const std::vector<uint8_t>& data) {
  auto parsed = IntegrityRecordRdata::Create(MakeStringPiece(data));
  CHECK(parsed);
  absl::optional<std::vector<uint8_t>> maybe_serialized = parsed->Serialize();
  // Since |data| is chosen by a fuzzer, the record's digest is unlikely to
  // match its nonce. As a result, |parsed->IsIntact()| may be false, and thus
  // |parsed->Serialize()| may be |absl::nullopt|.
  CHECK_EQ(parsed->IsIntact(), !!maybe_serialized);
  if (maybe_serialized) {
    CHECK(data == *maybe_serialized);
  }
}

// For arbitrary IntegrityRecordRdata r, check that parse(r.serialize()) == r.
void SerializeThenParseProperty(const std::vector<uint8_t>& data) {
  // Ensure that the nonce is not too long to be serialized.
  if (data.size() > std::numeric_limits<uint16_t>::max()) {
    // Property is vacuously true because the record is not serializable.
    return;
  }
  // Build an IntegrityRecordRdata by treating |data| as a nonce.
  IntegrityRecordRdata record(data);
  CHECK(record.IsIntact());
  absl::optional<std::vector<uint8_t>> maybe_serialized = record.Serialize();
  CHECK(maybe_serialized.has_value());

  // Parsing |serialized| always produces a record identical to the original.
  auto parsed =
      IntegrityRecordRdata::Create(MakeStringPiece(*maybe_serialized));
  CHECK(parsed);
  CHECK(parsed->IsIntact());
  CHECK(parsed->IsEqual(&record));
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  const std::vector<uint8_t> data_vec(data, data + size);
  ParseThenSerializeProperty(data_vec);
  SerializeThenParseProperty(data_vec);
  // Construct a random IntegrityRecordRdata to exercise that code path. No need
  // to exercise parse/serialize since we already did that with |data|.
  IntegrityRecordRdata rand_record(IntegrityRecordRdata::Random());
  return 0;
}

}  // namespace net
