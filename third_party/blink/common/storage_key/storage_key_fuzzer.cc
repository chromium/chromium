// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/lpm_interface.h"

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "third_party/blink/common/storage_key/storage_key_fuzzer.pb.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_proto_converter.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

DEFINE_PROTO_FUZZER(
    const storage_key_proto::StorageKeyFuzzer& storage_key_fuzzer) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  blink::StorageKey storage_key = Convert(storage_key_fuzzer.storage_key());

  std::string result = storage_key.Serialize();
  absl::optional<blink::StorageKey> maybe_storage_key =
      blink::StorageKey::Deserialize(result);
  if (maybe_storage_key) {
    assert(storage_key == maybe_storage_key.value());
  }

  // TODO(crbug.com/1270350): This could be a little closer to the serialization
  // format
  blink::StorageKey::Deserialize(storage_key_fuzzer.deserialize());
}
