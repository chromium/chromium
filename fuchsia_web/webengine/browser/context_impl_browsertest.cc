// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fdio/namespace.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webengine/test/frame_for_test.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/url_constants.h"

namespace {

// Defines a suite of tests that exercise browser-level configuration and
// functionality.
class ContextImplTest : public WebEngineBrowserTest {
 public:
  ContextImplTest() = default;
  ~ContextImplTest() override = default;

  ContextImplTest(const ContextImplTest&) = delete;
  ContextImplTest& operator=(const ContextImplTest&) = delete;

 protected:
  // Synchronously gets the list of all cookies from the fuchsia.web.Context.
  std::vector<fuchsia::web::Cookie> GetCookies() {
    base::RunLoop get_cookies_loop;

    // Connect to the Context's CookieManager and request all the cookies.
    fuchsia::web::CookieManagerPtr cookie_manager;
    context()->GetCookieManager(cookie_manager.NewRequest());
    fuchsia::web::CookiesIteratorPtr cookies_iterator;
    cookie_manager->GetCookieList(nullptr, nullptr,
                                  cookies_iterator.NewRequest());

    // |cookies_iterator| will disconnect once after the last cookies have been
    // returned by GetNext().
    cookies_iterator.set_error_handler([&](zx_status_t status) {
      EXPECT_EQ(ZX_ERR_PEER_CLOSED, status);
      get_cookies_loop.Quit();
    });
    std::vector<fuchsia::web::Cookie> cookies;

    // std::function<> must be used here because fit::function<> is move-only
    // and this callback will be used both for the initial GetNext() call, and
    // for the follow-up calls made each time GetNext() results are received.
    std::function<void(std::vector<fuchsia::web::Cookie>)> get_next_callback =
        [&](std::vector<fuchsia::web::Cookie> new_cookies) {
          cookies.insert(cookies.end(),
                         std::make_move_iterator(new_cookies.begin()),
                         std::make_move_iterator(new_cookies.end()));
          cookies_iterator->GetNext(get_next_callback);
        };
    cookies_iterator->GetNext(get_next_callback);

    get_cookies_loop.Run();

    return cookies;
  }
};

fuchsia::web::Cookie CreateExpectedCookie() {
  fuchsia::web::Cookie cookie;
  fuchsia::web::CookieId id;
  id.set_name("foo");
  id.set_path("/");
  id.set_domain("127.0.0.1");
  cookie.set_id(std::move(id));
  cookie.set_value("bar");
  return cookie;
}

const fuchsia::web::Cookie& ExpectedCookie() {
  static const base::NoDestructor<fuchsia::web::Cookie> expected_cookie(
      CreateExpectedCookie());
  return *expected_cookie;
}

}  // namespace

class PersistedCookieTest : public ContextImplTest {
 public:
  PersistedCookieTest() = default;
  ~PersistedCookieTest() override = default;

  PersistedCookieTest(const PersistedCookieTest&) = delete;
  PersistedCookieTest& operator=(const PersistedCookieTest&) = delete;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kOnDiskHttpCacheSize, "1000000");
    ContextImplTest::SetUp();
  }

  // Ensures that the cookie backing store is written to disk.
  void FlushCookies() {
    base::RunLoop run_loop;
    mojo::Remote<network::mojom::CookieManager> cookie_manager;
    context_impl()->GetNetworkContextForTest()->GetCookieManager(
        cookie_manager.BindNewPipeAndPassReceiver());
    cookie_manager->FlushCookieStore(run_loop.QuitClosure());
    run_loop.Run();
  }
};

// BrowserContext with persistent storage stores cookies such that they can
// be retrieved via the CookieManager API.
IN_PROC_BROWSER_TEST_F(PersistedCookieTest, PersistedCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  // Max-Age needs to be set, otherwise the cookie is a "session" cookie and
  // therefore will not be written to the persistent cookie store.
  const GURL kSetCookieUrl(
      embedded_test_server()->GetURL("/set-cookie?foo=bar;Max-Age=100"));

  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(), kSetCookieUrl.spec());
  frame.navigation_listener().RunUntilUrlEquals(kSetCookieUrl);

  std::vector<fuchsia::web::Cookie> cookies = GetCookies();
  ASSERT_EQ(cookies.size(), 1u);
  ASSERT_TRUE(cookies[0].has_id());
  EXPECT_TRUE(fidl::Equals(cookies[0], ExpectedCookie()));
  FlushCookies();

  // Check that the cookie persists beyond the lifetime of the browser by
  // destroying and recreating the Context and re-querying the CookieStore.
  frame = FrameForTest();
  DisconnectContext();
  base::RunLoop().RunUntilIdle();
  ConnectContext();
  base::RunLoop().RunUntilIdle();

  cookies = GetCookies();
  ASSERT_EQ(cookies.size(), 1u);
  ASSERT_TRUE(cookies[0].has_id());
  EXPECT_TRUE(fidl::Equals(cookies[0], ExpectedCookie()));
}

