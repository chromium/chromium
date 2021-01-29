// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <set>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/cookie_settings.h"
#include "services/network/first_party_sets/first_party_sets.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/test/test_network_context_client.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace network {

class RecordingCookieObserver : public network::mojom::CookieAccessObserver {
 public:
  struct CookieOp {
    mojom::CookieAccessDetails::Type type;
    GURL url;
    GURL site_for_cookies;
    mojom::CookieOrLinePtr cookie_or_line;
    net::CookieInclusionStatus status;

    friend void PrintTo(const CookieOp& op, std::ostream* os) {
      *os << "{type=" << op.type << ", url=" << op.url
          << ", site_for_cookies=" << op.site_for_cookies
          << ", cookie_or_line=(" << CookieOrLineToString(op.cookie_or_line)
          << ", " << static_cast<int>(op.cookie_or_line->which()) << ")"
          << ", status=" << op.status.GetDebugString() << "}";
    }

    static std::string CookieOrLineToString(
        const mojom::CookieOrLinePtr& cookie_or_line) {
      switch (cookie_or_line->which()) {
        case mojom::CookieOrLine::Tag::COOKIE:
          return net::CanonicalCookie::BuildCookieLine(
              {cookie_or_line->get_cookie()});
        case mojom::CookieOrLine::Tag::COOKIE_STRING:
          return cookie_or_line->get_cookie_string();
      }
    }
  };

  RecordingCookieObserver() = default;
  ~RecordingCookieObserver() override = default;

  std::vector<CookieOp>& recorded_activity() { return recorded_activity_; }

  mojo::PendingRemote<mojom::CookieAccessObserver> GetRemote() {
    mojo::PendingRemote<mojom::CookieAccessObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void OnCookiesAccessed(mojom::CookieAccessDetailsPtr details) override {
    for (const auto& cookie_and_access_result : details->cookie_list) {
      CookieOp op;
      op.type = details->type;
      op.url = details->url;
      op.site_for_cookies = details->site_for_cookies.RepresentativeUrl();
      op.cookie_or_line = std::move(cookie_and_access_result->cookie_or_line);
      op.status = cookie_and_access_result->access_result.status;
      recorded_activity_.push_back(std::move(op));
    }
  }

  void Clone(
      mojo::PendingReceiver<mojom::CookieAccessObserver> observer) override {
    receivers_.Add(this, std::move(observer));
  }

 private:
  std::vector<CookieOp> recorded_activity_;
  mojo::ReceiverSet<mojom::CookieAccessObserver> receivers_;
};

// Synchronous proxies to a wrapped RestrictedCookieManager's methods.
class RestrictedCookieManagerSync {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit RestrictedCookieManagerSync(
      mojom::RestrictedCookieManager* cookie_service)
      : cookie_service_(cookie_service) {}
  ~RestrictedCookieManagerSync() = default;

