// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/cookie_manager_impl.h"

#include <lib/fidl/cpp/binding.h>

#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/cookie_access_result.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestCookieUrl[] = "https://www.testing.com/";
const char kTestOtherUrl[] = "https://www.other.com/";
const char kCookieName1[] = "Cookie";
const char kCookieName2[] = "Monster";
const char kCookieValue1[] = "Eats";
const char kCookieValue2[] = "Cookies";
const char kCookieValue3[] = "Nyom nyom nyom";

// Creates a CanonicalCookie with |name| and |value|, for kTestCookieUrl.
std::unique_ptr<net::CanonicalCookie> CreateCookie(std::string_view name,
                                                   std::string_view value) {
  return net::CanonicalCookie::CreateSanitizedCookie(
      GURL(kTestCookieUrl), std::string(name), std::string(value),
      /*domain=*/"",
      /*path=*/"", /*creation_time=*/base::Time(),
      /*expiration_time=*/base::Time(), /*last_access_time=*/base::Time(),
      /*secure=*/true,
      /*httponly*/ false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_MEDIUM,
      /*partition_key=*/std::nullopt, /*status=*/nullptr);
}

class CookieManagerImplTest : public testing::Test {
 public:
  CookieManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        network_service_(network::NetworkService::CreateForTesting()),
        cookie_manager_(
            base::BindRepeating(&CookieManagerImplTest::GetNetworkContext,
                                base::Unretained(this))) {}

  CookieManagerImplTest(const CookieManagerImplTest&) = delete;
  CookieManagerImplTest& operator=(const CookieManagerImplTest&) = delete;

  ~CookieManagerImplTest() override = default;

 protected:
  network::mojom::NetworkContext* GetNetworkContext() {
    if (!network_context_.is_bound()) {
      network::mojom::NetworkContextParamsPtr params =
          network::mojom::NetworkContextParams::New();
      // Use a dummy CertVerifier that always passes cert verification, since
      // these unittests don't need to test CertVerifier behavior.
      params->cert_verifier_params =
          network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();
      network_service_->CreateNetworkContext(
          network_context_.BindNewPipeAndPassReceiver(), std::move(params));
      network_context_.reset_on_disconnect();
    }
    return network_context_.get();
  }

  // Adds the specified cookie under kTestCookieUrl.
  void CreateAndSetCookieAsync(std::string_view name, std::string_view value) {
    EnsureMojoCookieManager();

    net::CookieOptions options;
    mojo_cookie_manager_->SetCanonicalCookie(
        *CreateCookie(name, value), GURL(kTestCookieUrl), options,
        base::BindOnce([](net::CookieAccessResult result) {
          EXPECT_TRUE(result.status.IsInclude());
        }));
  }

