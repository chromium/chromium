// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_registry.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "storage/browser/test/fake_blob.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {
namespace {

std::string UuidFromBlob(mojo::PendingRemote<blink::mojom::Blob> pending_blob) {
  mojo::Remote<blink::mojom::Blob> blob(std::move(pending_blob));

  base::RunLoop loop;
  std::string received_uuid;
  blob->GetInternalUUID(base::BindOnce(
      [](base::OnceClosure quit_closure, std::string* uuid_out,
         const std::string& uuid) {
        *uuid_out = uuid;
        std::move(quit_closure).Run();
      },
      loop.QuitClosure(), &received_uuid));
  loop.Run();
  return received_uuid;
}

enum class PartitionedBlobUrlTestCase {
  kPartitioningDisabled,
  kPartitioningEnabled,
};

class BlobUrlRegistryTestP
    : public testing::Test,
      public testing::WithParamInterface<PartitionedBlobUrlTestCase> {
 public:
  void SetUp() override {
    test_case_ = GetParam();
    InitializeScopedFeatureList();
  }

  void InitializeScopedFeatureList() {
    scoped_feature_list_.InitWithFeatureState(
        net::features::kThirdPartyStoragePartitioning,
        StoragePartitioningEnabled());
  }

  bool StoragePartitioningEnabled() {
    switch (test_case_) {
      case PartitionedBlobUrlTestCase::kPartitioningEnabled:
        return true;
      default:
        return false;
    }
  }

 private:
  PartitionedBlobUrlTestCase test_case_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(BlobUrlRegistryTestP, URLRegistration) {
  const std::string kBlobId1 = "Blob1";
  const std::string kType = "type1";
  const std::string kDisposition = "disp1";
  const std::string kBlobId2 = "Blob2";
  const GURL kURL1 = GURL("blob://Blob1");
  const GURL kURL2 = GURL("blob://Blob2");
  base::UnguessableToken kTokenId1 = base::UnguessableToken::Create();
  base::UnguessableToken kTokenId2 = base::UnguessableToken::Create();
  net::SchemefulSite kTopLevelSite1 =
      net::SchemefulSite(GURL("https://example.com"));
  net::SchemefulSite kTopLevelSite2 =
      net::SchemefulSite(GURL("https://foobar.com"));
  const blink::StorageKey storageKey1 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kURL1));
  const blink::StorageKey storageKey2 =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kURL2));

  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeBlob blob1(kBlobId1);
  FakeBlob blob2(kBlobId2);

  BlobUrlRegistry registry;
  EXPECT_FALSE(registry.IsUrlMapped(kURL1, storageKey1));
  EXPECT_FALSE(registry.GetBlobFromUrl(kURL1));
  EXPECT_FALSE(registry.RemoveUrlMapping(kURL1, storageKey1));
  EXPECT_EQ(0u, registry.url_count());

  EXPECT_TRUE(registry.AddUrlMapping(kURL1, blob1.Clone(), storageKey1,
                                     storageKey1.origin(), /*rph_id=*/0,
                                     kTokenId1, kTopLevelSite1));
  EXPECT_FALSE(registry.AddUrlMapping(kURL1, blob2.Clone(), storageKey1,
                                      storageKey1.origin(), /*rph_id=*/0,
                                      kTokenId1, kTopLevelSite1));
  EXPECT_EQ(kTokenId1, registry.GetUnsafeAgentClusterID(kURL1));
  EXPECT_EQ(kTopLevelSite1, registry.GetUnsafeTopLevelSite(kURL1));

  EXPECT_TRUE(registry.IsUrlMapped(kURL1, storageKey1));
  EXPECT_EQ(kBlobId1, UuidFromBlob(registry.GetBlobFromUrl(kURL1)));
  EXPECT_TRUE(registry.GetBlobFromUrl(kURL1));
  EXPECT_EQ(1u, registry.url_count());

  EXPECT_TRUE(registry.AddUrlMapping(kURL2, blob2.Clone(), storageKey2,
                                     storageKey2.origin(), /*rph_id=*/0,
                                     kTokenId2, kTopLevelSite2));
  EXPECT_EQ(kTokenId2, registry.GetUnsafeAgentClusterID(kURL2));
  EXPECT_EQ(kTopLevelSite2, registry.GetUnsafeTopLevelSite(kURL2));
  EXPECT_EQ(2u, registry.url_count());
  EXPECT_TRUE(registry.RemoveUrlMapping(kURL2, storageKey2));
  EXPECT_FALSE(registry.IsUrlMapped(kURL2, storageKey2));
  EXPECT_EQ(std::nullopt, registry.GetUnsafeAgentClusterID(kURL2));
  EXPECT_EQ(std::nullopt, registry.GetUnsafeTopLevelSite(kURL2));

  // Both urls point to the same blob.
  EXPECT_TRUE(registry.AddUrlMapping(kURL2, blob1.Clone(), storageKey2,

                                     storageKey2.origin(), /*rph_id=*/0,
                                     kTokenId2, kTopLevelSite2));
  EXPECT_EQ(kTokenId2, registry.GetUnsafeAgentClusterID(kURL2));
  EXPECT_EQ(kTopLevelSite2, registry.GetUnsafeTopLevelSite(kURL2));
  EXPECT_EQ(UuidFromBlob(registry.GetBlobFromUrl(kURL1)),
            UuidFromBlob(registry.GetBlobFromUrl(kURL2)));

  EXPECT_TRUE(registry.RemoveUrlMapping(kURL2, storageKey2));

  // Test using a storage key that doesn't correspond to the Blob URL.
  EXPECT_NE(storageKey1, storageKey2);
  EXPECT_FALSE(registry.IsUrlMapped(kURL1, storageKey2));
  EXPECT_FALSE(registry.RemoveUrlMapping(kURL1, storageKey2));
  EXPECT_TRUE(registry.IsUrlMapped(kURL1, storageKey1));
  EXPECT_TRUE(registry.RemoveUrlMapping(kURL1, storageKey1));

  EXPECT_EQ(0u, registry.url_count());

  // Now do some tests with third-party storage keys>
  if (StoragePartitioningEnabled()) {
    blink::StorageKey partitionedStorageKey1 =
        blink::StorageKey::Create(url::Origin::Create(kURL1), kTopLevelSite1,
                                  blink::mojom::AncestorChainBit::kCrossSite);
    blink::StorageKey partitionedStorageKey2 =
        blink::StorageKey::Create(url::Origin::Create(kURL1), kTopLevelSite2,
                                  blink::mojom::AncestorChainBit::kCrossSite);

    EXPECT_TRUE(
        registry.AddUrlMapping(kURL1, blob1.Clone(), partitionedStorageKey1,
                               partitionedStorageKey1.origin(), /*rph_id=*/0,
                               kTokenId1, kTopLevelSite1));
    EXPECT_TRUE(registry.IsUrlMapped(kURL1, partitionedStorageKey1));
    EXPECT_EQ(kBlobId1, UuidFromBlob(registry.GetBlobFromUrl(kURL1)));
    EXPECT_TRUE(registry.GetBlobFromUrl(kURL1));

    EXPECT_FALSE(registry.IsUrlMapped(kURL1, partitionedStorageKey2));
    EXPECT_FALSE(registry.RemoveUrlMapping(kURL1, partitionedStorageKey2));
    EXPECT_TRUE(registry.IsUrlMapped(kURL1, partitionedStorageKey1));
    EXPECT_TRUE(registry.RemoveUrlMapping(kURL1, partitionedStorageKey1));
  }
  EXPECT_EQ(0u, registry.url_count());
}

INSTANTIATE_TEST_SUITE_P(
    BlobUrlRegistryTests,
    BlobUrlRegistryTestP,
    ::testing::Values(PartitionedBlobUrlTestCase::kPartitioningDisabled,
                      PartitionedBlobUrlTestCase::kPartitioningEnabled));

}  // namespace
}  // namespace storage
