// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/blob/features.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::BlobURLStore;

namespace storage {

namespace {

enum class PartitionedBlobUrlTestCase {
  kPartitioningDisabled,
  kBlockCrossPartitionBlobUrlFetchingEnabled,
  kBlockCrossPartitionBlobUrlFetchingDisabled,
};

class BlobURLStoreImplTestP
    : public testing::Test,
      public testing::WithParamInterface<PartitionedBlobUrlTestCase> {
 public:
  void SetUp() override {
    test_case_ = GetParam();
    InitializeScopedFeatureList();

    context_ = std::make_unique<BlobStorageContext>();

    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BlobURLStoreImplTestP::OnBadMessage, base::Unretained(this)));
  }

  void InitializeScopedFeatureList() {
    std::vector<base::test::FeatureRef> enabled_features{};
    std::vector<base::test::FeatureRef> disabled_features{};

    if (BlockCrossPartitionBlobUrlFetchingEnabled()) {
      enabled_features.push_back(features::kBlockCrossPartitionBlobUrlFetching);
    } else {
      disabled_features.push_back(
          features::kBlockCrossPartitionBlobUrlFetching);
    }

    if (StoragePartitioningEnabled()) {
      enabled_features.push_back(net::features::kThirdPartyStoragePartitioning);
    } else {
      disabled_features.push_back(
          net::features::kThirdPartyStoragePartitioning);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool BlockCrossPartitionBlobUrlFetchingEnabled() {
    switch (test_case_) {
      case PartitionedBlobUrlTestCase::
          kBlockCrossPartitionBlobUrlFetchingEnabled:
        return true;
      case PartitionedBlobUrlTestCase::
          kBlockCrossPartitionBlobUrlFetchingDisabled:
      default:
        return false;
    }
  }

  bool StoragePartitioningEnabled() {
    return test_case_ == PartitionedBlobUrlTestCase::
                             kBlockCrossPartitionBlobUrlFetchingEnabled;
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  mojo::PendingRemote<blink::mojom::Blob> CreateBlobFromString(
      const std::string& contents) {
    std::string uuid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    auto builder = std::make_unique<BlobDataBuilder>(uuid);
    builder->set_content_type("text/plain");
    builder->AppendData(contents);
    mojo::PendingRemote<blink::mojom::Blob> blob;
    BlobImpl::Create(context_->AddFinishedBlob(std::move(builder)),
                     blob.InitWithNewPipeAndPassReceiver());
    return blob;
  }

  std::string UUIDFromBlob(blink::mojom::Blob* blob) {
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

  mojo::PendingRemote<BlobURLStore> CreateURLStore() {
    mojo::PendingRemote<BlobURLStore> result;
    url_registry_.AddReceiver(
        kStorageKey, kStorageKey.origin(), /*render_process_host_id=*/0,
        result.InitWithNewPipeAndPassReceiver(),
        /*context_type_for_debugging=*/"",
        /*storage_key_debug_string_callback=*/
        base::BindRepeating([]() -> std::string { return ""; }));
    return result;
  }

  GURL RegisterURL(BlobURLStore* store,
                   mojo::PendingRemote<blink::mojom::Blob> blob,
                   bool security_origin_serializes_as_null = false) {
    base::test::TestFuture<const GURL&> registered_url;
    store->Register(std::move(blob), security_origin_serializes_as_null,
                    registered_url.GetCallback());
    return registered_url.Take();
  }

  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const blink::StorageKey kStorageKey =
      blink::StorageKey::CreateFirstParty(kOrigin);
  const GURL kValidUrl = GURL("blob:" + kOrigin.Serialize() + "/id1");
  const GURL kInvalidUrl = GURL("bolb:id");
  const GURL kFragmentUrl = GURL(kValidUrl.spec() + "#fragment");
  const url::Origin kWrongOrigin =
      url::Origin::Create(GURL("https://test.com"));
  const GURL kWrongOriginUrl = GURL("blob:" + kWrongOrigin.Serialize() + "/id");
  const net::SchemefulSite kWrongTopLevelSite =
      net::SchemefulSite(kWrongOriginUrl);

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  PartitionedBlobUrlTestCase test_case_;
  std::unique_ptr<BlobStorageContext> context_;
  BlobUrlRegistry url_registry_;
  std::vector<std::string> bad_messages_;
};

TEST_P(BlobURLStoreImplTestP, BasicRegisterRevoke) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  // Register a URL and make sure the URL keeps the blob alive.
  BlobURLStoreImpl url_store(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                             url_registry_.AsWeakPtr());
  const GURL url = RegisterURL(&url_store, std::move(blob));
  EXPECT_EQ(kOrigin, url::Origin::Create(url));

  blob = url_registry_.GetBlobFromUrl(url);
  ASSERT_TRUE(blob);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob));
  std::string id = UUIDFromBlob(blob_remote.get());
  blob_remote.reset();

  // Revoke the URL.
  url_store.Revoke(url);
  blob = url_registry_.GetBlobFromUrl(url);
  EXPECT_FALSE(blob);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(id));
}

