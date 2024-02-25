// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/proto/lpm_interface.h"

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/storage_key/proto/storage_key.pb.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_proto_converter.h"

struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

DEFINE_PROTO_FUZZER(const storage_key_proto::StorageKey& storage_key_proto) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);

    blink::StorageKey storage_key = Convert(storage_key_proto);

    // General serialization test.
    std::optional<blink::StorageKey> maybe_storage_key =
        blink::StorageKey::Deserialize(storage_key.Serialize());
    assert(storage_key == maybe_storage_key.value());

    // LocalStorage serialization test.
    maybe_storage_key = blink::StorageKey::DeserializeForLocalStorage(
        storage_key.SerializeForLocalStorage());
    assert(storage_key == maybe_storage_key.value());
  }
}