  // Wraps GetAllForUrl() but discards CookieAccessResult from returned cookies.
  std::vector<net::CanonicalCookie> GetAllForUrl(
      const GURL& url,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      mojom::CookieManagerGetOptionsPtr options) {
    base::RunLoop run_loop;
    std::vector<net::CanonicalCookie> result;
    cookie_service_->GetAllForUrl(
        url, net::SiteForCookies::FromUrl(site_for_cookies), top_frame_origin,
        std::move(options),
        base::BindLambdaForTesting(
            [&run_loop, &result](const std::vector<net::CookieWithAccessResult>&
                                     backend_result) {
              result = net::cookie_util::StripAccessResults(backend_result);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  // Returns full CookieWithAccessResult from the backend.
  // TODO(chlily): Convert calls to the above method to this one.
  std::vector<net::CookieWithAccessResult> GetAllForUrlWithAccessResult(
      const GURL& url,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      mojom::CookieManagerGetOptionsPtr options) {
    base::RunLoop run_loop;
    std::vector<net::CookieWithAccessResult> result;
    cookie_service_->GetAllForUrl(
        url, net::SiteForCookies::FromUrl(site_for_cookies), top_frame_origin,
        std::move(options),
        base::BindLambdaForTesting(
            [&run_loop, &result](const std::vector<net::CookieWithAccessResult>&
                                     backend_result) {
              result = backend_result;
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const GURL& site_for_cookies,
                          const url::Origin& top_frame_origin) {
    base::RunLoop run_loop;
    bool result = false;
    cookie_service_->SetCanonicalCookie(
        cookie, url, net::SiteForCookies::FromUrl(site_for_cookies),
        top_frame_origin,
        base::BindLambdaForTesting([&run_loop, &result](bool backend_result) {
          result = backend_result;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void SetCookieFromString(const GURL& url,
                           const net::SiteForCookies& site_for_cookies,
                           const url::Origin& top_frame_origin,
                           const std::string& cookie) {
    base::RunLoop run_loop;
    cookie_service_->SetCookieFromString(
        url, site_for_cookies, top_frame_origin, cookie,
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  void AddChangeListener(
      const GURL& url,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      mojo::PendingRemote<mojom::CookieChangeListener> listener) {
    base::RunLoop run_loop;
    cookie_service_->AddChangeListener(
        url, net::SiteForCookies::FromUrl(site_for_cookies), top_frame_origin,
        std::move(listener), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  mojom::RestrictedCookieManager* cookie_service_;

  DISALLOW_COPY_AND_ASSIGN(RestrictedCookieManagerSync);
};

class RestrictedCookieManagerTest
    : public testing::TestWithParam<mojom::RestrictedCookieManagerRole> {
 public:
  RestrictedCookieManagerTest()
      : cookie_monster_(nullptr, nullptr /* netlog */),
        isolation_info_(net::IsolationInfo::CreateForInternalRequest(
            url::Origin::Create(GURL("https://example.com")))),
        service_(std::make_unique<RestrictedCookieManager>(
            GetParam(),
            &cookie_monster_,
            &cookie_settings_,
            url::Origin::Create(GURL("https://example.com")),
            isolation_info_,
            recording_client_.GetRemote())),
        receiver_(service_.get(),
                  service_remote_.BindNewPipeAndPassReceiver()) {
    sync_service_ =
        std::make_unique<RestrictedCookieManagerSync>(service_remote_.get());
  }
  ~RestrictedCookieManagerTest() override = default;

  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &RestrictedCookieManagerTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  // Set a canonical cookie directly into the store.
  // Default to be a cross-site, cross-party cookie context.
  bool SetCanonicalCookie(
      const net::CanonicalCookie& cookie,
      std::string source_scheme,
      bool can_modify_httponly,
      const net::CookieOptions::SameSiteCookieContext::ContextType
          same_site_cookie_context_type = net::CookieOptions::
              SameSiteCookieContext::ContextType::CROSS_SITE,
      const net::CookieOptions::SamePartyCookieContextType
          same_party_cookie_context_type =
              net::CookieOptions::SamePartyCookieContextType::kCrossParty) {
    net::ResultSavingCookieCallback<net::CookieAccessResult> callback;
    net::CookieOptions options;
    if (can_modify_httponly)
      options.set_include_httponly();
    net::CookieOptions::SameSiteCookieContext same_site_cookie_context(
        same_site_cookie_context_type, same_site_cookie_context_type);
    options.set_same_site_cookie_context(same_site_cookie_context);
    options.set_same_party_cookie_context_type(same_party_cookie_context_type);

    cookie_monster_.SetCanonicalCookieAsync(
        std::make_unique<net::CanonicalCookie>(cookie),
        net::cookie_util::SimulatedCookieSource(cookie, source_scheme), options,
        callback.MakeCallback());
    callback.WaitUntilDone();
    return callback.result().status.IsInclude();
  }

  // Simplified helper for SetCanonicalCookie.
  //
  // Creates a CanonicalCookie that is secure (unless overriden), not http-only,
  // and has SameSite=None. Crashes if the creation fails.
  void SetSessionCookie(const char* name,
                        const char* value,
                        const char* domain,
                        const char* path,
                        bool secure = true) {
    CHECK(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            name, value, domain, path, base::Time(), base::Time(), base::Time(),
            /* secure = */ secure,
            /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
        "https", /* can_modify_httponly = */ true));
  }

  // Like above, but makes an http-only cookie.
  void SetHttpOnlySessionCookie(const char* name,
                                const char* value,
                                const char* domain,
                                const char* path) {
    CHECK(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            name, value, domain, path, base::Time(), base::Time(), base::Time(),
            /* secure = */ true,
            /* httponly = */ true, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
        "https", /* can_modify_httponly = */ true));
  }

  void ExpectBadMessage() { expecting_bad_message_ = true; }

  bool received_bad_message() { return received_bad_message_; }

  mojom::RestrictedCookieManager* backend() { return service_remote_.get(); }

 protected:
  void OnBadMessage(const std::string& reason) {
    EXPECT_TRUE(expecting_bad_message_) << "Unexpected bad message: " << reason;
    received_bad_message_ = true;
  }

  std::vector<RecordingCookieObserver::CookieOp>& recorded_activity() {
    return recording_client_.recorded_activity();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::CookieMonster cookie_monster_;
  CookieSettings cookie_settings_;
  net::IsolationInfo isolation_info_;
  RecordingCookieObserver recording_client_;
  std::unique_ptr<RestrictedCookieManager> service_;
  mojo::Remote<mojom::RestrictedCookieManager> service_remote_;
  mojo::Receiver<mojom::RestrictedCookieManager> receiver_;
  std::unique_ptr<RestrictedCookieManagerSync> sync_service_;
  bool expecting_bad_message_ = false;
  bool received_bad_message_ = false;
};

class SamePartyEnabledRestrictedCookieManagerTest
    : public RestrictedCookieManagerTest {
 public:
  SamePartyEnabledRestrictedCookieManagerTest() {
    feature_list_.InitAndEnableFeature(net::features::kFirstPartySets);

    first_party_sets_.SetManuallySpecifiedSet(
        "https://example.com,https://member1.com");
    auto cookie_access_delegate = std::make_unique<CookieAccessDelegateImpl>(
        mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS,
        &first_party_sets_, &cookie_settings_);
    cookie_monster_.SetCookieAccessDelegate(std::move(cookie_access_delegate));
  }
  ~SamePartyEnabledRestrictedCookieManagerTest() override = default;

  // Set a canonical cookie directly into the store, has both SameSite=lax and
  // SameParty.
  void SetSamePartyCookie(const char* name,
                          const char* value,
                          const char* domain,
                          const char* path,
                          bool secure = true) {
    CHECK(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            name, value, domain, path, base::Time(), base::Time(), base::Time(),
            /* secure = */ secure,
            /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ true),
        "https", /* can_modify_httponly = */ true,
        net::CookieOptions::SameSiteCookieContext::ContextType::SAME_SITE_LAX,
        net::CookieOptions::SamePartyCookieContextType::kSameParty));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  FirstPartySets first_party_sets_;
};

INSTANTIATE_TEST_SUITE_P(
    SameParty,
    SamePartyEnabledRestrictedCookieManagerTest,
    testing::Values(mojom::RestrictedCookieManagerRole::SCRIPT,
                    mojom::RestrictedCookieManagerRole::NETWORK));

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::UnorderedElementsAre;

MATCHER_P5(MatchesCookieOp,
           type,
           url,
           site_for_cookies,
           cookie_or_line,
           status,
           "") {
  return testing::ExplainMatchResult(
      testing::AllOf(
          testing::Field(&RecordingCookieObserver::CookieOp::type, type),
          testing::Field(&RecordingCookieObserver::CookieOp::url, url),
          testing::Field(&RecordingCookieObserver::CookieOp::site_for_cookies,
                         site_for_cookies),
          testing::Field(&RecordingCookieObserver::CookieOp::cookie_or_line,
                         cookie_or_line),
          testing::Field(&RecordingCookieObserver::CookieOp::status, status)),
      arg, result_listener);
}

// Helper for checking that status.HasExactlyExclusionReasonsForTesting(reasons)
// == true.
MATCHER_P(HasExactlyExclusionReasonsForTesting, reasons, "") {
  net::CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(
      true, status.HasExactlyExclusionReasonsForTesting(reasons),
      result_listener);
}

MATCHER(IsInclude, "") {
  net::CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.IsInclude(), result_listener);
}

MATCHER(ShouldWarn, "") {
  net::CookieInclusionStatus status = arg;
  return testing::ExplainMatchResult(true, status.ShouldWarn(),
                                     result_listener);
}

MATCHER_P2(MatchesCookieNameValue, name, value, "") {
  return testing::ExplainMatchResult(
      testing::AllOf(testing::Property(&net::CanonicalCookie::Name, name),
                     testing::Property(&net::CanonicalCookie::Value, value)),
      arg, result_listener);
}

MATCHER_P2(CookieOrLine, string, type, "") {
  return type == arg->which() &&
         testing::ExplainMatchResult(
             string,
             RecordingCookieObserver::CookieOp::CookieOrLineToString(arg),
             result_listener);
}

}  // anonymous namespace

// Test the case when `origin` differs from `isolation_info.frame_origin`.
// RestrictedCookieManager only works for the bound origin and doesn't care
// about the IsolationInfo's frame_origin. Technically this should only happen
// when role == mojom::RestrictedCookieManagerRole::NETWORK.
TEST_P(RestrictedCookieManagerTest,
       GetAllForUrlFromMismatchingIsolationInfoFrameOrigin) {
  GURL top_frame_url("https://example.com");
  GURL resource_url("https://resource.com");
  auto top_frame_origin = url::Origin::Create(top_frame_url);
  auto resource_origin = url::Origin::Create(resource_url);

  service_->OverrideOriginForTesting(resource_origin);
  // Override isolation_info to make it explicit that its frame_origin is
  // different from the origin.
  service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, top_frame_origin,
      top_frame_origin, net::SiteForCookies::FromOrigin(top_frame_origin)));
  SetSessionCookie("new-name", "new-value", resource_url.host().c_str(), "/");

  // Fetch cookies from the wrong origin (IsolationInfo's frame_origin) should
  // result in a bad message.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "new-name";
    options->match_type = mojom::CookieMatchType::EQUALS;
    ExpectBadMessage();
    std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
        top_frame_url, top_frame_url, top_frame_origin, std::move(options));
    EXPECT_TRUE(received_bad_message());
  }
  // Fetch cookies from the correct origin value which RestrictedCookieManager
  // is bound to should work.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "new-name";
    options->match_type = mojom::CookieMatchType::EQUALS;
    std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
        resource_url, top_frame_url, top_frame_origin, std::move(options));

    ASSERT_THAT(cookies, testing::SizeIs(1));

    EXPECT_EQ("new-name", cookies[0].Name());
    EXPECT_EQ("new-value", cookies[0].Value());
  }
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlBlankFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      UnorderedElementsAre(
          MatchesCookieNameValue("cookie-name", "cookie-value"),
          MatchesCookieNameValue("cookie-name-2", "cookie-value-2")));

  // Can also use the document.cookie-style API to get the same info.
  std::string cookies_out;
  EXPECT_TRUE(backend()->GetCookiesString(
      GURL("https://example.com/test/"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")), &cookies_out));
  EXPECT_EQ("cookie-name=cookie-value; cookie-name-2=cookie-value-2",
            cookies_out);
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlEmptyFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      IsEmpty());
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlEqualsMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      ElementsAre(MatchesCookieNameValue("cookie-name", "cookie-value")));
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlStartsWithMatch) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("cookie-name-2b", "cookie-value-2b", "example.com", "/");
  SetSessionCookie("cookie-name-3b", "cookie-value-3b", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name-2";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      UnorderedElementsAre(
          MatchesCookieNameValue("cookie-name-2", "cookie-value-2"),
          MatchesCookieNameValue("cookie-name-2b", "cookie-value-2b")));
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlHttpOnly) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetHttpOnlySessionCookie("cookie-name-http", "cookie-value-2", "example.com",
                           "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com")), std::move(options));

