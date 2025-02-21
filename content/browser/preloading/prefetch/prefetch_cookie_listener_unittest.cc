// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"

#include <memory>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchCookieListenerTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());
  }

  std::unique_ptr<PrefetchCookieListener> MakeCookieListener(const GURL& url) {
    std::unique_ptr<PrefetchCookieListener> cookie_listener =
        PrefetchCookieListener::MakeAndRegister(url, cookie_manager_.get());
    DCHECK(cookie_listener);
    return cookie_listener;
  }

  // Creates a host cookie for the given url, and then adds it to the default
  // partition using |cookie_manager_|.
  bool SetHostCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateForTesting(url, value, base::Time::Now()));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsHostCookie());

    return SetCookie(*cookie.get(), url);
  }

  // Creates a domain cookie for the given url, and then adds it to the default
  // partition using |cookie_manager_|.
  bool SetDomainCookie(const GURL& url,
                       const std::string& name,
                       const std::string& value,
                       const std::string& domain) {
    net::CookieInclusionStatus status;
    std::unique_ptr<net::CanonicalCookie> cookie(
        net::CanonicalCookie::CreateSanitizedCookie(
            url, name, value, domain, /*path=*/"", base::Time::Now(),
            base::Time::Now() + base::Hours(1), base::Time::Now(),
            /*secure=*/true, /*http_only=*/false,
            net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
            /*partition_key=*/std::nullopt, &status));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsDomainCookie());
    EXPECT_TRUE(status.IsInclude());

    return SetCookie(*cookie.get(), url);
  }

 private:
  bool SetCookie(const net::CanonicalCookie& cookie, const GURL& url) {
    bool result = false;
    base::RunLoop run_loop;

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        cookie, url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));

    // This will run until the cookie is set.
    run_loop.Run();

    // This will run until the cookie listener is updated.
    base::RunLoop().RunUntilIdle();

    return result;
  }

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

TEST_F(PrefetchCookieListenerTest, NoCookiesChanged) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedHostCookiesForSameURL) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  ASSERT_TRUE(SetHostCookie(GURL("https://www.example.com/"), "test-cookie"));

  EXPECT_TRUE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedHostCookiesForOtherURL) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  ASSERT_TRUE(SetHostCookie(GURL("https://www.other.com/"), "test-cookie"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedHostCookiesForGeneralDomain) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  ASSERT_TRUE(SetHostCookie(GURL("https://generaldomain.com/"), "test-cookie"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedHostCookiesForSubomain) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  ASSERT_TRUE(SetHostCookie(
      GURL("https://veryspecificdomain.specificdomain.generaldomain.com/"),
      "test-cookie"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedDomainCookiesForSameURL) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  ASSERT_TRUE(SetDomainCookie(GURL("https://www.example.com/"), "test",
                              "cookie", "www.example.com"));

  EXPECT_TRUE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedDomainCookiesForOtherURL) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  ASSERT_TRUE(SetDomainCookie(GURL("https://www.other.com/"), "test", "cookie",
                              "www.other.com"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedDomainCookiesForGeneralDomain) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  ASSERT_TRUE(SetDomainCookie(GURL("https://generaldomain.com/"), "test",
                              "cookie", "generaldomain.com"));

  EXPECT_TRUE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, ChangedDomainCookiesForSubomain) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://specificdomain.generaldomain.com/"));

  ASSERT_TRUE(SetDomainCookie(
      GURL("https://veryspecificdomain.specificdomain.generaldomain.com/"),
      "test", "cookie", "veryspecificdomain.specificdomain.generaldomain.com"));

  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

TEST_F(PrefetchCookieListenerTest, StopListening) {
  std::unique_ptr<PrefetchCookieListener> cookie_listener =
      MakeCookieListener(GURL("https://www.example.com/"));

  cookie_listener->StopListening();

  ASSERT_TRUE(SetHostCookie(GURL("https://www.example.com"), "test-cookie"));

  // Since the cookies were changed after |StopListening| was called, the
  // listener shouldn't update.
  EXPECT_FALSE(cookie_listener->HaveCookiesChanged());
}

}  // namespace
}  // namespace content
