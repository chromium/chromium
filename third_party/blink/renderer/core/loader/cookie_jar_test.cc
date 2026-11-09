// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include <utility>

#include "mojo/public/cpp/base/shared_memory_version.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

class FakeRestrictedCookieManager
    : public network::mojom::blink::RestrictedCookieManager {
 public:
  explicit FakeRestrictedCookieManager(const String& initial_cookies = "")
      : cookies_(initial_cookies) {}
  ~FakeRestrictedCookieManager() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingReceiver<network::mojom::blink::RestrictedCookieManager>(
            std::move(handle)));
  }

  mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
  BindNewPipeAndPassRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Disconnect() { receiver_.reset(); }

  int get_cookies_string_count() const { return get_cookies_string_count_; }

  // network::mojom::blink::RestrictedCookieManager:
  void GetAllForUrl(const KURL& url,
                    const net::SiteForCookies& site_for_cookies,
                    const scoped_refptr<const SecurityOrigin>& top_frame_origin,
                    net::StorageAccessApiStatus storage_access_api_status,
                    network::mojom::blink::CookieManagerGetOptionsPtr options,
                    bool is_ad_tagged,
                    bool apply_devtools_overrides,
                    bool force_disable_third_party_cookies,
                    GetAllForUrlCallback callback) override {}

  void SetCanonicalCookie(
      network::mojom::blink::RestrictedCanonicalCookieParamsPtr
          canonical_cookie_params,
      const KURL& url,
      const net::SiteForCookies& site_for_cookies,
      const scoped_refptr<const SecurityOrigin>& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      SetCanonicalCookieCallback callback) override {}

  void AddChangeListener(
      const KURL& url,
      const net::SiteForCookies& site_for_cookies,
      const scoped_refptr<const SecurityOrigin>& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      mojo::PendingRemote<network::mojom::blink::CookieChangeListener> listener,
      AddChangeListenerCallback callback) override {}

  void SetCookieFromString(
      const KURL& url,
      const net::SiteForCookies& site_for_cookies,
      const scoped_refptr<const SecurityOrigin>& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      const String& cookie) override {
    cookies_ = cookie;
    version_controller_.Increment();
    version_controller_.CommitWrite();
  }

  void GetCookiesString(
      const KURL& url,
      const net::SiteForCookies& site_for_cookies,
      const scoped_refptr<const SecurityOrigin>& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool get_version_shared_memory,
      bool is_ad_tagged,
      bool apply_devtools_overrides,
      bool force_disable_third_party_cookies,
      GetCookiesStringCallback callback) override {
    ++get_cookies_string_count_;
    std::move(callback).Run(version_controller_.GetSharedVersion(),
                            get_version_shared_memory
                                ? version_controller_.GetSharedMemoryRegion()
                                : base::ReadOnlySharedMemoryRegion(),
                            cookies_);
  }

  void CookiesEnabledFor(
      const KURL& url,
      const net::SiteForCookies& site_for_cookies,
      const scoped_refptr<const SecurityOrigin>& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      bool apply_devtools_overrides,
      CookiesEnabledForCallback callback) override {
    std::move(callback).Run(true);
  }

 private:
  String cookies_;
  int get_cookies_string_count_ = 0;
  mojo::SharedMemoryVersionController version_controller_;
  mojo::Receiver<network::mojom::blink::RestrictedCookieManager> receiver_{
      this};
};

class CookieJarTest : public PageTestBase {
 protected:
  void SetUp() override {
    PageTestBase::SetUp();
    NavigateTo(KURL("https://example.com/"));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        network::mojom::blink::RestrictedCookieManager::Name_, {});
    PageTestBase::TearDown();
  }

  void SetCookieManagerBinder(FakeRestrictedCookieManager& backend) {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        network::mojom::blink::RestrictedCookieManager::Name_, {});
    ASSERT_TRUE(GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        network::mojom::blink::RestrictedCookieManager::Name_,
        BindRepeating(&FakeRestrictedCookieManager::Bind,
                      Unretained(&backend))));
  }
};

TEST_F(CookieJarTest, DisconnectResetsRequiredCommittedWrites) {
  FakeRestrictedCookieManager backend1;
  SetCookieManagerBinder(backend1);

  auto* cookie_jar = MakeGarbageCollected<CookieJar>(&GetDocument());
  EXPECT_TRUE(cookie_jar->SetCookie("a=1"));
  test::RunPendingTasks();

  // First read fetches the cookie string and maps the shared memory version.
  EXPECT_EQ("a=1", cookie_jar->Cookies());
  EXPECT_EQ(1, backend1.get_cookies_string_count());

  // Second read is served from the cache without an IPC.
  EXPECT_EQ("a=1", cookie_jar->Cookies());
  EXPECT_EQ(1, backend1.get_cookies_string_count());

  // Simulate a backend disconnect and reconnect to a new backend whose
  // committed writes counter starts at 0.
  FakeRestrictedCookieManager backend2("a=1");
  SetCookieManagerBinder(backend2);
  backend1.Disconnect();
  test::RunPendingTasks();

  // First read after reconnect fetches from the new backend and maps its
  // shared memory region.
  EXPECT_EQ("a=1", cookie_jar->Cookies());
  EXPECT_EQ(1, backend2.get_cookies_string_count());

  // Subsequent reads should hit the cache rather than issuing synchronous IPCs
  // due to a stale required_committed_writes_ count from the old backend.
  EXPECT_EQ("a=1", cookie_jar->Cookies());
  EXPECT_EQ(1, backend2.get_cookies_string_count());
}

TEST_F(CookieJarTest, SetCookieManagerResetsCacheAndWriteCount) {
  FakeRestrictedCookieManager backend1;
  auto* cookie_jar = MakeGarbageCollected<CookieJar>(&GetDocument());
  cookie_jar->SetCookieManager(backend1.BindNewPipeAndPassRemote());

  EXPECT_TRUE(cookie_jar->SetCookie("a=1"));
  test::RunPendingTasks();

  EXPECT_EQ("a=1", cookie_jar->Cookies());
  EXPECT_EQ(1, backend1.get_cookies_string_count());
  EXPECT_EQ("a=1", cookie_jar->Cookies());
  EXPECT_EQ(1, backend1.get_cookies_string_count());

  // Replacing the cookie manager should invalidate the cached cookie string
  // and reset required_committed_writes_.
  FakeRestrictedCookieManager backend2("b=2");
  cookie_jar->SetCookieManager(backend2.BindNewPipeAndPassRemote());

  EXPECT_EQ("b=2", cookie_jar->Cookies());
  EXPECT_EQ(1, backend2.get_cookies_string_count());

  // The second read on the new backend should be served from the cache.
  EXPECT_EQ("b=2", cookie_jar->Cookies());
  EXPECT_EQ(1, backend2.get_cookies_string_count());
}

}  // namespace
}  // namespace blink
