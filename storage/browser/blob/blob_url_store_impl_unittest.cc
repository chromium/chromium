// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::BlobURLStore;

namespace storage {

class BlobURLStoreImplTest : public testing::Test {
 public:
  void SetUp() override {
    context_ = std::make_unique<BlobStorageContext>();
    agent_cluster_id_ = base::UnguessableToken::Create();

    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &BlobURLStoreImplTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  void OnBadMessage(const std::string& error) {
    bad_messages_.push_back(error);
  }

  mojo::PendingRemote<blink::mojom::Blob> CreateBlobFromString(
      const std::string& uuid,
      const std::string& contents) {
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
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<BlobURLStoreImpl>(kOrigin, url_registry_.AsWeakPtr()),
        result.InitWithNewPipeAndPassReceiver());
    return result;
  }

  void RegisterURL(BlobURLStore* store,
                   mojo::PendingRemote<blink::mojom::Blob> blob,
                   const GURL& url) {
    base::RunLoop loop;
    store->Register(std::move(blob), url, agent_cluster_id_,
                    net::SchemefulSite(), loop.QuitClosure());
    loop.Run();
  }

  mojo::PendingRemote<blink::mojom::Blob> ResolveURL(BlobURLStore* store,
                                                     const GURL& url) {
    mojo::PendingRemote<blink::mojom::Blob> result;
    base::RunLoop loop;
    store->Resolve(kValidUrl,
                   base::BindOnce(
                       [](base::OnceClosure done,
                          mojo::PendingRemote<blink::mojom::Blob>* blob_out,
                          const base::UnguessableToken& agent_registered,
                          mojo::PendingRemote<blink::mojom::Blob> blob,
                          const absl::optional<base::UnguessableToken>&
                              unsafe_agent_cluster_id) {
                         if (blob)
                           EXPECT_EQ(agent_registered, unsafe_agent_cluster_id);
                         *blob_out = std::move(blob);
                         std::move(done).Run();
                       },
                       loop.QuitClosure(), &result, agent_cluster_id_));
    loop.Run();
    return result;
  }

  const std::string kId = "id";
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.com"));
  const GURL kValidUrl = GURL("blob:" + kOrigin.Serialize() + "/id1");
  const GURL kValidUrl2 = GURL("blob:" + kOrigin.Serialize() + "/id2");
  const GURL kInvalidUrl = GURL("bolb:id");
  const GURL kFragmentUrl = GURL(kValidUrl.spec() + "#fragment");
  const url::Origin kWrongOrigin =
      url::Origin::Create(GURL("https://test.com"));
  const GURL kWrongOriginUrl = GURL("blob:" + kWrongOrigin.Serialize() + "/id");

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<BlobStorageContext> context_;
  BlobUrlRegistry url_registry_;
  std::vector<std::string> bad_messages_;
  base::UnguessableToken agent_cluster_id_;
};

TEST_F(BlobURLStoreImplTest, BasicRegisterRevoke) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  // Register a URL and make sure the URL keeps the blob alive.
  BlobURLStoreImpl url_store(kOrigin, url_registry_.AsWeakPtr());
  RegisterURL(&url_store, std::move(blob), kValidUrl);

  blob = url_registry_.GetBlobFromUrl(kValidUrl);
  ASSERT_TRUE(blob);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob));
  EXPECT_EQ(kId, UUIDFromBlob(blob_remote.get()));
  blob_remote.reset();

  // Revoke the URL.
  url_store.Revoke(kValidUrl);
  blob = url_registry_.GetBlobFromUrl(kValidUrl);
  EXPECT_FALSE(blob);

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->registry().HasEntry(kId));
}

TEST_F(BlobURLStoreImplTest, RegisterInvalidScheme) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  RegisterURL(url_store.get(), std::move(blob), kInvalidUrl);
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(kInvalidUrl));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(BlobURLStoreImplTest, RegisterWrongOrigin) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  RegisterURL(url_store.get(), std::move(blob), kWrongOriginUrl);
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(kWrongOriginUrl));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(BlobURLStoreImplTest, RegisterUrlFragment) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  RegisterURL(url_store.get(), std::move(blob), kFragmentUrl);
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(kFragmentUrl));
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(BlobURLStoreImplTest, ImplicitRevoke) {
  mojo::Remote<blink::mojom::Blob> blob(
      CreateBlobFromString(kId, "hello world"));
  mojo::PendingRemote<blink::mojom::Blob> blob2;
  blob->Clone(blob2.InitWithNewPipeAndPassReceiver());

  auto url_store =
      std::make_unique<BlobURLStoreImpl>(kOrigin, url_registry_.AsWeakPtr());
  RegisterURL(url_store.get(), blob.Unbind(), kValidUrl);
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(kValidUrl));
  RegisterURL(url_store.get(), std::move(blob2), kValidUrl2);
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(kValidUrl2));

  // Destroy URL Store, should revoke URLs.
  url_store = nullptr;
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(kValidUrl));
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(kValidUrl2));
}

