// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-shared.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key_mojom_traits.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"

struct Environment {
  Environment() {
    CHECK(base::i18n::InitializeICU());
    mojo::core::Init();
    WTF::Partitions::Initialize();
  }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

Environment* env = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string serialized_storage_key(reinterpret_cast<const char*>(data), size);
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning, toggle);
    std::optional<blink::StorageKey> maybe_storage_key =
        blink::StorageKey::Deserialize(serialized_storage_key);
    if (!maybe_storage_key) {
      // We need a valid storage key to test the MOJOM path.
      return 0;
    }

    // Test mojom conversion path.
    std::vector<uint8_t> mojom_serialized =
        blink::mojom::StorageKey::Serialize(&*maybe_storage_key);
    WTF::Vector<uint8_t> mojom_serialized_as_wtf;
    mojom_serialized_as_wtf.AppendRange(mojom_serialized.begin(),
                                        mojom_serialized.end());
    blink::BlinkStorageKey mojom_blink_storage_key;
    assert(blink::mojom::blink::StorageKey::Deserialize(
        mojom_serialized_as_wtf, &mojom_blink_storage_key));
    WTF::Vector<uint8_t> mojom_blink_serialized =
        blink::mojom::blink::StorageKey::Serialize(&mojom_blink_storage_key);
    std::vector<uint8_t> mojom_blink_serialized_as_std(
        mojom_blink_serialized.begin(), mojom_blink_serialized.end());
    blink::StorageKey mojom_storage_key;
    assert(blink::mojom::StorageKey::Deserialize(mojom_blink_serialized_as_std,
                                                 &mojom_storage_key));
    assert(maybe_storage_key->ExactMatchForTesting(mojom_storage_key));

    // Test type conversion path.
    blink::BlinkStorageKey type_blink_storage_key(*maybe_storage_key);
    blink::StorageKey type_storage_key(type_blink_storage_key);
    assert(maybe_storage_key->ExactMatchForTesting(type_storage_key));

    // Each path should reach the same answers.
    assert(
        mojom_blink_storage_key.ExactMatchForTesting(type_blink_storage_key));
  }
  return 0;
}
