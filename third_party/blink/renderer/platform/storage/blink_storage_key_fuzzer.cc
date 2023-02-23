// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom-shared.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key.h"
#include "third_party/blink/renderer/platform/storage/blink_storage_key_mojom_traits.h"

struct Environment {
  Environment() {
    CHECK(base::i18n::InitializeICU());
    mojo::core::Init();
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
    absl::optional<blink::StorageKey> maybe_storage_key =
        blink::StorageKey::Deserialize(serialized_storage_key);
    if (!maybe_storage_key) {
      // We need a valid storage key to test the MOJOM path.
      return 0;
    }

    // Test mojom conversion path.
    mojo::Message message =
        blink::mojom::StorageKey::SerializeAsMessage(&*maybe_storage_key);
    mojo::ScopedMessageHandle handle = message.TakeMojoMessage();
    message = mojo::Message::CreateFromMessageHandle(&handle);
    blink::BlinkStorageKey mojom_blink_storage_key;
    blink::mojom::blink::StorageKey::DeserializeFromMessage(
        std::move(message), &mojom_blink_storage_key);
    message = blink::mojom::blink::StorageKey::SerializeAsMessage(
        &mojom_blink_storage_key);
    handle = message.TakeMojoMessage();
    message = mojo::Message::CreateFromMessageHandle(&handle);
    blink::StorageKey mojom_storage_key;
    blink::mojom::StorageKey::DeserializeFromMessage(std::move(message),
                                                     &mojom_storage_key);
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