TEST_F(BlobURLStoreImplTest, RevokeThroughDifferentURLStore) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  BlobURLStoreImpl url_store1(kOrigin, url_registry_.AsWeakPtr());
  BlobURLStoreImpl url_store2(kOrigin, url_registry_.AsWeakPtr());

  RegisterURL(&url_store1, std::move(blob), kValidUrl);
  EXPECT_TRUE(url_registry_.GetBlobFromUrl(kValidUrl));

  url_store2.Revoke(kValidUrl);
  EXPECT_FALSE(url_registry_.GetBlobFromUrl(kValidUrl));
}

TEST_F(BlobURLStoreImplTest, RevokeInvalidScheme) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  url_store->Revoke(kInvalidUrl);
  url_store.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(BlobURLStoreImplTest, RevokeWrongOrigin) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  url_store->Revoke(kWrongOriginUrl);
  url_store.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(BlobURLStoreImplTest, RevokeURLWithFragment) {
  mojo::Remote<BlobURLStore> url_store(CreateURLStore());
  url_store->Revoke(kFragmentUrl);
  url_store.FlushForTesting();
  EXPECT_EQ(1u, bad_messages_.size());
}

TEST_F(BlobURLStoreImplTest, Resolve) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  BlobURLStoreImpl url_store(kOrigin, url_registry_.AsWeakPtr());
  RegisterURL(&url_store, std::move(blob), kValidUrl);

  blob = ResolveURL(&url_store, kValidUrl);
  ASSERT_TRUE(blob);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob));
  EXPECT_EQ(kId, UUIDFromBlob(blob_remote.get()));
  blob = ResolveURL(&url_store, kFragmentUrl);
  ASSERT_TRUE(blob);
  blob_remote.reset();
  blob_remote.Bind(std::move(blob));
  EXPECT_EQ(kId, UUIDFromBlob(blob_remote.get()));
}

TEST_F(BlobURLStoreImplTest, ResolveNonExistentURL) {
  BlobURLStoreImpl url_store(kOrigin, url_registry_.AsWeakPtr());

  mojo::PendingRemote<blink::mojom::Blob> blob =
      ResolveURL(&url_store, kValidUrl);
  EXPECT_FALSE(blob);
  blob = ResolveURL(&url_store, kFragmentUrl);
  EXPECT_FALSE(blob);
}

TEST_F(BlobURLStoreImplTest, ResolveInvalidURL) {
  BlobURLStoreImpl url_store(kOrigin, url_registry_.AsWeakPtr());

  mojo::PendingRemote<blink::mojom::Blob> blob =
      ResolveURL(&url_store, kInvalidUrl);
  EXPECT_FALSE(blob);
}

TEST_F(BlobURLStoreImplTest, ResolveAsURLLoaderFactory) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  BlobURLStoreImpl url_store(kOrigin, url_registry_.AsWeakPtr());
  RegisterURL(&url_store, std::move(blob), kValidUrl);

  mojo::Remote<network::mojom::URLLoaderFactory> factory;
  base::RunLoop resolve_loop;
  url_store.ResolveAsURLLoaderFactory(
      kValidUrl, factory.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](base::OnceClosure done,
             const base::UnguessableToken& agent_registered,
             const absl::optional<base::UnguessableToken>&
                 unsafe_agent_cluster_id,
             const absl::optional<net::SchemefulSite>& unsafe_top_level_site) {
            EXPECT_EQ(agent_registered, unsafe_agent_cluster_id);
            std::move(done).Run();
          },
          resolve_loop.QuitClosure(), agent_cluster_id_));
  resolve_loop.Run();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kValidUrl;
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  base::RunLoop download_loop;
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory.get(), base::BindLambdaForTesting(
                         [&](std::unique_ptr<std::string> response_body) {
                           download_loop.Quit();
                           ASSERT_TRUE(response_body);
                           EXPECT_EQ("hello world", *response_body);
                         }));
  download_loop.Run();
}

TEST_F(BlobURLStoreImplTest, ResolveForNavigation) {
  mojo::PendingRemote<blink::mojom::Blob> blob =
      CreateBlobFromString(kId, "hello world");

  BlobURLStoreImpl url_store(kOrigin, url_registry_.AsWeakPtr());
  RegisterURL(&url_store, std::move(blob), kValidUrl);

  base::RunLoop loop0;
  mojo::Remote<blink::mojom::BlobURLToken> token_remote;
  url_store.ResolveForNavigation(
      kValidUrl, token_remote.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](base::OnceClosure done,
             const base::UnguessableToken& agent_registered,
             const absl::optional<base::UnguessableToken>&
                 unsafe_agent_cluster_id) {
            EXPECT_EQ(agent_registered, unsafe_agent_cluster_id);
            std::move(done).Run();
          },
          loop0.QuitClosure(), agent_cluster_id_));
  loop0.Run();

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
  EXPECT_EQ(kValidUrl, blob_url);
  mojo::Remote<blink::mojom::Blob> blob_remote(std::move(blob));
  EXPECT_EQ(kId, UUIDFromBlob(blob_remote.get()));
}

}  // namespace storage
