// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string serialized_storage_key(reinterpret_cast<const char*>(data), size);
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    // General deserialization test.
    std::optional<blink::StorageKey> maybe_storage_key =
        blink::StorageKey::Deserialize(serialized_storage_key);
    if (maybe_storage_key) {
      assert(maybe_storage_key->Serialize() == serialized_storage_key);
    }

    // LocalStorage deserialization test.
    maybe_storage_key =
        blink::StorageKey::DeserializeForLocalStorage(serialized_storage_key);
    if (maybe_storage_key) {
      assert(maybe_storage_key->SerializeForLocalStorage() ==
             serialized_storage_key);
    }
  }
  return 0;
}