TEST_P(BlobURLStoreImplTestP, LocalOriginSerialization) {
  const url::Origin file_origin =
      url::Origin::Create(GURL("file:///example.txt"));
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(file_origin);
  BlobURLStoreImpl url_store(storage_key, file_origin, /*rph_id=*/0,
                             url_registry_.AsWeakPtr());

  const GURL serialized_url =
      RegisterURL(&url_store, CreateBlobFromString("hello world"));
  EXPECT_EQ(file_origin, url::Origin::Create(serialized_url));
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(serialized_url));

  const GURL null_url =
      RegisterURL(&url_store, CreateBlobFromString("hello world"),
                  /*security_origin_serializes_as_null=*/true);
  EXPECT_EQ("null", url::Origin::Create(null_url).Serialize());
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(null_url));
}

TEST_P(BlobURLStoreImplTestP, NonLocalOriginCannotForceNullSerialization) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());

  const GURL url =
      RegisterURL(url_store.get(), CreateBlobFromString("hello world"),
                  /*security_origin_serializes_as_null=*/true);
  url_store.FlushForTesting();

  EXPECT_FALSE(url_registry_.GetBlobFromUrl(url));
  ASSERT_EQ(1u, bad_messages_.size());
  EXPECT_EQ("URL with invalid origin passed to BlobURLStore::Register",
            bad_messages_[0]);
}

TEST_P(BlobURLStoreImplTestP, ImplicitRevoke) {
  mojo::Remote<blink::mojom::Blob> blob(CreateBlobFromString("hello world"));
  mojo::PendingRemote<blink::mojom::Blob> blob2;
  blob->Clone(blob2.InitWithNewPipeAndPassReceiver());

  auto url_store = std::make_unique<BlobURLStoreImpl>(
      kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
      url_registry_.AsWeakPtr());
  const GURL url = RegisterURL(url_store.get(), blob.Unbind());
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(url));
  const GURL url2 = RegisterURL(url_store.get(), std::move(blob2));
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(url2));
  EXPECT_NE(url, url2);

  // Destroy URL Store, should revoke URLs.
  url_store = nullptr;
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(url));
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(url2));
}

TEST_P(BlobURLStoreImplTestP, RevokeThroughDifferentURLStore) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  BlobURLStoreImpl url_store1(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                              url_registry_.AsWeakPtr());
  BlobURLStoreImpl url_store2(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                              url_registry_.AsWeakPtr());

  const GURL url = RegisterURL(&url_store1, std::move(blob));
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(url));

  url_store2.Revoke(url);
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(url));
}

TEST_P(BlobURLStoreImplTestP, RevokeInvalidScheme) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  url_store->Revoke(kInvalidUrl);
  url_store.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_P(BlobURLStoreImplTestP, RevokeWrongOrigin) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  url_store->Revoke(kWrongOriginUrl);
  url_store.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_P(BlobURLStoreImplTestP, RevokeURLWithFragment) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  url_store->Revoke(kFragmentUrl);
  url_store.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_P(BlobURLStoreImplTestP, RevokeWrongStorageKey) {
  const blink::StorageKey kWrongStorageKey = blink::StorageKey::Create(
      kOrigin, kWrongTopLevelSite, blink::mojom::AncestorChainBit::kCrossSite);

  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  BlobURLStoreImpl url_store1(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                              url_registry_.AsWeakPtr());
  BlobURLStoreImpl url_store2(kWrongStorageKey, kWrongStorageKey.origin(),
                              /*rph_id=*/0, url_registry_.AsWeakPtr());

  const GURL url = RegisterURL(&url_store1, std::move(blob));
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(url));

  url_store2.Revoke(url);
  if (StoragePartitioningEnabled()) {
    EXPECT_TRUE(url_registry_.GetBlobFromUrl(url));
  } else {
    // The storage keys are either the same or are ignored by the Revoke call.
    EXPECT_FALSE(url_registry_.GetBlobFromUrl(url));
  }
}

TEST_P(BlobURLStoreImplTestP, ResolveAsURLLoaderFactoryNonExistentURL) {
  BlobURLStoreImpl url_store(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                             url_registry_.AsWeakPtr());
  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  url_store.ResolveAsURLLoaderFactory(kValidUrl,
                                      factory.BindNewPipeAndPassReceiver());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kValidUrl;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop download_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(),
      base::BindLambdaForTesting([&](std::optional<std::string> response_body) {
        download_loop.Quit();
        EXPECT_FALSE(response_body);
      }));
  download_loop.Run();
}
TEST_P(BlobURLStoreImplTestP, ResolveAsURLLoaderFactoryInvalidURL) {
  BlobURLStoreImpl url_store(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                             url_registry_.AsWeakPtr());
  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  url_store.ResolveAsURLLoaderFactory(kInvalidUrl,
                                      factory.BindNewPipeAndPassReceiver());
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kInvalidUrl;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop download_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(),
      base::BindLambdaForTesting([&](std::optional<std::string> response_body) {
        download_loop.Quit();
        EXPECT_FALSE(response_body);
      }));
  download_loop.Run();
}

