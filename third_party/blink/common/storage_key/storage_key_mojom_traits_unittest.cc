// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"

#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/storage_key.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
namespace {

TEST(StorageKeyMojomTraitsTest, SerializeAndDeserialize) {
  StorageKey test_keys[] = {
      StorageKey(url::Origin::Create(GURL("https://example.com"))),
      StorageKey(url::Origin::Create(GURL("http://example.com"))),
      StorageKey(url::Origin::Create(GURL("https://example.test"))),
      StorageKey(url::Origin::Create(GURL("https://sub.example.com"))),
      StorageKey(url::Origin::Create(GURL("http://sub2.example.com"))),
      StorageKey(url::Origin()),
      StorageKey::CreateWithNonce(
          url::Origin::Create(GURL("https://.example.com")),
          base::UnguessableToken::Create()),
      StorageKey::CreateWithNonce(url::Origin(),
                                  base::UnguessableToken::Create()),
  };

  for (auto& original : test_keys) {
    StorageKey copied;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StorageKey>(original,
                                                                       copied));
    EXPECT_EQ(original, copied);
  }
}

}  // namespace
}  // namespace blink