  if (GetParam() == mojom::RestrictedCookieManagerRole::SCRIPT) {
    EXPECT_THAT(cookies, UnorderedElementsAre(MatchesCookieNameValue(
                             "cookie-name", "cookie-value")));
  } else {
    EXPECT_THAT(
        cookies,
        UnorderedElementsAre(
            MatchesCookieNameValue("cookie-name", "cookie-value"),
            MatchesCookieNameValue("cookie-name-http", "cookie-value-2")));
  }
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlFromWrongOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  ExpectBadMessage();
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://not-example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      IsEmpty());
  EXPECT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlFromOpaqueOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  url::Origin opaque_origin;
  ASSERT_TRUE(opaque_origin.opaque());
  service_->OverrideOriginForTesting(opaque_origin);

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  ExpectBadMessage();
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      IsEmpty());
  EXPECT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, GetCookieStringFromWrongOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");

  ExpectBadMessage();
  std::string cookies_out;
  EXPECT_TRUE(backend()->GetCookiesString(
      GURL("https://notexample.com/test/"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")), &cookies_out));
  EXPECT_TRUE(received_bad_message());
  EXPECT_THAT(cookies_out, IsEmpty());
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlPolicy) {
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")));
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  // With default policy, should be able to get all cookies, even third-party.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            GURL("https://example.com/test/"), GURL("https://notexample.com"),
            url::Origin::Create(GURL("https://example.com")),
            std::move(options)),
        ElementsAre(MatchesCookieNameValue("cookie-name", "cookie-value")));
  }

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kRead,
                  "https://example.com/test/", "https://notexample.com/",
                  CookieOrLine("cookie-name=cookie-value",
                               mojom::CookieOrLine::Tag::COOKIE),
                  testing::AllOf(IsInclude(), Not(ShouldWarn())))));

  // Disabing getting third-party cookies works correctly.
  cookie_settings_.set_block_third_party_cookies(true);
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            GURL("https://example.com/test/"), GURL("https://notexample.com"),
            url::Origin::Create(GURL("https://example.com")),
            std::move(options)),
        IsEmpty());
  }

  EXPECT_THAT(
      recorded_activity(),
      ElementsAre(
          testing::_,
          MatchesCookieOp(
              mojom::CookieAccessDetails::Type::kRead,
              "https://example.com/test/", "https://notexample.com/",
              CookieOrLine("cookie-name=cookie-value",
                           mojom::CookieOrLine::Tag::COOKIE),
              HasExactlyExclusionReasonsForTesting(
                  std::vector<net::CookieInclusionStatus::ExclusionReason>{
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}))));
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlPolicyWarnActual) {
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")));
  {
    // Disable kCookiesWithoutSameSiteMustBeSecure to inject such a cookie.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {} /* enabled_features */,
        {net::features::kSameSiteByDefaultCookies,
         net::features::
             kCookiesWithoutSameSiteMustBeSecure} /* disabled_features */);
    SetSessionCookie("cookie-name", "cookie-value", "example.com", "/",
                     /* secure = */ false);
  }

  // Now test get with the feature on.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {net::features::kSameSiteByDefaultCookies,
       net::features::
           kCookiesWithoutSameSiteMustBeSecure} /* enabled_features */,
      {} /* disabled_features */);

  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            GURL("https://example.com/test/"), GURL("https://notexample.com"),
            url::Origin::Create(GURL("https://example.com")),
            std::move(options)),
        IsEmpty());
  }

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kRead,
                  "https://example.com/test/", "https://notexample.com/",
                  CookieOrLine("cookie-name=cookie-value",
                               mojom::CookieOrLine::Tag::COOKIE),
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::
                              EXCLUDE_SAMESITE_NONE_INSECURE}))));
}

