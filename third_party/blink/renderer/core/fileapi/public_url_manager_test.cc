// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/fileapi/url_registry.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob_url_store.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

using mojom::blink::BlobURLStore;

class TestURLRegistrable : public URLRegistrable {
 public:
  TestURLRegistrable(
      URLRegistry* registry,
      mojo::PendingRemote<mojom::blink::Blob> blob = mojo::NullRemote())
      : registry_(registry), blob_(std::move(blob)) {}

  URLRegistry& Registry() const override { return *registry_; }

  bool IsMojoBlob() override { return bool{blob_}; }

  void CloneMojoBlob(
      mojo::PendingReceiver<mojom::blink::Blob> receiver) override {
    if (!blob_)
      return;
    blob_->Clone(std::move(receiver));
  }

 private:
  URLRegistry* const registry_;
  mojo::Remote<mojom::blink::Blob> blob_;
};

class FakeURLRegistry : public URLRegistry {
 public:
  void RegisterURL(const KURL& url, URLRegistrable* registrable) override {
    registrations.push_back(Registration{url, registrable});
  }
  void UnregisterURL(const KURL&) override {}

  struct Registration {
    KURL url;
    URLRegistrable* registrable;
  };
  Vector<Registration> registrations;
};

}  // namespace

class PublicURLManagerTest : public testing::Test {
 public:
  PublicURLManagerTest() : url_store_receiver_(&url_store_) {}

  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();
    // By default this creates a unique origin, which is exactly what this test
    // wants.
    SetUpSecurityContextForTesting();

    HeapMojoAssociatedRemote<BlobURLStore> url_store_remote(
        GetExecutionContext());
    url_store_receiver_.Bind(
        url_store_remote.BindNewEndpointAndPassDedicatedReceiver());
    url_manager().SetURLStoreForTesting(std::move(url_store_remote));
  }

  PublicURLManager& url_manager() {
    return GetExecutionContext()->GetPublicURLManager();
  }

  mojo::PendingRemote<mojom::blink::Blob> CreateMojoBlob(const String& uuid) {
    mojo::PendingRemote<mojom::blink::Blob> result;
    mojo::MakeSelfOwnedReceiver(std::make_unique<FakeBlob>(uuid),
                                result.InitWithNewPipeAndPassReceiver());
    return result;
  }

  ExecutionContext* GetExecutionContext() const {
    return page_holder_->GetFrame().DomWindow();
  }

  void SetURL(const KURL& url) { page_holder_->GetDocument().SetURL(url); }

  void SetUpSecurityContextForTesting() {
    // Replicate what NullExecutionContext::SetUpSecurityContextForTesting()
    // does but using the execution context associated with `page_holder_`.
    auto* policy = MakeGarbageCollected<ContentSecurityPolicy>();
    auto* window = page_holder_->GetFrame().DomWindow();
    auto& security_context = window->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(
        SecurityOrigin::Create(window->Url()));
    policy->BindToDelegate(window->GetContentSecurityPolicyDelegate());
    window->SetContentSecurityPolicy(policy);
  }

 protected:
  test::TaskEnvironment task_environment_;

  std::unique_ptr<DummyPageHolder> page_holder_;

  FakeBlobURLStore url_store_;
  mojo::AssociatedReceiver<BlobURLStore> url_store_receiver_;
};

TEST_F(PublicURLManagerTest, RegisterNonMojoBlob) {
  FakeURLRegistry registry;
  TestURLRegistrable registrable(&registry);
  String url = url_manager().RegisterURL(&registrable);
  ASSERT_EQ(1u, registry.registrations.size());
  EXPECT_EQ(0u, url_store_.registrations.size());
  EXPECT_EQ(url, registry.registrations[0].url);
  EXPECT_EQ(&registrable, registry.registrations[0].registrable);

  EXPECT_TRUE(SecurityOrigin::CreateFromString(url)->IsSameOriginWith(
      GetExecutionContext()->GetSecurityOrigin()));
  EXPECT_EQ(GetExecutionContext()->GetSecurityOrigin(),
            SecurityOrigin::CreateFromString(url));

  url_manager().Revoke(KURL(url));
  EXPECT_FALSE(SecurityOrigin::CreateFromString(url)->IsSameOriginWith(
      GetExecutionContext()->GetSecurityOrigin()));
  url_store_receiver_.FlushForTesting();
  // Even though this was not a mojo blob, the PublicURLManager might not know
  // that, so still expect a revocation on the mojo interface.
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
}

TEST_F(PublicURLManagerTest, RegisterMojoBlob) {
  FakeURLRegistry registry;
  TestURLRegistrable registrable(&registry, CreateMojoBlob("id"));
  String url = url_manager().RegisterURL(&registrable);

  EXPECT_EQ(0u, registry.registrations.size());
  ASSERT_EQ(1u, url_store_.registrations.size());
  EXPECT_EQ(url, url_store_.registrations.begin()->key);

  EXPECT_TRUE(SecurityOrigin::CreateFromString(url)->IsSameOriginWith(
      GetExecutionContext()->GetSecurityOrigin()));
  EXPECT_EQ(GetExecutionContext()->GetSecurityOrigin(),
            SecurityOrigin::CreateFromString(url));

  url_manager().Revoke(KURL(url));
  EXPECT_FALSE(SecurityOrigin::CreateFromString(url)->IsSameOriginWith(
      GetExecutionContext()->GetSecurityOrigin()));
  url_store_receiver_.FlushForTesting();
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
}

TEST_F(PublicURLManagerTest, RevokeValidNonRegisteredURL) {
  SetURL(KURL("http://example.com/foo/bar"));
  SetUpSecurityContextForTesting();

  KURL url = KURL("blob:http://example.com/id");
  url_manager().Revoke(url);
  url_store_receiver_.FlushForTesting();
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
}

TEST_F(PublicURLManagerTest, RevokeInvalidURL) {
  SetURL(KURL("http://example.com/foo/bar"));
  SetUpSecurityContextForTesting();

  KURL invalid_scheme_url = KURL("blb:http://example.com/id");
  KURL fragment_url = KURL("blob:http://example.com/id#fragment");
  KURL invalid_origin_url = KURL("blob:http://foobar.com/id");
  url_manager().Revoke(invalid_scheme_url);
  url_manager().Revoke(fragment_url);
  url_manager().Revoke(invalid_origin_url);
  url_store_receiver_.FlushForTesting();
  // Both should have been silently ignored.
  EXPECT_TRUE(url_store_.revocations.empty());
}

}  // namespace blink