  // Removes the specified cookie from under kTestCookieUrl.
  void DeleteCookieAsync(std::string_view name, std::string_view value) {
    EnsureMojoCookieManager();

    mojo_cookie_manager_->DeleteCanonicalCookie(
        *CreateCookie(name, value),
        base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  }

  // Synchronously fetches all cookies via the |cookie_manager_|.
  // Returns a std::nullopt if the iterator closes before a GetNext() returns.
  std::optional<std::vector<fuchsia::web::Cookie>> GetAllCookies() {
    base::RunLoop get_cookies_loop;
    fuchsia::web::CookiesIteratorPtr cookies_iterator;
    cookies_iterator.set_error_handler([&](zx_status_t status) {
      EXPECT_EQ(ZX_ERR_PEER_CLOSED, status);
      get_cookies_loop.Quit();
    });
    cookie_manager_.GetCookieList(nullptr, nullptr,
                                  cookies_iterator.NewRequest());
    std::optional<std::vector<fuchsia::web::Cookie>> cookies;
    std::function<void(std::vector<fuchsia::web::Cookie>)> get_next_callback =
        [&](std::vector<fuchsia::web::Cookie> new_cookies) {
          if (!cookies.has_value()) {
            cookies.emplace(std::move(new_cookies));
          } else {
            cookies->insert(cookies->end(),
                            std::make_move_iterator(new_cookies.begin()),
                            std::make_move_iterator(new_cookies.end()));
          }
          cookies_iterator->GetNext(get_next_callback);
        };
    cookies_iterator->GetNext(get_next_callback);
    get_cookies_loop.Run();
    return cookies;
  }

  void EnsureMojoCookieManager() {
    if (mojo_cookie_manager_.is_bound())
      return;
    network_context_->GetCookieManager(
        mojo_cookie_manager_.BindNewPipeAndPassReceiver());
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::CookieManager> mojo_cookie_manager_;

  CookieManagerImpl cookie_manager_;
};

// Calls GetNext() on the supplied |iterator| and lets the caller express
// expectations on the results.
class GetNextCookiesIteratorResult {
 public:
  explicit GetNextCookiesIteratorResult(
      fuchsia::web::CookiesIterator* iterator) {
    iterator->GetNext(CallbackToFitFunction(result_.GetCallback()));
  }

  GetNextCookiesIteratorResult(const GetNextCookiesIteratorResult&) = delete;
  GetNextCookiesIteratorResult& operator=(const GetNextCookiesIteratorResult&) =
      delete;

  ~GetNextCookiesIteratorResult() = default;

  void ExpectSingleCookie(std::string_view name,
                          std::optional<std::string_view> value) {
    ExpectCookieUpdates({{name, value}});
  }

  void ExpectDeleteSingleCookie(std::string_view name) {
    ExpectCookieUpdates({{name, std::nullopt}});
  }

  // Specifies the cookie name/value pairs expected in the GetNext() results.
  // Deletions expectations are specified by using std::nullopt as the value.
  void ExpectCookieUpdates(
      std::map<std::string_view, std::optional<std::string_view>> expected) {
    ASSERT_TRUE(result_.Wait());
    ASSERT_EQ(result_.Get().size(), expected.size());
    std::map<std::string_view, std::string_view> result_updates;
    for (const auto& cookie_update : result_.Get()) {
      ASSERT_TRUE(cookie_update.has_id());
      ASSERT_TRUE(cookie_update.id().has_name());
      auto it = expected.find(cookie_update.id().name());
      ASSERT_TRUE(it != expected.end());
      ASSERT_EQ(cookie_update.has_value(), it->second.has_value());
      if (it->second.has_value())
        EXPECT_EQ(*it->second, cookie_update.value());
      expected.erase(it);
    }
    EXPECT_TRUE(expected.empty());
  }

  void ExpectReceivedNoUpdates() {
    // If we ran |loop_| then this would hang, so just ensure any pending work
    // has been processed.
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(result_.IsReady());
  }

 protected:
  base::test::TestFuture<std::vector<fuchsia::web::Cookie>> result_;
};

}  // namespace

TEST_F(CookieManagerImplTest, GetAndObserveAddModifyDelete) {
  // Add global, URL-filtered and URL+name-filtered observers.
  fuchsia::web::CookiesIteratorPtr global_changes;
  global_changes.set_error_handler([](zx_status_t) { ADD_FAILURE(); });
  cookie_manager_.ObserveCookieChanges(nullptr, nullptr,
                                       global_changes.NewRequest());

  fuchsia::web::CookiesIteratorPtr url_changes;
  url_changes.set_error_handler([](zx_status_t) { ADD_FAILURE(); });
  cookie_manager_.ObserveCookieChanges(kTestCookieUrl, nullptr,
                                       url_changes.NewRequest());

  fuchsia::web::CookiesIteratorPtr name_changes;
  name_changes.set_error_handler([](zx_status_t) { ADD_FAILURE(); });
  cookie_manager_.ObserveCookieChanges(kTestCookieUrl, kCookieName1,
                                       name_changes.NewRequest());

  // Register interest in updates for another URL, so we can verify none are
  // received.
  fuchsia::web::CookiesIteratorPtr other_changes;
  name_changes.set_error_handler([](zx_status_t) { ADD_FAILURE(); });
  cookie_manager_.ObserveCookieChanges(kTestOtherUrl, nullptr,
                                       other_changes.NewRequest());
  GetNextCookiesIteratorResult other_updates(other_changes.get());

  // Ensure that all ObserveCookieChanges() were processed before modifying
  // cookies.
  EXPECT_EQ(GetAllCookies()->size(), 0u);

  // Set cookie kCookieName1, which should trigger notifications to all
  // observers.
  {
    GetNextCookiesIteratorResult global_update(global_changes.get());
    GetNextCookiesIteratorResult url_update(url_changes.get());
    GetNextCookiesIteratorResult name_update(name_changes.get());

    CreateAndSetCookieAsync(kCookieName1, kCookieValue1);

    global_update.ExpectSingleCookie(kCookieName1, kCookieValue1);
    url_update.ExpectSingleCookie(kCookieName1, kCookieValue1);
    name_update.ExpectSingleCookie(kCookieName1, kCookieValue1);
  }

  // Expect an add notification for kCookieName2, except on the with-name
  // observer. If the with-name observer does get notified then the remove &
  // re-add check below will observe kCookieName2 rather than kCookieName1, and
  // fail.
  {
    GetNextCookiesIteratorResult global_update(global_changes.get());
    GetNextCookiesIteratorResult url_update(url_changes.get());

    CreateAndSetCookieAsync(kCookieName2, kCookieValue2);

    global_update.ExpectSingleCookie(kCookieName2, kCookieValue2);
    url_update.ExpectSingleCookie(kCookieName2, kCookieValue2);
  }

  // Set kCookieName1 to a new value, which will trigger deletion notifications,
  // followed by an addition with the new value.
  {
    GetNextCookiesIteratorResult global_update(global_changes.get());
    GetNextCookiesIteratorResult url_update(url_changes.get());
    GetNextCookiesIteratorResult name_update(name_changes.get());

    // Updating the cookie will generate a deletion, following by an insertion.
    // CookiesIterator will batch updates into a single response, so we may get
    // two separate updates, or a single update, depending on timing. Eliminate
    // the non-determinism by ensuring that the GetNext() calls have been
    // received before updating the cookie.
    base::RunLoop().RunUntilIdle();

    CreateAndSetCookieAsync(kCookieName1, kCookieValue3);

    global_update.ExpectDeleteSingleCookie(kCookieName1);
    url_update.ExpectDeleteSingleCookie(kCookieName1);
    name_update.ExpectDeleteSingleCookie(kCookieName1);
  }
  {
    GetNextCookiesIteratorResult global_update(global_changes.get());
    GetNextCookiesIteratorResult url_update(url_changes.get());
    GetNextCookiesIteratorResult name_update(name_changes.get());

    global_update.ExpectSingleCookie(kCookieName1, kCookieValue3);
    url_update.ExpectSingleCookie(kCookieName1, kCookieValue3);
    name_update.ExpectSingleCookie(kCookieName1, kCookieValue3);
  }

  // Set kCookieName2 empty, which will notify only the global and URL
  // observers. If the name observer is mis-notified then the next step, below,
  // will fail.
  {
    GetNextCookiesIteratorResult global_update(global_changes.get());
    GetNextCookiesIteratorResult url_update(url_changes.get());

    DeleteCookieAsync(kCookieName2, kCookieValue2);

    global_update.ExpectDeleteSingleCookie(kCookieName2);
    url_update.ExpectDeleteSingleCookie(kCookieName2);
  }

  // Set kCookieName1 empty, which will notify all the observers that it was
  // removed.
  {
    GetNextCookiesIteratorResult global_update(global_changes.get());
    GetNextCookiesIteratorResult url_update(url_changes.get());
    GetNextCookiesIteratorResult name_update(name_changes.get());

    DeleteCookieAsync(kCookieName1, kCookieValue3);

    global_update.ExpectDeleteSingleCookie(kCookieName1);
    url_update.ExpectDeleteSingleCookie(kCookieName1);
    name_update.ExpectDeleteSingleCookie(kCookieName1);
  }

  // Verify that no updates were received for the "other" URL (since we did not
  // set any cookies for it). It is possible that this could pass due to the
  // CookiesIterator not having been scheduled, but that is very unlikely.
  other_updates.ExpectReceivedNoUpdates();
}

TEST_F(CookieManagerImplTest, UpdateBatching) {
  fuchsia::web::CookiesIteratorPtr global_changes;
  global_changes.set_error_handler([](zx_status_t) { ADD_FAILURE(); });
  cookie_manager_.ObserveCookieChanges(nullptr, nullptr,
                                       global_changes.NewRequest());

  // Ensure that all ObserveCookieChanges() were processed before modifying
  // cookies.
  EXPECT_EQ(GetAllCookies()->size(), 0u);

  {
    // Verify that some insertions are batched into a single GetNext() result.
    CreateAndSetCookieAsync(kCookieName1, kCookieValue1);
    CreateAndSetCookieAsync(kCookieName2, kCookieValue2);
    CreateAndSetCookieAsync(kCookieName1, kCookieValue3);

    // Flush the Cookie Manager so that all cookie changes are processed.
    mojo_cookie_manager_.FlushForTesting();

    // Run all pending tasks so that CookiesIteratorImpl receives all cookie
    // changes through network::mojom::CookieChangeListener::OnCookieChange().
    // This is important because fuchsia::web::CookiesIterator::GetNext() only
    // returns cookie updates that have already been received by the iterator
    // implementation.
    base::RunLoop().RunUntilIdle();

    // Request cookie updates through fuchsia::web::CookiesIterator::GetNext().
    // Multiple updates to the same cookie should be coalesced.
    GetNextCookiesIteratorResult global_updates(global_changes.get());
    global_updates.ExpectCookieUpdates(
        {{kCookieName1, kCookieValue3}, {kCookieName2, kCookieValue2}});
  }

  {
    // Verify that some deletions are batched into a single GetNext() result.
    DeleteCookieAsync(kCookieName2, kCookieValue2);
    DeleteCookieAsync(kCookieName1, kCookieValue3);
    mojo_cookie_manager_.FlushForTesting();

    GetNextCookiesIteratorResult global_updates(global_changes.get());
    global_updates.ExpectCookieUpdates(
        {{kCookieName1, std::nullopt}, {kCookieName2, std::nullopt}});
  }
}

TEST_F(CookieManagerImplTest, ReconnectToNetworkContext) {
  // Attach a cookie observer, which we expect should become disconnected with
  // an appropriate error if the NetworkService goes away.
  base::RunLoop mojo_disconnect_loop;
  cookie_manager_.set_on_mojo_disconnected_for_test(
      mojo_disconnect_loop.QuitClosure());

  // Verify that GetAllCookies() returns a valid list of cookies (as opposed to
  // not returning a list at all) initially.
  EXPECT_TRUE(GetAllCookies().has_value());

  // Tear-down and re-create the NetworkService and |network_context_|, causing
  // the CookieManager's connection to it to be dropped.
  network_service_.reset();
  network_context_.reset();
  network_service_ = network::NetworkService::CreateForTesting();

  // Wait for the |cookie_manager_| to observe the NetworkContext disconnect,
  // so that GetAllCookies() can re-connect.
  mojo_disconnect_loop.Run();

  // If the CookieManager fails to re-connect then GetAllCookies() will receive
  // no data (as opposed to receiving an empty list of cookies).
  EXPECT_TRUE(GetAllCookies().has_value());
}
