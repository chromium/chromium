// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/host_resolver_cache.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/libfuzzer/proto/json.pb.h"
#include "testing/libfuzzer/proto/json_proto_converter.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace net {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider data_provider(data, size);

  size_t cache_size = data_provider.ConsumeIntegral<size_t>();
  if (cache_size == 0) {
    return 0;
  }

  // Either consume a JSON proto string to maximize base::Value compatibility or
  // a bare string to maximize fuzzing.
  std::string json_string;
  if (data_provider.ConsumeBool()) {
    std::vector<uint8_t> bytes = data_provider.ConsumeRemainingBytes<uint8_t>();

    json_proto::JsonValue proto;
    if (!protobuf_mutator::libfuzzer::LoadProtoInput(
            /*binary=*/false, bytes.data(), bytes.size(), &proto)) {
      return 0;
    }

    json_string = json_proto::JsonProtoConverter().Convert(proto);
  } else {
    json_string = data_provider.ConsumeRemainingBytesAsString();
  }

  std::optional<base::Value> value = base::JSONReader::Read(json_string);
  if (!value.has_value()) {
    return 0;
  }

  HostResolverCache cache(cache_size);
  if (!cache.RestoreFromValue(value.value())) {
    return 0;
  }

  base::Value reserialized = cache.Serialize();

  // If at max size, may not have deserialized all data out of the fuzzed input.
  if (cache.AtMaxSizeForTesting()) {
    return 0;
  }

  CHECK_EQ(reserialized, value.value());

  return 0;
}

}  // namespace net
