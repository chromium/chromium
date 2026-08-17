// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_media_source.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/html/media/media_source_registry.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob.h"
#include "third_party/blink/renderer/platform/blob/testing/fake_blob_url_store.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

using mojom::blink::BlobURLStore;

class TestMediaSourceAttachment final : public MediaSourceAttachment {
 public:
  explicit TestMediaSourceAttachment(MediaSourceRegistry& registry)
      : registry_(registry) {}

  MediaSourceRegistry& Registry() const override { return registry_; }
  void Unregister() override {}
  MediaSourceTracer* StartAttachingToMediaElement(HTMLMediaElement*,
                                                  bool* success) override {
    *success = true;
    return nullptr;
  }
  void CompleteAttachingToMediaElement(
      MediaSourceTracer*,
      std::unique_ptr<WebMediaSource>) override {}
  void Close(MediaSourceTracer*) override {}
  WebTimeRanges BufferedInternal(MediaSourceTracer*) const override {
    return {};
  }
  WebTimeRanges SeekableInternal(MediaSourceTracer*) const override {
    return {};
  }
  void OnTrackChanged(MediaSourceTracer*, TrackBase*) override {}
  void OnElementTimeUpdate(double) override {}
  void OnElementError() override {}
  void OnElementContextDestroyed() override {}

 private:
  ~TestMediaSourceAttachment() override = default;

  MediaSourceRegistry& registry_;
};

class FakeMediaSourceRegistry final : public MediaSourceRegistry {
 public:
  void RegisterUrl(const KURL& url,
                   scoped_refptr<MediaSourceAttachment> attachment) override {
    registrations.push_back(Registration{url, std::move(attachment)});
  }

  void UnregisterUrl(const KURL& url) override {
    unregistrations.push_back(url);
    for (wtf_size_t i = 0; i < registrations.size(); ++i) {
      if (registrations[i].url != url) {
        continue;
      }
      registrations[i].attachment->Unregister();
      registrations.EraseAt(i);
      return;
    }
  }

  scoped_refptr<MediaSourceAttachment> LookupMediaSource(
      const String&) override {
    return nullptr;
  }

  struct Registration {
    KURL url;
    scoped_refptr<MediaSourceAttachment> attachment;
  };
  Vector<Registration> registrations;
  Vector<KURL> unregistrations;
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

  FakeMediaSourceRegistry media_source_registry_;
  std::unique_ptr<DummyPageHolder> page_holder_;

  FakeBlobURLStore url_store_;
  mojo::AssociatedReceiver<BlobURLStore> url_store_receiver_;
};

TEST_F(PublicURLManagerTest, RegisterMediaSourceAttachment) {
  auto attachment =
      base::MakeRefCounted<TestMediaSourceAttachment>(media_source_registry_);
  auto* attachment_ptr = attachment.get();
  String url = url_manager().RegisterUrl(std::move(attachment));
  ASSERT_EQ(1u, media_source_registry_.registrations.size());
  EXPECT_EQ(0u, url_store_.registrations.size());
  EXPECT_EQ(url, media_source_registry_.registrations[0].url);
  EXPECT_EQ(attachment_ptr,
            media_source_registry_.registrations[0].attachment.get());

  EXPECT_TRUE(SecurityOrigin::CreateFromString(url)->IsSameOriginWith(
      GetExecutionContext()->GetSecurityOrigin()));
  EXPECT_EQ(GetExecutionContext()->GetSecurityOrigin(),
            SecurityOrigin::CreateFromString(url));

  url_manager().Revoke(KURL(url));
  EXPECT_FALSE(SecurityOrigin::CreateFromString(url)->IsSameOriginWith(
      GetExecutionContext()->GetSecurityOrigin()));
  url_store_receiver_.FlushForTesting();
  // Revoke() forwards the URL to BlobURLStore even though it was registered
  // with MediaSourceRegistry.
  ASSERT_EQ(1u, url_store_.revocations.size());
  EXPECT_EQ(url, url_store_.revocations[0]);
  ASSERT_EQ(1u, media_source_registry_.unregistrations.size());
  EXPECT_EQ(url, media_source_registry_.unregistrations[0]);
  EXPECT_TRUE(media_source_registry_.registrations.empty());
}

TEST_F(PublicURLManagerTest, ContextDestroyedUnregistersMediaSourceAttachment) {
  auto attachment =
      base::MakeRefCounted<TestMediaSourceAttachment>(media_source_registry_);
  String url = url_manager().RegisterUrl(std::move(attachment));
  ASSERT_EQ(1u, media_source_registry_.registrations.size());

  url_manager().ContextDestroyed();

  ASSERT_EQ(1u, media_source_registry_.unregistrations.size());
  EXPECT_EQ(url, media_source_registry_.unregistrations[0]);
  EXPECT_TRUE(media_source_registry_.registrations.empty());

  // Registration after the manager has stopped releases the attachment.
  auto unregistered_attachment =
      base::MakeRefCounted<TestMediaSourceAttachment>(media_source_registry_);
  EXPECT_TRUE(
      url_manager().RegisterUrl(std::move(unregistered_attachment)).empty());
  EXPECT_TRUE(media_source_registry_.registrations.empty());
}

TEST_F(PublicURLManagerTest, RegisterBlob) {
  Blob* blob = MakeGarbageCollected<Blob>(
      BlobDataHandle::Create("id", "", 0, CreateMojoBlob("id")));
  String url = url_manager().RegisterUrl(blob);

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
  // All three should have been silently ignored.
  EXPECT_TRUE(url_store_.revocations.empty());
}

}  // namespace blink