TEST_P(SamePartyEnabledRestrictedCookieManagerTest, GetAllForUrlSameParty) {
  SetSamePartyCookie("cookie-name", "cookie-value", "example.com", "/");
  // Same Party
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            GURL("https://example.com/test/"), GURL("https://example.com"),
            url::Origin::Create(GURL("https://example.com")),
            std::move(options)),
        ElementsAre(MatchesCookieNameValue("cookie-name", "cookie-value")));
  }
  // Same Party. `party_context` contains fps site.
  {
    service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://example.com")),
        url::Origin::Create(GURL("https://example.com")), net::SiteForCookies(),
        std::set<net::SchemefulSite>{
            net::SchemefulSite(GURL("https://member1.com"))}));

    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            GURL("https://example.com/test/"), GURL(),
            url::Origin::Create(GURL("https://example.com")),
            std::move(options)),
        ElementsAre(MatchesCookieNameValue("cookie-name", "cookie-value")));
  }

  // Cross Party
  {
    // `party_context` contains cross-party site.
    service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://example.com")),
        url::Origin::Create(GURL("https://example.com")), net::SiteForCookies(),
        std::set<net::SchemefulSite>{
            net::SchemefulSite(GURL("https://nonexample.com"))}));

    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(sync_service_->GetAllForUrl(
                    GURL("https://example.com/test/"), GURL(),
                    url::Origin::Create(GURL("https://example.com")),
                    std::move(options)),
                IsEmpty());

    EXPECT_THAT(
        recorded_activity(),
        ElementsAre(
            testing::_, testing::_,
            MatchesCookieOp(
                testing::_ /* type */, testing::_ /* url */,
                testing::_ /* site_for_cookies */,
                testing::_ /* cookie_or_line */,
                HasExactlyExclusionReasonsForTesting(
                    std::vector<net::CookieInclusionStatus::ExclusionReason>{
                        net::CookieInclusionStatus::
                            EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT}))));
  }
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookie) {
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ true,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com"))));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      ElementsAre(MatchesCookieNameValue("new-name", "new-value")));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieHttpOnly) {
  EXPECT_EQ(GetParam() == mojom::RestrictedCookieManagerRole::NETWORK,
            sync_service_->SetCanonicalCookie(
                *net::CanonicalCookie::CreateUnsafeCookieForTesting(
                    "new-name", "new-value", "example.com", "/", base::Time(),
                    base::Time(), base::Time(), /* secure = */ true,
                    /* httponly = */ true, net::CookieSameSite::NO_RESTRICTION,
                    net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
                GURL("https://example.com/test/"), GURL("https://example.com"),
                url::Origin::Create(GURL("https://example.com"))));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com")), std::move(options));

  if (GetParam() == mojom::RestrictedCookieManagerRole::SCRIPT) {
    EXPECT_THAT(cookies, IsEmpty());
  } else {
    EXPECT_THAT(cookies,
                ElementsAre(MatchesCookieNameValue("new-name", "new-value")));
  }
}

TEST_P(RestrictedCookieManagerTest, SetCookieFromString) {
  EXPECT_TRUE(backend()->SetCookieFromString(
      GURL("https://example.com/test/"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")),
      "new-name=new-value;path=/"));
  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      ElementsAre(MatchesCookieNameValue("new-name", "new-value")));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieFromWrongOrigin) {
  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ true,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      GURL("https://not-example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com"))));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieFromOpaqueOrigin) {
  url::Origin opaque_origin;
  ASSERT_TRUE(opaque_origin.opaque());
  service_->OverrideOriginForTesting(opaque_origin);

  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ true,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com"))));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieWithMismatchingDomain) {
  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), /* secure = */ true,
          /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com"))));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCookieFromStringWrongOrigin) {
  ExpectBadMessage();
  EXPECT_TRUE(backend()->SetCookieFromString(
      GURL("https://notexample.com/test/"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")),
      "new-name=new-value;path=/"));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookiePolicy) {
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")));
  {
    // With default settings object, setting a third-party cookie is OK.
    auto cookie = net::CanonicalCookie::Create(
        GURL("https://example.com"), "A=B; SameSite=none; Secure",
        base::Time::Now(), base::nullopt /* server_time */);
    EXPECT_TRUE(sync_service_->SetCanonicalCookie(
        *cookie, GURL("https://example.com"), GURL("https://notexample.com"),
        url::Origin::Create(GURL("https://example.com"))));
  }

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kChange,
                  "https://example.com/", "https://notexample.com/",
                  CookieOrLine("A=B", mojom::CookieOrLine::Tag::COOKIE),
                  testing::AllOf(IsInclude(), Not(ShouldWarn())))));

  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://otherexample.com")));
  {
    // Not if third-party cookies are disabled, though.
    cookie_settings_.set_block_third_party_cookies(true);
    auto cookie = net::CanonicalCookie::Create(
        GURL("https://example.com"), "A2=B2; SameSite=none; Secure",
        base::Time::Now(), base::nullopt /* server_time */);
    EXPECT_FALSE(sync_service_->SetCanonicalCookie(
        *cookie, GURL("https://example.com"), GURL("https://otherexample.com"),
        url::Origin::Create(GURL("https://example.com"))));
  }

  EXPECT_THAT(
      recorded_activity(),
      ElementsAre(
          testing::_,
          MatchesCookieOp(
              mojom::CookieAccessDetails::Type::kChange, "https://example.com/",
              "https://otherexample.com/",
              CookieOrLine("A2=B2", mojom::CookieOrLine::Tag::COOKIE),
              HasExactlyExclusionReasonsForTesting(
                  std::vector<net::CookieInclusionStatus::ExclusionReason>{
                      net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}))));

  // Read back, in first-party context
  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "A";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;

  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://example.com")));
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          GURL("https://example.com/test/"), GURL("https://example.com/"),
          url::Origin::Create(GURL("https://example.com")), std::move(options)),
      ElementsAre(MatchesCookieNameValue("A", "B")));

  EXPECT_THAT(
      recorded_activity(),
      ElementsAre(
          testing::_, testing::_,
          MatchesCookieOp(mojom::CookieAccessDetails::Type::kRead,
                          "https://example.com/test/", "https://example.com/",
                          CookieOrLine("A=B", mojom::CookieOrLine::Tag::COOKIE),
                          IsInclude())));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookiePolicyWarnActual) {
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")));
  // Make sure the deprecation warnings are also produced when the feature
  // to enable the new behavior is on.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kSameSiteByDefaultCookies);

  auto cookie = net::CanonicalCookie::Create(GURL("https://example.com"), "A=B",
                                             base::Time::Now(),
                                             base::nullopt /* server_time */);
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *cookie, GURL("https://example.com"), GURL("https://notexample.com"),
      url::Origin::Create(GURL("https://example.com"))));

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kChange,
                  "https://example.com/", "https://notexample.com/",
                  CookieOrLine("A=B", mojom::CookieOrLine::Tag::COOKIE),
                  HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::
                              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}))));
}