// Suite for tests which run the BrowserContext in incognito mode (no data
// directory).
class IncognitoContextImplTest : public ContextImplTest {
 public:
  IncognitoContextImplTest() = default;
  ~IncognitoContextImplTest() override = default;

  IncognitoContextImplTest(const IncognitoContextImplTest&) = delete;
  IncognitoContextImplTest& operator=(const IncognitoContextImplTest&) = delete;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kIncognito);
    ContextImplTest::SetUp();
  }
};

// Verify that the browser can be initialized without a persistent data
// directory.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, NavigateFrame) {
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlEquals(GURL(url::kAboutBlankURL));
}

// In-memory cookie store stores cookies, and is accessible via CookieManager.
IN_PROC_BROWSER_TEST_F(IncognitoContextImplTest, InMemoryCookieStore) {
  ASSERT_TRUE(embedded_test_server()->Start());
  auto frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  const GURL kSetCookieUrl(
      embedded_test_server()->GetURL("/set-cookie?foo=bar"));
  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(), kSetCookieUrl.spec());
  frame.navigation_listener().RunUntilUrlEquals(kSetCookieUrl);

  std::vector<fuchsia::web::Cookie> cookies = GetCookies();
  ASSERT_EQ(cookies.size(), 1u);
  EXPECT_TRUE(fidl::Equals(cookies[0], ExpectedCookie()));
}

enum CacheTestFlags : uint32_t {
  CACHE_TEST_DEFAULT = 0,

  // Whether to use an in-memory cache or a disk cache.
  CACHE_TEST_CACHE_DIR_AVAILABLE = 1 << 0,

  // If the browser Context is off the record ("Incognito").
  CACHE_TEST_INCOGNITO = 1 << 1,

  // If the browser should be restarted (i.e. to verify whether disk
  // persistence is functioning properly).
  CACHE_TEST_RECREATE_CONTEXT = 1 << 2,
};

enum class CacheExpectations {
  HIT,
  MISS,
};

struct CacheTestParams {
  // See documentation in CacheTestFlags.
  uint32_t flags;

  // The quota to apply to the disk cache.
  absl::optional<size_t> disk_cache_size;

  // The quota to apply to the memory cache.
  absl::optional<size_t> mem_cache_size;

  // Whether the cache is expected to succeed..
  CacheExpectations expectations;
};

class CacheContextImplTest
    : public ContextImplTest,
      public testing::WithParamInterface<CacheTestParams> {
 public:
  CacheContextImplTest() = default;
  ~CacheContextImplTest() override = default;

  CacheContextImplTest(const CacheContextImplTest&) = delete;
  CacheContextImplTest& operator=(const CacheContextImplTest&) = delete;

  void SetUp() override {
    if (GetParam().disk_cache_size) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kOnDiskHttpCacheSize,
          base::NumberToString(*GetParam().disk_cache_size));
    }
    if (GetParam().mem_cache_size) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kInMemoryHttpCacheSize,
          base::NumberToString(*GetParam().mem_cache_size));
    }

    if ((GetParam().flags & CACHE_TEST_CACHE_DIR_AVAILABLE) !=
        CACHE_TEST_CACHE_DIR_AVAILABLE) {
      // Banish /cache for the process, which will lead to an in-memory cache
      // being used. The namespace modification is isolated to the individual
      // test's process.
      fdio_ns_t* fdio_namespace;
      zx_status_t status = fdio_ns_get_installed(&fdio_namespace);
      ZX_DCHECK(status == ZX_OK, status) << "fdio_ns_get_installed";

      status =
          fdio_ns_unbind(fdio_namespace, base::kPersistedCacheDirectoryPath);
      ZX_DCHECK(status == ZX_OK, status) << "fdio_ns_unbind";
    }

    if (GetParam().flags & CACHE_TEST_INCOGNITO) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kIncognito);
    }

    ContextImplTest::SetUp();
  }
};

