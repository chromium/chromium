// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_storage_registry.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "storage/browser/blob/blob_entry.h"
#include "storage/browser/test/fake_blob.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace storage {
namespace {

TEST(BlobStorageRegistry, UUIDRegistration) {
  const std::string kBlob1 = "Blob1";
  const std::string kType = "type1";
  const std::string kDisposition = "disp1";
  BlobStorageRegistry registry;

  EXPECT_FALSE(registry.DeleteEntry(kBlob1));
  EXPECT_EQ(0u, registry.blob_count());

  BlobEntry* entry = registry.CreateEntry(kBlob1, kType, kDisposition);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(BlobStatus::PENDING_QUOTA, entry->status());
  EXPECT_EQ(kType, entry->content_type());
  EXPECT_EQ(kDisposition, entry->content_disposition());
  EXPECT_EQ(0u, entry->refcount());

  EXPECT_EQ(entry, registry.GetEntry(kBlob1));
  EXPECT_TRUE(registry.DeleteEntry(kBlob1));
  entry = registry.CreateEntry(kBlob1, kType, kDisposition);

  EXPECT_EQ(1u, registry.blob_count());
}

std::string UUIDFromBlob(mojo::PendingRemote<blink::mojom::Blob> pending_blob) {
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

TEST(BlobStorageRegistry, URLRegistration) {
  const std::string kBlobId1 = "Blob1";
  const std::string kType = "type1";
  const std::string kDisposition = "disp1";
  const std::string kBlobId2 = "Blob2";
  const GURL kURL = GURL("blob://Blob1");
  const GURL kURL2 = GURL("blob://Blob2");

  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeBlob blob1(kBlobId1);
  FakeBlob blob2(kBlobId2);

  BlobStorageRegistry registry;
  EXPECT_FALSE(registry.IsURLMapped(kURL));
  EXPECT_FALSE(registry.GetBlobFromURL(kURL));
  EXPECT_FALSE(registry.DeleteURLMapping(kURL));
  EXPECT_EQ(0u, registry.url_count());

  EXPECT_TRUE(registry.CreateUrlMapping(kURL, blob1.Clone()));
  EXPECT_FALSE(registry.CreateUrlMapping(kURL, blob2.Clone()));

  EXPECT_TRUE(registry.IsURLMapped(kURL));
  EXPECT_EQ(kBlobId1, UUIDFromBlob(registry.GetBlobFromURL(kURL)));
  EXPECT_EQ(1u, registry.url_count());

  EXPECT_TRUE(registry.CreateUrlMapping(kURL2, blob2.Clone()));
  EXPECT_EQ(2u, registry.url_count());
  EXPECT_TRUE(registry.DeleteURLMapping(kURL2));
  EXPECT_FALSE(registry.IsURLMapped(kURL2));

  // Both urls point to the same blob.
  EXPECT_TRUE(registry.CreateUrlMapping(kURL2, blob1.Clone()));
  EXPECT_EQ(UUIDFromBlob(registry.GetBlobFromURL(kURL)),
            UUIDFromBlob(registry.GetBlobFromURL(kURL2)));
}

}  // namespace
}  // namespace storage