TEST_P(SamePartyEnabledRestrictedCookieManagerTest,
       SetCookieFromString_SameParty_ReportsInvalid) {
  // Invalid. Should be reported.
  sync_service_->SetCookieFromString(
      GURL("https://example.com/test/"), net::SiteForCookies(),
      url::Origin::Create(GURL("https://example.com")), "name=value;SameParty");

  EXPECT_THAT(
      recorded_activity(),
      ElementsAre(MatchesCookieOp(
          mojom::CookieAccessDetails::Type::kChange,
          GURL("https://example.com/test/"), GURL(),
          CookieOrLine("name=value;SameParty",
                       mojom::CookieOrLine::Tag::COOKIE_STRING),
          HasExactlyExclusionReasonsForTesting(
              std::vector<net::CookieInclusionStatus::ExclusionReason>{
                  net::CookieInclusionStatus::EXCLUDE_INVALID_SAMEPARTY}))));
}

TEST_P(SamePartyEnabledRestrictedCookieManagerTest,
       SetCanonicalCookieSameParty) {
  // Same Party. `party_context` contains fps site.
  {
    service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://example.com")),
        url::Origin::Create(GURL("https://member1.com")), net::SiteForCookies(),
        std::set<net::SchemefulSite>{
            net::SchemefulSite(GURL("https://member1.com"))}));
    // Need to override origin as well since the access is from member1.com.
    service_->OverrideOriginForTesting(
        url::Origin::Create(GURL("https://member1.com")));

    EXPECT_TRUE(sync_service_->SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "new-name", "new-value", "member1.com", "/", base::Time(),
            base::Time(), base::Time(), /* secure = */ true,
            /* httponly = */ false, net::CookieSameSite::LAX_MODE,
            net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ true),
        GURL("https://member1.com/test/"), GURL(),
        url::Origin::Create(GURL("https://example.com"))));

    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "new-name";
    options->match_type = mojom::CookieMatchType::EQUALS;
    EXPECT_THAT(sync_service_->GetAllForUrl(
                    GURL("https://member1.com/test/"), GURL(),
                    url::Origin::Create(GURL("https://example.com")),
                    std::move(options)),
                ElementsAre(MatchesCookieNameValue("new-name", "new-value")));
  }

  // Cross Party
  {
    service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://example.com")),
        url::Origin::Create(GURL("https://example.com")), net::SiteForCookies(),
        std::set<net::SchemefulSite>{
            net::SchemefulSite(GURL("https://not-example.com"))}));
    // Need to restore the origin value since the previous Same Party test case
    // changed it to member1.com.
    service_->OverrideOriginForTesting(
        url::Origin::Create(GURL("https://example.com")));

    EXPECT_FALSE(sync_service_->SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            "new-name", "new-value", "example.com", "/", base::Time(),
            base::Time(), base::Time(), /* secure = */ true,
            /* httponly = */ false, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ true),
        GURL("https://example.com/test/"), GURL(),
        url::Origin::Create(GURL("https://example.com"))));

    EXPECT_THAT(
        recorded_activity(),
        ElementsAre(
            testing::_, testing::_,
            MatchesCookieOp(
                mojom::CookieAccessDetails::Type::kChange,
                "https://example.com/test/", GURL(),
                CookieOrLine("new-name=new-value",
                             mojom::CookieOrLine::Tag::COOKIE),
                HasExactlyExclusionReasonsForTesting(
                    std::vector<net::CookieInclusionStatus::ExclusionReason>{
                        net::CookieInclusionStatus::
                            EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT}))));
  }
}