// Parameterized test that verifies HTTP cache functionality, using
// disk vs. RAM and a quota size as parameters.
IN_PROC_BROWSER_TEST_P(CacheContextImplTest, TestCache) {
  auto test_server_handle = embedded_test_server()->StartAndReturnHandle();
  FrameForTest frame =
      FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());

  // Max-Age needs to be set, otherwise the cookie is a "session" cookie and
  // therefore will not be written to the persistent cookie store.
  const GURL kCachedPageUrl(embedded_test_server()->GetURL("/cachetime"));
  const std::string kCachedPageTitle = "Cache: max-age=60";

  LoadUrlAndExpectResponse(frame.GetNavigationController(),
                           fuchsia::web::LoadUrlParams(),
                           kCachedPageUrl.spec());
  frame.navigation_listener().RunUntilTitleEquals(kCachedPageTitle);

  if (GetParam().flags & CACHE_TEST_RECREATE_CONTEXT) {
    // Exercise data persistence on disk by dumping the browser context and
    // recreating one from scratch.
    frame = FrameForTest();
    DisconnectContext();
    base::RunLoop().RunUntilIdle();
    ConnectContext();
    base::RunLoop().RunUntilIdle();
    frame = FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
    base::RunLoop().RunUntilIdle();
  } else {
    // Navigate away from the page so that we can navigate back to it again
    // later.
    LoadUrlAndExpectResponse(frame.GetNavigationController(),
                             fuchsia::web::LoadUrlParams(), "about:blank");
    frame.navigation_listener().RunUntilLoaded();
  }

  // Terminate the test server.
  test_server_handle = {};

  // Attempt to access the same page.
  if (GetParam().expectations == CacheExpectations::HIT) {
    LoadUrlAndExpectResponse(frame.GetNavigationController(),
                             fuchsia::web::LoadUrlParams(),
                             kCachedPageUrl.spec());
    frame.navigation_listener().RunUntilTitleEquals(kCachedPageTitle);
  } else {
    LoadUrlAndExpectResponse(frame.GetNavigationController(),
                             fuchsia::web::LoadUrlParams(),
                             kCachedPageUrl.spec());
    std::string expected_title = kCachedPageUrl.spec().substr(7);
    frame.navigation_listener().RunUntilErrorPageIsLoadedAndTitleEquals(
        expected_title);
  }
}

CacheTestParams cache_test_variations[] = {
    // Field order:
    // flags, disk_cache_size, mem_cache_size, expect_cache_hit
    // In-memory cache tests.
    // Cache success: cache is large enough.
    {CACHE_TEST_DEFAULT, absl::nullopt, 10 * 1024 * 1024,
     CacheExpectations::HIT},
    // Cache success: mem cache used even if /cache is present when no disk
    // quota specified.
    {CACHE_TEST_CACHE_DIR_AVAILABLE, absl::nullopt, 10 * 1024 * 1024,
     CacheExpectations::HIT},
    // Cache success: No cache size specified, cache active as per browser
    // defaults.
    {CACHE_TEST_DEFAULT, absl::nullopt, absl::nullopt, CacheExpectations::HIT},
    // Cache failure: Cache disabled (zero cache size).
    {CACHE_TEST_DEFAULT, absl::nullopt, 0, CacheExpectations::MISS},
    // Cache failure: Cache too small (10-byte limit).
    {CACHE_TEST_DEFAULT, absl::nullopt, 10, CacheExpectations::MISS},

    // On-disk cache tests.
    // Cache success: Disk cache is large enough.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_RECREATE_CONTEXT,
     10 * 1024 * 1024, absl::nullopt, CacheExpectations::HIT},
    // Cache failure: Invalid cache size.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_RECREATE_CONTEXT, 0,
     absl::nullopt, CacheExpectations::MISS},
    // Cache failure: No cache size specified.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_RECREATE_CONTEXT,
     absl::nullopt, absl::nullopt, CacheExpectations::MISS},
    // Cache failure: Disk cache is too small.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_RECREATE_CONTEXT, 1,
     absl::nullopt, CacheExpectations::MISS},

    // Incognito mode.
    // Cache success: the in-memory cache is used, and the degenerate disk cache
    // is skipped.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_INCOGNITO, 1, 10 * 1024 * 1024,
     CacheExpectations::HIT},
    // Cache success: the in-memory cache is used by default by OTR unless
    // disabled
    // otherwise.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_INCOGNITO, 10 * 1024 * 1024,
     absl::nullopt, CacheExpectations::HIT},
    // Cache failure: the in memory cache is disabled.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_INCOGNITO, 10 * 1024 * 1024, 0,
     CacheExpectations::MISS},
    // Cache failure: the cache is dumped on browser reinstantiation.
    {CACHE_TEST_CACHE_DIR_AVAILABLE | CACHE_TEST_INCOGNITO |
         CACHE_TEST_RECREATE_CONTEXT,
     10 * 1024 * 1024, 10 * 1024 * 1024, CacheExpectations::MISS},
};

INSTANTIATE_TEST_SUITE_P(All,
                         CacheContextImplTest,
                         testing::ValuesIn(cache_test_variations));

class DataQuotaSwitchTest : public WebEngineBrowserTest {
 public:
  DataQuotaSwitchTest() = default;
  ~DataQuotaSwitchTest() override = default;

  DataQuotaSwitchTest(const DataQuotaSwitchTest&) = delete;
  DataQuotaSwitchTest& operator=(const DataQuotaSwitchTest&) = delete;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDataQuotaBytes, "10000");
    WebEngineBrowserTest::SetUp();
  }
};

// Verify that the quota command line switch is read and applied to the
// SysInfo quota.
IN_PROC_BROWSER_TEST_F(DataQuotaSwitchTest, SysInfoPopulated) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_EQ(base::SysInfo::AmountOfTotalDiskSpace(base::FilePath("/data")),
            10000u);
}