TEST_P(BlobURLStoreImplTestP, ResolveAsURLLoaderFactory) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  BlobURLStoreImpl url_store(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                             url_registry_.AsWeakPtr());
  const GURL url = RegisterURL(&url_store, std::move(blob));

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  url_store.ResolveAsURLLoaderFactory(url,
                                      factory.BindNewPipeAndPassReceiver());

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop download_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(),
      base::BindLambdaForTesting([&](std::optional<std::string> response_body) {
        download_loop.Quit();
        ASSERT_TRUE(response_body);
        EXPECT_EQ("hello world", *response_body);
      }));
  download_loop.Run();
}

TEST_P(BlobURLStoreImplTestP,
       ResolveAsURLLoaderFactoryWithSeparateStorageKeys) {
  const blink::StorageKey kWrongStorageKey = blink::StorageKey::Create(
      kOrigin, kWrongTopLevelSite, blink::mojom::AncestorChainBit::kCrossSite);

  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  BlobURLStoreImpl url_store1(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                              url_registry_.AsWeakPtr());
  BlobURLStoreImpl url_store2(kWrongStorageKey, kStorageKey.origin(),
                              /*rph_id=*/0, url_registry_.AsWeakPtr());

  const GURL url = RegisterURL(&url_store1, std::move(blob));

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  url_store2.ResolveAsURLLoaderFactory(url,
                                       factory.BindNewPipeAndPassReceiver());

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop download_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(),
      base::BindLambdaForTesting([&](std::optional<std::string> response_body) {
        download_loop.Quit();
        if (BlockCrossPartitionBlobUrlFetchingEnabled()) {
          EXPECT_FALSE(response_body);
        } else {
          ASSERT_TRUE(response_body);
          EXPECT_EQ("hello world", *response_body);
        }
      }));
  download_loop.Run();
}

TEST_P(BlobURLStoreImplTestP, ResolveAsURLLoaderFactoryWithFragmentUrl) {
  const blink::StorageKey kWrongStorageKey = blink::StorageKey::Create(
      kOrigin, kWrongTopLevelSite, blink::mojom::AncestorChainBit::kCrossSite);

  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  BlobURLStoreImpl url_store1(kWrongStorageKey, kStorageKey.origin(),
                              /*rph_id=*/0, url_registry_.AsWeakPtr());
  mojo::Remote<BlobURLStore> url_store2(CreateURLStore());
  const GURL url = RegisterURL(url_store2.get(), std::move(blob));
  const GURL fragment_url(url.spec() + "#fragment");

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  url_store1.ResolveAsURLLoaderFactory(fragment_url,
                                       factory.BindNewPipeAndPassReceiver());

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = fragment_url;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop download_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(),
      base::BindLambdaForTesting([&](std::optional<std::string> response_body) {
        download_loop.Quit();
        if (BlockCrossPartitionBlobUrlFetchingEnabled()) {
          EXPECT_FALSE(response_body);
        } else {
          ASSERT_TRUE(response_body);
          EXPECT_EQ("hello world", *response_body);
        }
      }));
  download_loop.Run();
}

TEST_P(BlobURLStoreImplTestP, ResolveForNavigation) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString("hello world");

  BlobURLStoreImpl url_store(kStorageKey, kStorageKey.origin(), /*rph_id=*/0,
                             url_registry_.AsWeakPtr());
  const GURL url = RegisterURL(&url_store, std::move(blob));

  mojo::Remote<blink::mojom::BlobURLToken> token_remote;
  url_store.ResolveAsBlobURLToken(url,
                                  token_remote.BindNewPipeAndPassReceiver(),
                                  /*is_top_level_navigation=*/false);

  base::UnguessableToken token;
  base::RunLoop loop;
  token_remote->GetToken(base::BindLambdaForTesting(
      [&](const base::UnguessableToken& received_token) {
        token = received_token;
        loop.Quit();
      }));
  loop.Run();

  GURL blob_url;
  EXPECT_TRUE(url_registry_.GetTokenMapping(token, &blob_url, &blob));
  EXPECT_EQ(url, blob_url);
}

INSTANTIATE_TEST_SUITE_P(
    BlobURLStoreImplTests,
    BlobURLStoreImplTestP,
    testing::Values(
        PartitionedBlobUrlTestCase::kPartitioningDisabled,
        PartitionedBlobUrlTestCase::kBlockCrossPartitionBlobUrlFetchingDisabled,
        PartitionedBlobUrlTestCase::kBlockCrossPartitionBlobUrlFetchingEnabled),
    [](const testing::TestParamInfo<PartitionedBlobUrlTestCase>& info) {
      switch (info.param) {
        case PartitionedBlobUrlTestCase::kPartitioningDisabled:
          return "PartitioningDisabled";
        case PartitionedBlobUrlTestCase::
            kBlockCrossPartitionBlobUrlFetchingDisabled:
          return "BlockCrossPartitionBlobUrlFetchingDisabled";
        case PartitionedBlobUrlTestCase::
            kBlockCrossPartitionBlobUrlFetchingEnabled:
          return "BlockCrossPartitionBlobUrlFetchingEnabled";
      }
    });

}  // namespace
}  // namespace storage