TEST_P(RestrictedCookieManagerTest, CookiesEnabledFor) {
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")));
  // Default, third-party access is OK.
  bool result = false;
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      GURL("https://example.com"),
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")),
      url::Origin::Create(GURL("https://example.com")), &result));
  EXPECT_TRUE(result);

  // Third-part cookies disabled.
  cookie_settings_.set_block_third_party_cookies(true);
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      GURL("https://example.com"),
      net::SiteForCookies::FromUrl(GURL("https://notexample.com")),
      url::Origin::Create(GURL("https://example.com")), &result));
  EXPECT_FALSE(result);

  // First-party ones still OK.
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://example.com")));
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      GURL("https://example.com"),
      net::SiteForCookies::FromUrl(GURL("https://example.com")),
      url::Origin::Create(GURL("https://example.com")), &result));
  EXPECT_TRUE(result);
}

// Test that special chrome:// scheme always attaches SameSite cookies when the
// requested origin is secure.
TEST_P(RestrictedCookieManagerTest, SameSiteCookiesSpecialScheme) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  cookie_settings_.set_secure_origin_cookies_allowed_schemes({"chrome"});
  url::AddStandardScheme("chrome", url::SchemeType::SCHEME_WITH_HOST);

  GURL extension_url("chrome-extension://abcdefghijklmnopqrstuvwxyz");
  GURL chrome_url("chrome://whatever");
  GURL http_url("http://example.com/test");
  GURL https_url("https://example.com/test");
  auto http_origin = url::Origin::Create(http_url);
  auto https_origin = url::Origin::Create(https_url);

  // Test if site_for_cookies is chrome, then SameSite cookies can be
  // set and gotten if the origin is secure.
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(chrome_url));
  service_->OverrideOriginForTesting(https_origin);
  service_->OverrideTopFrameOriginForTesting(https_origin);
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "strict-cookie", "1", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      https_url, chrome_url, https_origin));
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "lax-cookie", "1", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      https_url, chrome_url, https_origin));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  EXPECT_THAT(sync_service_->GetAllForUrl(https_url, chrome_url, https_origin,
                                          std::move(options)),
              testing::SizeIs(2));

  // Test if site_for_cookies is chrome, then SameSite cookies cannot be
  // set and gotten if the origin is not secure.
  service_->OverrideOriginForTesting(http_origin);
  service_->OverrideTopFrameOriginForTesting(http_origin);
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "strict-cookie", "2", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      http_url, chrome_url, http_origin));
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "lax-cookie", "2", "example.com", "/", base::Time(), base::Time(),
          base::Time(), /* secure = */ false,
          /* httponly = */ false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT, /* same_party = */ false),
      http_url, chrome_url, http_origin));

  options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  EXPECT_THAT(sync_service_->GetAllForUrl(http_url, chrome_url, http_origin,
                                          std::move(options)),
              IsEmpty());
}

namespace {

// Stashes the cookie changes it receives, for testing.
class TestCookieChangeListener : public network::mojom::CookieChangeListener {
 public:
  explicit TestCookieChangeListener(
      mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~TestCookieChangeListener() override = default;

  // Spin in a run loop until a change is received.
  void WaitForChange() {
    base::RunLoop loop;
    run_loop_ = &loop;
    loop.Run();
    run_loop_ = nullptr;
  }

  // Changes received by this listener.
  const std::vector<net::CookieChangeInfo>& observed_changes() const {
    return observed_changes_;
  }

  // network::mojom::CookieChangeListener
  void OnCookieChange(const net::CookieChangeInfo& change) override {
    observed_changes_.emplace_back(change);

    if (run_loop_)  // Set in WaitForChange().
      run_loop_->Quit();
  }

 private:
  std::vector<net::CookieChangeInfo> observed_changes_;
  mojo::Receiver<network::mojom::CookieChangeListener> receiver_;

  // If not null, will be stopped when a cookie change notification is received.
  base::RunLoop* run_loop_ = nullptr;
};

}  // anonymous namespace

TEST_P(RestrictedCookieManagerTest, ChangeDispatch) {
  mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
      listener_remote.InitWithNewPipeAndPassReceiver();
  sync_service_->AddChangeListener(
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com")),
      std::move(listener_remote));
  TestCookieChangeListener listener(std::move(receiver));

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));

  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  listener.WaitForChange();

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            listener.observed_changes()[0].cause);
  EXPECT_THAT(listener.observed_changes()[0].cookie,
              MatchesCookieNameValue("cookie-name", "cookie-value"));
}

TEST_P(RestrictedCookieManagerTest, ChangeSettings) {
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://notexample.com/")));
  mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
      listener_remote.InitWithNewPipeAndPassReceiver();
  sync_service_->AddChangeListener(
      GURL("https://example.com/test/"), GURL("https://notexample.com"),
      url::Origin::Create(GURL("https://example.com")),
      std::move(listener_remote));
  TestCookieChangeListener listener(std::move(receiver));

  EXPECT_THAT(listener.observed_changes(), IsEmpty());

  cookie_settings_.set_block_third_party_cookies(true);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(listener.observed_changes(), IsEmpty());
}

TEST_P(RestrictedCookieManagerTest, AddChangeListenerFromWrongOrigin) {
  mojo::PendingRemote<network::mojom::CookieChangeListener> bad_listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> bad_receiver =
      bad_listener_remote.InitWithNewPipeAndPassReceiver();
  ExpectBadMessage();
  sync_service_->AddChangeListener(
      GURL("https://not-example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com")),
      std::move(bad_listener_remote));
  EXPECT_TRUE(received_bad_message());
  TestCookieChangeListener bad_listener(std::move(bad_receiver));

  mojo::PendingRemote<network::mojom::CookieChangeListener>
      good_listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> good_receiver =
      good_listener_remote.InitWithNewPipeAndPassReceiver();
  sync_service_->AddChangeListener(
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com")),
      std::move(good_listener_remote));
  TestCookieChangeListener good_listener(std::move(good_receiver));

  ASSERT_THAT(bad_listener.observed_changes(), IsEmpty());
  ASSERT_THAT(good_listener.observed_changes(), IsEmpty());

  SetSessionCookie("other-cookie-name", "other-cookie-value", "not-example.com",
                   "/");
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  good_listener.WaitForChange();

  EXPECT_THAT(bad_listener.observed_changes(), IsEmpty());

  ASSERT_THAT(good_listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            good_listener.observed_changes()[0].cause);
  EXPECT_THAT(good_listener.observed_changes()[0].cookie,
              MatchesCookieNameValue("cookie-name", "cookie-value"));
}

TEST_P(RestrictedCookieManagerTest, AddChangeListenerFromOpaqueOrigin) {
  url::Origin opaque_origin;
  ASSERT_TRUE(opaque_origin.opaque());
  service_->OverrideOriginForTesting(opaque_origin);

  mojo::PendingRemote<network::mojom::CookieChangeListener> bad_listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> bad_receiver =
      bad_listener_remote.InitWithNewPipeAndPassReceiver();
  ExpectBadMessage();
  sync_service_->AddChangeListener(
      GURL("https://example.com/test/"), GURL("https://example.com"),
      url::Origin::Create(GURL("https://example.com")),
      std::move(bad_listener_remote));
  EXPECT_TRUE(received_bad_message());

  TestCookieChangeListener bad_listener(std::move(bad_receiver));
  ASSERT_THAT(bad_listener.observed_changes(), IsEmpty());
}

TEST_P(SamePartyEnabledRestrictedCookieManagerTest,
       AddChangeListenerSameParty) {
  // Same Party. `party_context` contains fps site.
  {
    service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://example.com")),
        url::Origin::Create(GURL("https://example.com")), net::SiteForCookies(),
        std::set<net::SchemefulSite>{
            net::SchemefulSite(GURL("https://member1.com"))}));

    mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
    mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
        listener_remote.InitWithNewPipeAndPassReceiver();
    sync_service_->AddChangeListener(
        GURL("https://example.com/test/"), GURL(),
        url::Origin::Create(GURL("https://example.com")),
        std::move(listener_remote));
    TestCookieChangeListener listener(std::move(receiver));

    ASSERT_THAT(listener.observed_changes(), IsEmpty());

    SetSamePartyCookie("cookie-name", "cookie-value", "example.com", "/");
    listener.WaitForChange();

    ASSERT_THAT(listener.observed_changes(), testing::SizeIs(1));
    EXPECT_EQ(net::CookieChangeCause::INSERTED,
              listener.observed_changes()[0].cause);
    EXPECT_THAT(listener.observed_changes()[0].cookie,
                MatchesCookieNameValue("cookie-name", "cookie-value"));
  }
  // Cross Party.
  {
    service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kOther,
        url::Origin::Create(GURL("https://example.com")),
        url::Origin::Create(GURL("https://example.com")), net::SiteForCookies(),
        std::set<net::SchemefulSite>{
            net::SchemefulSite(GURL("https://not-example.com"))}));

    mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
    mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
        listener_remote.InitWithNewPipeAndPassReceiver();
    sync_service_->AddChangeListener(
        GURL("https://example.com/test/"), GURL(),
        url::Origin::Create(GURL("https://example.com")),
        std::move(listener_remote));
    TestCookieChangeListener listener(std::move(receiver));

    EXPECT_THAT(listener.observed_changes(), IsEmpty());
    SetSamePartyCookie("cookie-name", "cookie-value", "example.com", "/");
    EXPECT_THAT(listener.observed_changes(), IsEmpty());
  }
}

// Test that the Change listener receives the access semantics, and that they
// are taken into account when deciding when to dispatch the change.
TEST_P(RestrictedCookieManagerTest, ChangeNotificationIncludesAccessSemantics) {
  // Turn on SameSiteByDefaultCookies.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kSameSiteByDefaultCookies);

  auto cookie_access_delegate =
      std::make_unique<net::TestCookieAccessDelegate>();
  cookie_access_delegate->SetExpectationForCookieDomain(
      "example.com", net::CookieAccessSemantics::LEGACY);
  cookie_monster_.SetCookieAccessDelegate(std::move(cookie_access_delegate));

  mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
      listener_remote.InitWithNewPipeAndPassReceiver();

  // Use a cross-site site_for_cookies.
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://not-example.com")));
  sync_service_->AddChangeListener(
      GURL("https://example.com/test/"),
      GURL("https://not-example.com") /* site_for_cookies */,
      url::Origin::Create(GURL("https://example.com")),
      std::move(listener_remote));
  TestCookieChangeListener listener(std::move(receiver));

  ASSERT_THAT(listener.observed_changes(), IsEmpty());

  GURL cookie_url("https://example.com");
  auto cookie = net::CanonicalCookie::Create(
      cookie_url, "cookie_with_no_samesite=unspecified", base::Time::Now(),
      base::nullopt);

  // Set cookie directly into the CookieMonster, using all-inclusive options.
  net::ResultSavingCookieCallback<net::CookieAccessResult> callback;
  cookie_monster_.SetCanonicalCookieAsync(
      std::move(cookie), cookie_url, net::CookieOptions::MakeAllInclusive(),
      callback.MakeCallback());
  callback.WaitUntilDone();
  ASSERT_TRUE(callback.result().status.IsInclude());

  // The listener only receives the change because the cookie is legacy.
  listener.WaitForChange();

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            listener.observed_changes()[0].access_result.access_semantics);
}

TEST_P(RestrictedCookieManagerTest, NoChangeNotificationForNonlegacyCookie) {
  // Turn on SameSiteByDefaultCookies.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kSameSiteByDefaultCookies);

  auto cookie_access_delegate =
      std::make_unique<net::TestCookieAccessDelegate>();
  cookie_access_delegate->SetExpectationForCookieDomain(
      "example.com", net::CookieAccessSemantics::NONLEGACY);
  cookie_monster_.SetCookieAccessDelegate(std::move(cookie_access_delegate));

  mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
      listener_remote.InitWithNewPipeAndPassReceiver();

  // Use a cross-site site_for_cookies.
  service_->OverrideSiteForCookiesForTesting(
      net::SiteForCookies::FromUrl(GURL("https://not-example.com")));
  sync_service_->AddChangeListener(
      GURL("https://example.com/test/"),
      GURL("https://not-example.com") /* site_for_cookies */,
      url::Origin::Create(GURL("https://example.com")),
      std::move(listener_remote));
  TestCookieChangeListener listener(std::move(receiver));

  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));

  GURL cookie_url("https://example.com");

  auto unspecified_cookie = net::CanonicalCookie::Create(
      cookie_url, "cookie_with_no_samesite=unspecified", base::Time::Now(),
      base::nullopt);

  auto samesite_none_cookie = net::CanonicalCookie::Create(
      cookie_url, "samesite_none_cookie=none; SameSite=None; Secure",
      base::Time::Now(), base::nullopt);

  // Set cookies directly into the CookieMonster, using all-inclusive options.
  net::ResultSavingCookieCallback<net::CookieAccessResult> callback1;
  cookie_monster_.SetCanonicalCookieAsync(
      std::move(unspecified_cookie), cookie_url,
      net::CookieOptions::MakeAllInclusive(), callback1.MakeCallback());
  callback1.WaitUntilDone();
  ASSERT_TRUE(callback1.result().status.IsInclude());

  // Listener doesn't receive notification because cookie is not included for
  // request URL for being unspecified and treated as lax.
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(0));

  net::ResultSavingCookieCallback<net::CookieAccessResult> callback2;
  cookie_monster_.SetCanonicalCookieAsync(
      std::move(samesite_none_cookie), cookie_url,
      net::CookieOptions::MakeAllInclusive(), callback2.MakeCallback());
  callback2.WaitUntilDone();
  ASSERT_TRUE(callback2.result().status.IsInclude());

  // Listener only receives notification about the SameSite=None cookie.
  listener.WaitForChange();
  ASSERT_THAT(listener.observed_changes(), testing::SizeIs(1));

  EXPECT_EQ("samesite_none_cookie",
            listener.observed_changes()[0].cookie.Name());
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            listener.observed_changes()[0].access_result.access_semantics);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RestrictedCookieManagerTest,
    ::testing::Values(mojom::RestrictedCookieManagerRole::SCRIPT,
                      mojom::RestrictedCookieManagerRole::NETWORK));

}  // namespace network
