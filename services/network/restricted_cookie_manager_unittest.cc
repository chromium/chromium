// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/restricted_cookie_manager.h"

#include <set>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_test_callbacks.h"
#include "net/cookies/cookie_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/cookies/test_cookie_access_delegate.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "net/storage_access_api/status.h"
#include "services/network/cookie_access_delegate_impl.h"
#include "services/network/cookie_settings.h"
#include "services/network/first_party_sets/first_party_sets_access_delegate.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-shared.h"
#include "services/network/test/test_network_context_client.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"
#include "url/url_util.h"

using testing::AllOf;

namespace net {
bool operator==(const net::SiteForCookies& a, const net::SiteForCookies& b) {
  return a.IsEquivalent(b);
}
}  // namespace net

namespace network {

namespace {

net::FirstPartySetMetadata ComputeFirstPartySetMetadataSync(
    const url::Origin& origin,
    const net::CookieStore* cookie_store,
    const net::IsolationInfo& isolation_info) {
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  RestrictedCookieManager::ComputeFirstPartySetMetadata(
      origin, cookie_store, isolation_info, future.GetCallback());
  return future.Take();
}

}  // namespace

class RecordingCookieObserver : public network::mojom::CookieAccessObserver {
 public:
  struct CookieOp {
    mojom::CookieAccessDetails::Type type;
    GURL url;
    net::SiteForCookies site_for_cookies;
    mojom::CookieOrLinePtr cookie_or_line;
    net::CookieInclusionStatus status;
    bool is_ad_tagged;

    friend void PrintTo(const CookieOp& op, std::ostream* os) {
      *os << "{type=" << op.type << ", url=" << op.url
          << ", site_for_cookies=" << op.site_for_cookies.RepresentativeUrl()
          << ", cookie_or_line=(" << CookieOrLineToString(op.cookie_or_line)
          << ", " << static_cast<int>(op.cookie_or_line->which()) << ")"
          << ", status=" << op.status.GetDebugString()
          << ", is_ad_tagged=" << op.is_ad_tagged << "}";
    }

    static std::string CookieOrLineToString(
        const mojom::CookieOrLinePtr& cookie_or_line) {
      switch (cookie_or_line->which()) {
        case mojom::CookieOrLine::Tag::kCookie:
          return net::CanonicalCookie::BuildCookieLine(
              {cookie_or_line->get_cookie()});
        case mojom::CookieOrLine::Tag::kCookieString:
          return cookie_or_line->get_cookie_string();
      }
    }
  };

  RecordingCookieObserver() : run_loop_(std::make_unique<base::RunLoop>()) {}

  ~RecordingCookieObserver() override = default;

  std::vector<CookieOp>& recorded_activity() { return recorded_activity_; }

  mojo::PendingRemote<mojom::CookieAccessObserver> GetRemote() {
    mojo::PendingRemote<mojom::CookieAccessObserver> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void WaitForCallback() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void OnCookiesAccessed(std::vector<network::mojom::CookieAccessDetailsPtr>
                             details_vector) override {
    for (auto& details : details_vector) {
      for (const auto& cookie_and_access_result : details->cookie_list) {
        CookieOp op;
        op.type = details->type;
        op.url = details->url;
        op.site_for_cookies = details->site_for_cookies;
        op.cookie_or_line = std::move(cookie_and_access_result->cookie_or_line);
        op.status = cookie_and_access_result->access_result.status;
        op.is_ad_tagged = details->is_ad_tagged;
        recorded_activity_.push_back(std::move(op));
      }
    }

    run_loop_->QuitClosure().Run();
  }

  void Clone(
      mojo::PendingReceiver<mojom::CookieAccessObserver> observer) override {
    receivers_.Add(this, std::move(observer));
  }

 private:
  std::vector<CookieOp> recorded_activity_;
  mojo::ReceiverSet<mojom::CookieAccessObserver> receivers_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Synchronous proxies to a wrapped RestrictedCookieManager's methods.
class RestrictedCookieManagerSync {
 public:
  // Caller must guarantee that |*cookie_service| outlives the
  // SynchronousCookieManager.
  explicit RestrictedCookieManagerSync(
      mojom::RestrictedCookieManager* cookie_service)
      : cookie_service_(cookie_service) {}

  RestrictedCookieManagerSync(const RestrictedCookieManagerSync&) = delete;
  RestrictedCookieManagerSync& operator=(const RestrictedCookieManagerSync&) =
      delete;

  ~RestrictedCookieManagerSync() = default;

  // Wraps GetAllForUrl() but discards CookieAccessResult from returned cookies.
  std::vector<net::CanonicalCookie> GetAllForUrl(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      mojom::CookieManagerGetOptionsPtr options,
      bool is_ad_tagged = false,
      bool force_disable_third_party_cookies = false) {
    base::test::TestFuture<const std::vector<net::CookieWithAccessResult>&>
        future;
    cookie_service_->GetAllForUrl(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        std::move(options), is_ad_tagged, force_disable_third_party_cookies,
        future.GetCallback());
    return net::cookie_util::StripAccessResults(future.Take());
  }

  bool SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          const GURL& url,
                          const net::SiteForCookies& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          net::StorageAccessApiStatus storage_access_api_status,
                          std::optional<net::CookieInclusionStatus>
                              cookie_inclusion_status = std::nullopt) {
    net::CookieInclusionStatus status = cookie_inclusion_status.has_value()
                                            ? cookie_inclusion_status.value()
                                            : net::CookieInclusionStatus();
    base::test::TestFuture<bool> future;
    cookie_service_->SetCanonicalCookie(
        cookie, url, site_for_cookies, top_frame_origin,
        storage_access_api_status, status, future.GetCallback());
    return future.Get();
  }

  void SetCookieFromString(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      const std::string& cookie) {
    base::test::TestFuture<void> future;
    cookie_service_->SetCookieFromString(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        cookie, future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  void AddChangeListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status,
      mojo::PendingRemote<mojom::CookieChangeListener> listener) {
    base::RunLoop run_loop;
    cookie_service_->AddChangeListener(
        url, site_for_cookies, top_frame_origin, storage_access_api_status,
        std::move(listener), run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  raw_ptr<mojom::RestrictedCookieManager> cookie_service_;
};

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
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

}  // namespace

class RestrictedCookieManagerTest
    : public testing::TestWithParam<mojom::RestrictedCookieManagerRole> {
 public:
  RestrictedCookieManagerTest()
      : cookie_monster_(/*store=*/nullptr,
                        /*net_log=*/nullptr),
        isolation_info_(kDefaultIsolationInfo),
        service_(std::make_unique<RestrictedCookieManager>(
            RestrictedCookieManagerRole(),
            &cookie_monster_,
            cookie_settings_,
            kDefaultOrigin,
            isolation_info_,
            CookieSettingOverrides(),
            recording_client_.GetRemote(),
            ComputeFirstPartySetMetadataSync(kDefaultOrigin,
                                             &cookie_monster_,
                                             isolation_info_))),
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

  mojom::RestrictedCookieManagerRole RestrictedCookieManagerRole() const {
    return GetParam();
  }

  net::CookieSettingOverrides CookieSettingOverrides() const {
    return net::CookieSettingOverrides();
  }

  // Set a canonical cookie directly into the store.
  // Default to be a cross-site, cross-party cookie context.
  bool SetCanonicalCookie(
      const net::CanonicalCookie& cookie,
      std::string source_scheme,
      bool can_modify_httponly,
      const net::CookieOptions::SameSiteCookieContext::ContextType
          same_site_cookie_context_type = net::CookieOptions::
              SameSiteCookieContext::ContextType::CROSS_SITE) {
    net::ResultSavingCookieCallback<net::CookieAccessResult> callback;
    net::CookieOptions options;
    if (can_modify_httponly)
      options.set_include_httponly();
    net::CookieOptions::SameSiteCookieContext same_site_cookie_context(
        same_site_cookie_context_type, same_site_cookie_context_type);
    options.set_same_site_cookie_context(same_site_cookie_context);

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
    ASSERT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            name, value, domain, path, base::Time(), base::Time(), base::Time(),
            base::Time(),
            /*secure=*/secure,
            /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT),
        "https", /*can_modify_httponly=*/true));
  }

  // Like above, but makes an http-only cookie.
  void SetHttpOnlySessionCookie(const char* name,
                                const char* value,
                                const char* domain,
                                const char* path) {
    CHECK(SetCanonicalCookie(
        *net::CanonicalCookie::CreateUnsafeCookieForTesting(
            name, value, domain, path, base::Time(), base::Time(), base::Time(),
            base::Time(),
            /*secure=*/true,
            /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
            net::COOKIE_PRIORITY_DEFAULT),
        "https", /*can_modify_httponly=*/true));
  }

  std::unique_ptr<TestCookieChangeListener> CreateCookieChangeListener(
      const GURL& url,
      const net::SiteForCookies& site_for_cookies,
      const url::Origin& top_frame_origin,
      net::StorageAccessApiStatus storage_access_api_status) {
    mojo::PendingRemote<network::mojom::CookieChangeListener> listener_remote;
    mojo::PendingReceiver<network::mojom::CookieChangeListener> receiver =
        listener_remote.InitWithNewPipeAndPassReceiver();
    sync_service_->AddChangeListener(url, site_for_cookies, top_frame_origin,
                                     storage_access_api_status,
                                     std::move(listener_remote));
    return std::make_unique<TestCookieChangeListener>(std::move(receiver));
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

  void WaitForCallback() { return recording_client_.WaitForCallback(); }

  const GURL kDefaultUrl{"https://example.com/"};
  const GURL kDefaultUrlWithPath{"https://example.com/test/"};
  const GURL kOtherUrl{"https://notexample.com/"};
  const GURL kOtherUrlWithPath{"https://notexample.com/test/"};
  const url::Origin kDefaultOrigin = url::Origin::Create(kDefaultUrl);
  const url::Origin kOtherOrigin = url::Origin::Create(kOtherUrl);
  const net::SiteForCookies kDefaultSiteForCookies =
      net::SiteForCookies::FromUrl(kDefaultUrl);
  const net::SiteForCookies kOtherSiteForCookies =
      net::SiteForCookies::FromUrl(kOtherUrl);
  const net::IsolationInfo kDefaultIsolationInfo =
      net::IsolationInfo::CreateForInternalRequest(kDefaultOrigin);
  // IsolationInfo that replaces the default SiteForCookies with a blank one.
  const net::IsolationInfo kOtherIsolationInfo =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 kDefaultOrigin,
                                 kDefaultOrigin,
                                 net::SiteForCookies());
  // IsolationInfo that uses `kOtherOrigin` as the top-level and
  // `kDefaultOrigin` as the embedded origin.
  const net::IsolationInfo kThirdPartyIsolationInfo =
      net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                 kOtherOrigin,
                                 kDefaultOrigin,
                                 net::SiteForCookies());

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

namespace {

using testing::Contains;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::IsEmpty;
using testing::Not;
using testing::UnorderedElementsAre;

using CanonicalCookieMatcher =
    net::MatchesCookieNameValueMatcherP2<const char*, const char*>;

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
// when role == mojom::RestrictedCookieManagerRole::NETWORK. Otherwise, it
// should trigger a CHECK in the RestrictedCookieManager constructor (which is
// bypassed here due to constructing it properly, then using an origin
// override).
TEST_P(RestrictedCookieManagerTest,
       GetAllForUrlFromMismatchingIsolationInfoFrameOrigin) {
  service_->OverrideOriginForTesting(kOtherOrigin);
  // Override isolation_info to make it explicit that its frame_origin is
  // different from the origin.
  service_->OverrideIsolationInfoForTesting(kDefaultIsolationInfo);
  SetSessionCookie("new-name", "new-value", kOtherUrl.host().c_str(), "/");

  // Fetch cookies from the wrong origin (IsolationInfo's frame_origin) should
  // result in a bad message.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "new-name";
    options->match_type = mojom::CookieMatchType::EQUALS;
    ExpectBadMessage();
    std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
        kDefaultUrl, kDefaultSiteForCookies, kDefaultOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));
    EXPECT_TRUE(received_bad_message());
  }
  // Fetch cookies from the correct origin value which RestrictedCookieManager
  // is bound to should work.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "new-name";
    options->match_type = mojom::CookieMatchType::EQUALS;
    std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
        kOtherUrl, kDefaultSiteForCookies, kDefaultOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));

    ASSERT_THAT(cookies, testing::SizeIs(1));

    EXPECT_EQ("new-name", cookies[0].Name());
    EXPECT_EQ("new-value", cookies[0].Value());
  }
}

// TODO(crbug.com/40061885): Add test cases that modify the cookies through
// net::CookieStore::SetCanonicalCookie and/or modify the subscription URL.
TEST_P(RestrictedCookieManagerTest, CookieVersion) {
  std::string cookies_out;
  base::ReadOnlySharedMemoryRegion mapped_region;
  uint64_t version;

  EXPECT_TRUE(backend()->GetCookiesString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/false,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  // Version is at initial value on first query.
  EXPECT_EQ(version, mojom::kInitialCookieVersion);

  EXPECT_TRUE(backend()->GetCookiesString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/false,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  // Version is still at initial value since nothing modified the cookie.
  EXPECT_EQ(version, mojom::kInitialCookieVersion);

  EXPECT_TRUE(backend()->SetCookieFromString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, "new-name=new-value;path=/"));

  EXPECT_TRUE(backend()->GetCookiesString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/false,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  // Version has been incremented by the set operation.
  EXPECT_NE(version, mojom::kInitialCookieVersion);
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
          kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
          net::StorageAccessApiStatus::kNone, std::move(options)),
      UnorderedElementsAre(
          net::MatchesCookieNameValue("cookie-name", "cookie-value"),
          net::MatchesCookieNameValue("cookie-name-2", "cookie-value-2")));

  // Can also use the document.cookie-style API to get the same info.
  std::string cookies_out;
  base::ReadOnlySharedMemoryRegion mapped_region;
  uint64_t version;

  EXPECT_TRUE(backend()->GetCookiesString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/false,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  EXPECT_FALSE(mapped_region.IsValid());
  EXPECT_EQ("cookie-name=cookie-value; cookie-name-2=cookie-value-2",
            cookies_out);

  // One more time but also requesting some shared memory.
  EXPECT_TRUE(backend()->GetCookiesString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  EXPECT_TRUE(mapped_region.IsValid());
  EXPECT_EQ("cookie-name=cookie-value; cookie-name-2=cookie-value-2",
            cookies_out);
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlEmptyFilter) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
                  net::StorageAccessApiStatus::kNone, std::move(options)),
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
          kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
          net::StorageAccessApiStatus::kNone, std::move(options)),
      ElementsAre(net::MatchesCookieNameValue("cookie-name", "cookie-value")));
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlEqualsMatch_WithStorageAccess) {
  service_->OverrideIsolationInfoForTesting(kThirdPartyIsolationInfo);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  cookie_settings_.set_block_third_party_cookies(true);
  cookie_settings_.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {ContentSettingPatternSource(
          ContentSettingsPattern::FromURL(kDefaultUrl),
          ContentSettingsPattern::FromURL(kOtherUrl),
          base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
          content_settings::ProviderType::kNone,
          /*incognito=*/false)});

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  kDefaultUrlWithPath, net::SiteForCookies(), kOtherOrigin,
                  net::StorageAccessApiStatus::kNone, options.Clone()),
              testing::ElementsAreArray<CanonicalCookieMatcher>({}));

  // When `storage_access_api_status` is not kNone, Storage Access grants may be
  // used to access cookies.
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          kDefaultUrlWithPath, net::SiteForCookies(), kOtherOrigin,
          net::StorageAccessApiStatus::kAccessViaAPI, std::move(options)),
      ElementsAre(net::MatchesCookieNameValue("cookie-name", "cookie-value")));
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
          kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
          net::StorageAccessApiStatus::kNone, std::move(options)),
      UnorderedElementsAre(
          net::MatchesCookieNameValue("cookie-name-2", "cookie-value-2"),
          net::MatchesCookieNameValue("cookie-name-2b", "cookie-value-2b")));
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlHttpOnly) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetHttpOnlySessionCookie("cookie-name-http", "cookie-value-2", "example.com",
                           "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, std::move(options));

  if (RestrictedCookieManagerRole() ==
      mojom::RestrictedCookieManagerRole::SCRIPT) {
    EXPECT_THAT(cookies, UnorderedElementsAre(net::MatchesCookieNameValue(
                             "cookie-name", "cookie-value")));
  } else {
    EXPECT_THAT(
        cookies,
        UnorderedElementsAre(
            net::MatchesCookieNameValue("cookie-name", "cookie-value"),
            net::MatchesCookieNameValue("cookie-name-http", "cookie-value-2")));
  }
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlFromWrongOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "notexample.com",
                   "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  ExpectBadMessage();
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  kOtherUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
                  net::StorageAccessApiStatus::kNone, std::move(options)),
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
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
                  net::StorageAccessApiStatus::kNone, std::move(options)),
              IsEmpty());
  EXPECT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, GetCookieStringFromWrongOrigin) {
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  SetSessionCookie("cookie-name-2", "cookie-value-2", "example.com", "/");
  SetSessionCookie("other-cookie-name", "other-cookie-value", "notexample.com",
                   "/");

  ExpectBadMessage();
  std::string cookies_out;
  base::ReadOnlySharedMemoryRegion mapped_region;
  uint64_t version;

  EXPECT_TRUE(backend()->GetCookiesString(
      kOtherUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/false,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  EXPECT_TRUE(received_bad_message());
  EXPECT_THAT(cookies_out, IsEmpty());

  // One more time but also requesting some shared memory.
  EXPECT_TRUE(backend()->GetCookiesString(
      kOtherUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone,
      /*get_version_shared_memory=*/true,
      /*is_ad_tagged=*/false, /*force_disable_third_party_cookies=*/false,
      &version, &mapped_region, &cookies_out));
  EXPECT_TRUE(received_bad_message());
  EXPECT_THAT(cookies_out, IsEmpty());
}

TEST_P(RestrictedCookieManagerTest, GetAllAdTagged) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  sync_service_->GetAllForUrl(kDefaultUrlWithPath, net::SiteForCookies(),
                              kDefaultOrigin,
                              net::StorageAccessApiStatus::kNone,
                              std::move(options), /*is_ad_tagged=*/true);
  WaitForCallback();
  EXPECT_THAT(recorded_activity().back().is_ad_tagged, true);

  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  options = mojom::CookieManagerGetOptions::New();
  options->name = "cookie-name";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  sync_service_->GetAllForUrl(kDefaultUrlWithPath, net::SiteForCookies(),
                              kDefaultOrigin,
                              net::StorageAccessApiStatus::kNone,
                              std::move(options), /*is_ad_tagged=*/false);
  WaitForCallback();

  EXPECT_THAT(recorded_activity().back().is_ad_tagged, false);
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlPolicy) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  // With default policy, should be able to get all cookies, even third-party.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(sync_service_->GetAllForUrl(
                    kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
                    net::StorageAccessApiStatus::kNone, std::move(options)),
                ElementsAre(net::MatchesCookieNameValue("cookie-name",
                                                        "cookie-value")));
  }

  WaitForCallback();

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kRead,
                  "https://example.com/test/", net::SiteForCookies(),
                  CookieOrLine("cookie-name=cookie-value",
                               mojom::CookieOrLine::Tag::kCookie),
                  net::IsInclude())));

  // Disabling getting third-party cookies works correctly.
  cookie_settings_.set_block_third_party_cookies(true);
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(sync_service_->GetAllForUrl(
                    kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
                    net::StorageAccessApiStatus::kNone, std::move(options)),
                testing::ElementsAreArray<CanonicalCookieMatcher>({}));
  }

  WaitForCallback();
  EXPECT_THAT(
      recorded_activity(),
      ElementsAre(
          testing::_,
          MatchesCookieOp(
              mojom::CookieAccessDetails::Type::kRead,
              "https://example.com/test/", net::SiteForCookies(),
              CookieOrLine("cookie-name=cookie-value",
                           mojom::CookieOrLine::Tag::kCookie),
              net::CookieInclusionStatus::MakeFromReasonsForTesting(
                  {net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}))));
}

TEST_P(RestrictedCookieManagerTest, FilteredCookieAccessEvents) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);

  const char* kCookieName = "cookie-name";
  const char* kCookieValue = "cookie-value";
  const char* kCookieSite = "example.com";
  std::string cookie_name_field =
      std::string(kCookieName) + std::string("=") + std::string(kCookieValue);

  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  SetSessionCookie(kCookieName, kCookieValue, kCookieSite, "/");

  // With default policy, should be able to get all cookies, even third-party.
  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = kCookieName;
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
            net::StorageAccessApiStatus::kNone, std::move(options)),
        ElementsAre(net::MatchesCookieNameValue(kCookieName, kCookieValue)));

    WaitForCallback();

    EXPECT_THAT(
        recorded_activity(),
        ElementsAre(MatchesCookieOp(
            mojom::CookieAccessDetails::Type::kRead, kDefaultUrlWithPath,
            net::SiteForCookies(),
            CookieOrLine(cookie_name_field, mojom::CookieOrLine::Tag::kCookie),
            AllOf(
                net::IsInclude(),
                net::HasWarningReason(
                    net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)))));
  }

  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = kCookieName;
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
            net::StorageAccessApiStatus::kNone, std::move(options)),
        ElementsAre(net::MatchesCookieNameValue(kCookieName, kCookieValue)));

    // A second cookie access should not generate a notification.
    EXPECT_THAT(
        recorded_activity(),
        ElementsAre(MatchesCookieOp(
            mojom::CookieAccessDetails::Type::kRead, kDefaultUrlWithPath,
            net::SiteForCookies(),
            CookieOrLine(cookie_name_field, mojom::CookieOrLine::Tag::kCookie),
            AllOf(
                net::IsInclude(),
                net::HasWarningReason(
                    net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)))));
  }

  // Change the cookie with a new value so that a cookie access
  // event is possible, then block the cookie access.
  const char* kNewCookieValue = "new-cookie-value";
  cookie_name_field = std::string(kCookieName) + std::string("=") +
                      std::string(kNewCookieValue);
  SetSessionCookie(kCookieName, kNewCookieValue, kCookieSite, "/");
  cookie_settings_.set_block_third_party_cookies(true);

  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = kCookieName;
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(sync_service_->GetAllForUrl(
                    kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
                    net::StorageAccessApiStatus::kNone, std::move(options)),
                testing::ElementsAreArray<CanonicalCookieMatcher>({}));

    WaitForCallback();

    // A change in access result (allowed -> blocked) should generate a new
    // notification.
    EXPECT_EQ(recorded_activity().size(), 2ul);
  }

  // Allow the cookie access.
  cookie_settings_.set_block_third_party_cookies(false);

  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = kCookieName;
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(
        sync_service_->GetAllForUrl(
            kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
            net::StorageAccessApiStatus::kNone, std::move(options)),
        ElementsAre(net::MatchesCookieNameValue(kCookieName, kNewCookieValue)));

    WaitForCallback();

    // A change in access result (blocked -> allowed) should generate a new
    // notification.
    EXPECT_THAT(
        recorded_activity(),
        Contains(MatchesCookieOp(
            mojom::CookieAccessDetails::Type::kRead, kDefaultUrlWithPath,
            net::SiteForCookies(),
            CookieOrLine(cookie_name_field, mojom::CookieOrLine::Tag::kCookie),
            AllOf(
                net::IsInclude(),
                net::HasWarningReason(
                    net::CookieInclusionStatus::WARN_THIRD_PARTY_PHASEOUT)))));
  }
}

TEST_P(RestrictedCookieManagerTest, GetAllForUrlPolicyWarnActual) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);

  // Activate legacy semantics to be able to inject a bad cookie.
  auto cookie_access_delegate =
      std::make_unique<net::TestCookieAccessDelegate>();
  cookie_access_delegate->SetExpectationForCookieDomain(
      "example.com", net::CookieAccessSemantics::LEGACY);
  cookie_monster_.SetCookieAccessDelegate(std::move(cookie_access_delegate));

  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/",
                   /*secure=*/false);

  // Now test get using (default) nonlegacy semantics.
  cookie_monster_.SetCookieAccessDelegate(nullptr);

  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "cookie-name";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    EXPECT_THAT(sync_service_->GetAllForUrl(
                    kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
                    net::StorageAccessApiStatus::kNone, std::move(options)),
                IsEmpty());
  }

  WaitForCallback();

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kRead,
                  "https://example.com/test/", net::SiteForCookies(),
                  CookieOrLine("cookie-name=cookie-value",
                               mojom::CookieOrLine::Tag::kCookie),
                  net::HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::
                              EXCLUDE_SAMESITE_NONE_INSECURE}))));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookie) {
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
          net::StorageAccessApiStatus::kNone, std::move(options)),
      ElementsAre(net::MatchesCookieNameValue("new-name", "new-value")));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookie_WithStorageAccess) {
  service_->OverrideIsolationInfoForTesting(kThirdPartyIsolationInfo);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  cookie_settings_.set_block_third_party_cookies(true);
  cookie_settings_.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {ContentSettingPatternSource(
          ContentSettingsPattern::FromURL(kDefaultUrl),
          ContentSettingsPattern::FromURL(kOtherUrl),
          base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
          content_settings::ProviderType::kNone,
          /*incognito=*/false)});

  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, net::SiteForCookies(), kOtherOrigin,
      net::StorageAccessApiStatus::kNone));

  // When `storage_access_api_status` is not kNone, the write should succeed.
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, net::SiteForCookies(), kOtherOrigin,
      net::StorageAccessApiStatus::kAccessViaAPI));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieHttpOnly) {
  EXPECT_EQ(RestrictedCookieManagerRole() ==
                mojom::RestrictedCookieManagerRole::NETWORK,
            sync_service_->SetCanonicalCookie(
                *net::CanonicalCookie::CreateUnsafeCookieForTesting(
                    "new-name", "new-value", "example.com", "/", base::Time(),
                    base::Time(), base::Time(), base::Time(), /*secure=*/true,
                    /*httponly=*/true, net::CookieSameSite::NO_RESTRICTION,
                    net::COOKIE_PRIORITY_DEFAULT),
                kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
                net::StorageAccessApiStatus::kNone));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  std::vector<net::CanonicalCookie> cookies = sync_service_->GetAllForUrl(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, std::move(options));

  if (RestrictedCookieManagerRole() ==
      mojom::RestrictedCookieManagerRole::SCRIPT) {
    EXPECT_THAT(cookies, IsEmpty());
  } else {
    EXPECT_THAT(cookies, ElementsAre(net::MatchesCookieNameValue("new-name",
                                                                 "new-value")));
  }
}

TEST_P(RestrictedCookieManagerTest, SetCookieFromString) {
  EXPECT_TRUE(backend()->SetCookieFromString(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, "new-name=new-value;path=/"));
  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "new-name";
  options->match_type = mojom::CookieMatchType::EQUALS;
  EXPECT_THAT(
      sync_service_->GetAllForUrl(
          kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
          net::StorageAccessApiStatus::kNone, std::move(options)),
      ElementsAre(net::MatchesCookieNameValue("new-name", "new-value")));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieFromWrongOrigin) {
  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      kOtherUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone));
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
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieWithMismatchingDomain) {
  ExpectBadMessage();
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "not-example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCookieFromStringWrongOrigin) {
  ExpectBadMessage();
  EXPECT_TRUE(backend()->SetCookieFromString(
      kOtherUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, "new-name=new-value;path=/"));
  ASSERT_TRUE(received_bad_message());
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookiePolicy) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  {
    // With default settings object, setting a third-party cookie is OK.
    auto cookie = net::CanonicalCookie::CreateForTesting(
        kDefaultUrl, "A=B; SameSite=none; Secure", base::Time::Now(),
        std::nullopt /* server_time */,
        std::nullopt /* cookie_partition_key */);
    EXPECT_TRUE(sync_service_->SetCanonicalCookie(
        *cookie, kDefaultUrl, net::SiteForCookies(), kDefaultOrigin,
        net::StorageAccessApiStatus::kNone));
  }

  WaitForCallback();

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kChange,
                  "https://example.com/", net::SiteForCookies(),
                  CookieOrLine("A=B", mojom::CookieOrLine::Tag::kCookie),
                  net::IsInclude())));

  {
    // Not if third-party cookies are disabled, though.
    cookie_settings_.set_block_third_party_cookies(true);
    auto cookie = net::CanonicalCookie::CreateForTesting(
        kDefaultUrl, "A2=B2; SameSite=none; Secure", base::Time::Now(),
        std::nullopt /* server_time */,
        std::nullopt /* cookie_partition_key */);
    EXPECT_FALSE(sync_service_->SetCanonicalCookie(
        *cookie, kDefaultUrl, net::SiteForCookies(), kDefaultOrigin,
        net::StorageAccessApiStatus::kNone));
  }

  WaitForCallback();
  EXPECT_THAT(
      recorded_activity(),
      Contains(MatchesCookieOp(
          mojom::CookieAccessDetails::Type::kChange, "https://example.com/",
          net::SiteForCookies(),
          CookieOrLine("A2=B2", mojom::CookieOrLine::Tag::kCookie),
          net::HasExactlyExclusionReasonsForTesting(
              std::vector<net::CookieInclusionStatus::ExclusionReason>{
                  net::CookieInclusionStatus::EXCLUDE_USER_PREFERENCES}))));

  // Read back, in first-party context
  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "A";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;

  service_->OverrideIsolationInfoForTesting(kDefaultIsolationInfo);
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
                  net::StorageAccessApiStatus::kNone, std::move(options)),
              Contains(net::MatchesCookieNameValue("A", "B")));

  WaitForCallback();

  EXPECT_THAT(
      recorded_activity(),
      Contains(MatchesCookieOp(
          mojom::CookieAccessDetails::Type::kRead, "https://example.com/test/",
          net::SiteForCookies::FromUrl(GURL("https://example.com/")),
          CookieOrLine("A=B", mojom::CookieOrLine::Tag::kCookie),
          net::IsInclude())));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookiePolicyWarnActual) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);

  auto cookie = net::CanonicalCookie::CreateForTesting(
      kDefaultUrl, "A=B", base::Time::Now(), std::nullopt /* server_time */,
      std::nullopt /* cookie_partition_key */);
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *cookie, kDefaultUrl, net::SiteForCookies(), kDefaultOrigin,
      net::StorageAccessApiStatus::kNone));

  WaitForCallback();

  EXPECT_THAT(recorded_activity(),
              ElementsAre(MatchesCookieOp(
                  mojom::CookieAccessDetails::Type::kChange,
                  "https://example.com/", net::SiteForCookies(),
                  CookieOrLine("A=B", mojom::CookieOrLine::Tag::kCookie),
                  net::HasExactlyExclusionReasonsForTesting(
                      std::vector<net::CookieInclusionStatus::ExclusionReason>{
                          net::CookieInclusionStatus::
                              EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX}))));
}

TEST_P(RestrictedCookieManagerTest, SetCanonicalCookieWithInclusionStatus) {
  ExpectBadMessage();
  net::CookieInclusionStatus status_exclude(
      net::CookieInclusionStatus::ExclusionReason::EXCLUDE_USER_PREFERENCES);
  // In this instance cookie should be OK but due to the status having
  // an exclusion reason, the result should be false and a BadMessage should
  // be received.
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, status_exclude));
  ASSERT_TRUE(received_bad_message());

  // In this instance the cookie should be OK and the status only
  // has a warning so the result should be true.
  net::CookieInclusionStatus status_warning(
      net::CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE);
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "new-name", "new-value", "example.com", "/", base::Time(),
          base::Time(), base::Time(), base::Time(), /*secure=*/true,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, status_warning));

  WaitForCallback();
  EXPECT_THAT(
      recorded_activity(),
      ElementsAre(MatchesCookieOp(
          mojom::CookieAccessDetails::Type::kChange, kDefaultUrlWithPath,
          kDefaultSiteForCookies,
          CookieOrLine("new-name=new-value", mojom::CookieOrLine::Tag::kCookie),
          net::CookieInclusionStatus::MakeFromReasonsForTesting(
              {}, {net::CookieInclusionStatus::
                       WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE}))));
}

TEST_P(RestrictedCookieManagerTest, CookiesEnabledFor) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  // Default, third-party access is OK.
  bool result = false;
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      kDefaultUrl, net::SiteForCookies(), kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, &result));
  EXPECT_TRUE(result);

  // Third-party cookies disabled.
  cookie_settings_.set_block_third_party_cookies(true);
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      kDefaultUrl, net::SiteForCookies(), kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, &result));
  EXPECT_FALSE(result);

  // First-party ones still OK.
  service_->OverrideIsolationInfoForTesting(kDefaultIsolationInfo);
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      kDefaultUrl, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, &result));
  EXPECT_TRUE(result);
}

TEST_P(RestrictedCookieManagerTest, CookiesEnabledFor_WithStorageAccess) {
  service_->OverrideIsolationInfoForTesting(kThirdPartyIsolationInfo);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  cookie_settings_.set_block_third_party_cookies(true);
  cookie_settings_.set_content_settings(
      ContentSettingsType::STORAGE_ACCESS,
      {ContentSettingPatternSource(
          ContentSettingsPattern::FromURL(kDefaultUrl),
          ContentSettingsPattern::FromURL(kOtherUrl),
          base::Value(ContentSetting::CONTENT_SETTING_ALLOW),
          content_settings::ProviderType::kNone,
          /*incognito=*/false)});

  bool result;
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      kDefaultUrl, net::SiteForCookies(), kOtherOrigin,
      net::StorageAccessApiStatus::kNone, &result));
  EXPECT_FALSE(result);

  // When `storage_access_api_status` is not kNone, access is allowed since
  // there's a matching permission grant.
  EXPECT_TRUE(backend()->CookiesEnabledFor(
      kDefaultUrl, net::SiteForCookies(), kOtherOrigin,
      net::StorageAccessApiStatus::kAccessViaAPI, &result));
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
  auto chrome_origin = url::Origin::Create(chrome_url);
  net::SiteForCookies chrome_site_for_cookies =
      net::SiteForCookies::FromUrl(chrome_url);

  // Test if site_for_cookies is chrome, then SameSite cookies can be
  // set and gotten if the origin is secure.
  service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, chrome_origin, https_origin,
      chrome_site_for_cookies));
  service_->OverrideOriginForTesting(https_origin);
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "strict-cookie", "1", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      https_url, chrome_site_for_cookies, chrome_origin,
      net::StorageAccessApiStatus::kNone));
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "lax-cookie", "1", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      https_url, chrome_site_for_cookies, chrome_origin,
      net::StorageAccessApiStatus::kNone));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  https_url, chrome_site_for_cookies, chrome_origin,
                  net::StorageAccessApiStatus::kNone, std::move(options)),
              testing::SizeIs(2));

  // Test if site_for_cookies is chrome, then SameSite cookies cannot be
  // set and gotten if the origin is not secure.
  service_->OverrideIsolationInfoForTesting(net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, chrome_origin, http_origin,
      chrome_site_for_cookies));
  service_->OverrideOriginForTesting(http_origin);
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "strict-cookie", "2", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::STRICT_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      http_url, chrome_site_for_cookies, chrome_origin,
      net::StorageAccessApiStatus::kNone));
  EXPECT_FALSE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateUnsafeCookieForTesting(
          "lax-cookie", "2", "example.com", "/", base::Time(), base::Time(),
          base::Time(), base::Time(), /*secure=*/false,
          /*httponly=*/false, net::CookieSameSite::LAX_MODE,
          net::COOKIE_PRIORITY_DEFAULT),
      http_url, chrome_site_for_cookies, chrome_origin,
      net::StorageAccessApiStatus::kNone));

  options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;
  EXPECT_THAT(sync_service_->GetAllForUrl(
                  http_url, chrome_site_for_cookies, chrome_origin,
                  net::StorageAccessApiStatus::kNone, std::move(options)),
              IsEmpty());
}

TEST_P(RestrictedCookieManagerTest, ChangeDispatch) {
  auto listener = CreateCookieChangeListener(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone);

  ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));

  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");
  listener->WaitForChange();

  ASSERT_THAT(listener->observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(net::CookieChangeCause::INSERTED,
            listener->observed_changes()[0].cause);
  EXPECT_THAT(listener->observed_changes()[0].cookie,
              net::MatchesCookieNameValue("cookie-name", "cookie-value"));
}

TEST_P(RestrictedCookieManagerTest, ChangeSettings) {
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  auto listener = CreateCookieChangeListener(
      kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
      net::StorageAccessApiStatus::kNone);

  EXPECT_THAT(listener->observed_changes(), IsEmpty());

  cookie_settings_.set_block_third_party_cookies(true);
  SetSessionCookie("cookie-name", "cookie-value", "example.com", "/");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(listener->observed_changes(), IsEmpty());
}

TEST_P(RestrictedCookieManagerTest, AddChangeListenerFromWrongOrigin) {
  mojo::PendingRemote<network::mojom::CookieChangeListener> bad_listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> bad_receiver =
      bad_listener_remote.InitWithNewPipeAndPassReceiver();
  ExpectBadMessage();
  sync_service_->AddChangeListener(
      kOtherUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, std::move(bad_listener_remote));
  EXPECT_TRUE(received_bad_message());
  TestCookieChangeListener bad_listener(std::move(bad_receiver));

  mojo::PendingRemote<network::mojom::CookieChangeListener>
      good_listener_remote;
  mojo::PendingReceiver<network::mojom::CookieChangeListener> good_receiver =
      good_listener_remote.InitWithNewPipeAndPassReceiver();
  sync_service_->AddChangeListener(
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, std::move(good_listener_remote));
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
              net::MatchesCookieNameValue("cookie-name", "cookie-value"));
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
      kDefaultUrlWithPath, kDefaultSiteForCookies, kDefaultOrigin,
      net::StorageAccessApiStatus::kNone, std::move(bad_listener_remote));
  EXPECT_TRUE(received_bad_message());

  TestCookieChangeListener bad_listener(std::move(bad_receiver));
  ASSERT_THAT(bad_listener.observed_changes(), IsEmpty());
}

// Test that the Change listener receives the access semantics, and that they
// are taken into account when deciding when to dispatch the change.
TEST_P(RestrictedCookieManagerTest, ChangeNotificationIncludesAccessSemantics) {
  auto cookie_access_delegate =
      std::make_unique<net::TestCookieAccessDelegate>();
  cookie_access_delegate->SetExpectationForCookieDomain(
      "example.com", net::CookieAccessSemantics::LEGACY);
  cookie_monster_.SetCookieAccessDelegate(std::move(cookie_access_delegate));

  // Use a cross-site site_for_cookies.
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  auto listener = CreateCookieChangeListener(
      kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
      net::StorageAccessApiStatus::kNone);

  ASSERT_THAT(listener->observed_changes(), IsEmpty());

  auto cookie = net::CanonicalCookie::CreateForTesting(
      kDefaultUrl, "cookie_with_no_samesite=unspecified", base::Time::Now(),
      std::nullopt, std::nullopt /* cookie_partition_key */);

  // Set cookie directly into the CookieMonster, using all-inclusive options.
  net::ResultSavingCookieCallback<net::CookieAccessResult> callback;
  cookie_monster_.SetCanonicalCookieAsync(
      std::move(cookie), kDefaultUrl, net::CookieOptions::MakeAllInclusive(),
      callback.MakeCallback());
  callback.WaitUntilDone();
  ASSERT_TRUE(callback.result().status.IsInclude());

  // The listener only receives the change because the cookie is legacy.
  listener->WaitForChange();

  ASSERT_THAT(listener->observed_changes(), testing::SizeIs(1));
  EXPECT_EQ(net::CookieAccessSemantics::LEGACY,
            listener->observed_changes()[0].access_result.access_semantics);
}

TEST_P(RestrictedCookieManagerTest, NoChangeNotificationForNonlegacyCookie) {
  auto cookie_access_delegate =
      std::make_unique<net::TestCookieAccessDelegate>();
  cookie_access_delegate->SetExpectationForCookieDomain(
      "example.com", net::CookieAccessSemantics::NONLEGACY);
  cookie_monster_.SetCookieAccessDelegate(std::move(cookie_access_delegate));

  // Use a cross-site site_for_cookies.
  service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);
  auto listener = CreateCookieChangeListener(
      kDefaultUrlWithPath, net::SiteForCookies(), kDefaultOrigin,
      net::StorageAccessApiStatus::kNone);

  ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));

  auto unspecified_cookie = net::CanonicalCookie::CreateForTesting(
      kDefaultUrl, "cookie_with_no_samesite=unspecified", base::Time::Now(),
      std::nullopt, std::nullopt /* cookie_partition_key */);

  auto samesite_none_cookie = net::CanonicalCookie::CreateForTesting(
      kDefaultUrl, "samesite_none_cookie=none; SameSite=None; Secure",
      base::Time::Now(), std::nullopt, std::nullopt /* cookie_partition_key */);

  // Set cookies directly into the CookieMonster, using all-inclusive options.
  net::ResultSavingCookieCallback<net::CookieAccessResult> callback1;
  cookie_monster_.SetCanonicalCookieAsync(
      std::move(unspecified_cookie), kDefaultUrl,
      net::CookieOptions::MakeAllInclusive(), callback1.MakeCallback());
  callback1.WaitUntilDone();
  ASSERT_TRUE(callback1.result().status.IsInclude());

  // Listener doesn't receive notification because cookie is not included for
  // request URL for being unspecified and treated as lax.
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));

  net::ResultSavingCookieCallback<net::CookieAccessResult> callback2;
  cookie_monster_.SetCanonicalCookieAsync(
      std::move(samesite_none_cookie), kDefaultUrl,
      net::CookieOptions::MakeAllInclusive(), callback2.MakeCallback());
  callback2.WaitUntilDone();
  ASSERT_TRUE(callback2.result().status.IsInclude());

  // Listener only receives notification about the SameSite=None cookie.
  listener->WaitForChange();
  ASSERT_THAT(listener->observed_changes(), testing::SizeIs(1));

  EXPECT_EQ("samesite_none_cookie",
            listener->observed_changes()[0].cookie.Name());
  EXPECT_EQ(net::CookieAccessSemantics::NONLEGACY,
            listener->observed_changes()[0].access_result.access_semantics);
}

// Test Partitioned cookie behavior when feature is enabled.
TEST_P(RestrictedCookieManagerTest, PartitionedCookies) {
  // TODO crbug.com/328043119 remove code associated with
  // kAncestorChainBitEnabledInPartitionedCookies
  // after it's enabled by default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kAncestorChainBitEnabledInPartitionedCookies);

  const GURL kCookieURL("https://example.com");
  const GURL kTopFrameURL("https://sub.foo.com");
  const net::SiteForCookies kSiteForCookies =
      net::SiteForCookies::FromUrl(kTopFrameURL);
  const url::Origin kTopFrameOrigin = url::Origin::Create(kTopFrameURL);
  const net::IsolationInfo kIsolationInfo =
      net::IsolationInfo::CreateForInternalRequest(kTopFrameOrigin);

  service_->OverrideIsolationInfoForTesting(kIsolationInfo);
  sync_service_->SetCookieFromString(
      kCookieURL, kSiteForCookies, kTopFrameOrigin,
      net::StorageAccessApiStatus::kNone,
      "__Host-foo=bar; Secure; SameSite=None; Path=/; Partitioned");

  {  // Test request from the same top-level site.
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    net::CookieList cookies = sync_service_->GetAllForUrl(
        kCookieURL, kSiteForCookies, kTopFrameOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));
    ASSERT_EQ(1u, cookies.size());
    EXPECT_TRUE(cookies[0].IsPartitioned());
    EXPECT_EQ(
        net::CookiePartitionKey::FromURLForTesting(GURL("https://foo.com")),
        cookies[0].PartitionKey());
    EXPECT_EQ("__Host-foo", cookies[0].Name());
    EXPECT_EQ(net::CookieSourceType::kScript, cookies[0].SourceType());

    auto listener =
        CreateCookieChangeListener(kCookieURL, kSiteForCookies, kTopFrameOrigin,
                                   net::StorageAccessApiStatus::kNone);

    // Update partitioned cookie Max-Age: None -> 7200.
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-foo=bar; Secure; SameSite=None; Path=/; Partitioned; "
            "Max-Age=7200",
            base::Time::Now(), std::nullopt /* server_time */,
            net::CookiePartitionKey::FromNetworkIsolationKey(
                kIsolationInfo.network_isolation_key(),
                kIsolationInfo.site_for_cookies(),
                net::SchemefulSite(kCookieURL),
                kIsolationInfo.IsMainFrameRequest())),
        "https", false /* can_modify_httponly */));

    // If Partitioned cookies are enabled, the change listener should see the
    // change to the cookie on this top-level site.
    listener->WaitForChange();
    ASSERT_THAT(listener->observed_changes(), testing::SizeIs(1));
  }

  {  // Test request from another top-level site.
    const GURL kOtherTopFrameURL("https://foo.bar.com");
    const net::SiteForCookies kOtherSiteForCookies =
        net::SiteForCookies::FromUrl(kOtherTopFrameURL);
    const url::Origin kOtherTopFrameOrigin =
        url::Origin::Create(kOtherTopFrameURL);
    const net::IsolationInfo kOtherIsolationInfo =
        net::IsolationInfo::CreateForInternalRequest(kOtherTopFrameOrigin);

    service_->OverrideIsolationInfoForTesting(kOtherIsolationInfo);

    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    net::CookieList cookies = sync_service_->GetAllForUrl(
        kCookieURL, kOtherSiteForCookies, kOtherTopFrameOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));
    ASSERT_EQ(0u, cookies.size());

    auto listener = CreateCookieChangeListener(
        kCookieURL, kOtherSiteForCookies, kOtherTopFrameOrigin,
        net::StorageAccessApiStatus::kNone);

    // Set a new listener with the original IsolationInfo, we wait for this
    // listener to receive an event to verify that the second listener was
    // either called or skipped.
    service_->OverrideIsolationInfoForTesting(kIsolationInfo);
    auto second_listener =
        CreateCookieChangeListener(kCookieURL, kSiteForCookies, kTopFrameOrigin,
                                   net::StorageAccessApiStatus::kNone);

    // Update partitioned cookie Max-Age: 7200 -> 3600.
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-foo=bar; Secure; SameSite=None; Path=/; Partitioned; "
            "Max-Age=3600",
            base::Time::Now(), std::nullopt /* server_time */,
            net::CookiePartitionKey::FromNetworkIsolationKey(
                kIsolationInfo.network_isolation_key(),
                kIsolationInfo.site_for_cookies(),
                net::SchemefulSite(kCookieURL),
                kIsolationInfo.IsMainFrameRequest())),
        "https", false /* can_modify_httponly */));

    // If Partitioned cookies are enabled, the listener should not see cookie
    // change events on this top-level site.
    second_listener->WaitForChange();
    ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));
  }

  {  // Test that a cookie cannot be set with a different partition key than
     // RestrictedCookieManager's.
    service_->OverrideIsolationInfoForTesting(kIsolationInfo);
    ExpectBadMessage();
    EXPECT_FALSE(sync_service_->SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-foo=bar; Secure; SameSite=None; Path=/; Partitioned",
            base::Time::Now(), std::nullopt /* server_time */,
            net::CookiePartitionKey::FromURLForTesting(
                GURL("https://foo.bar.com"))),
        kCookieURL, kSiteForCookies, kTopFrameOrigin,
        net::StorageAccessApiStatus::kNone));
  }
}

TEST_P(RestrictedCookieManagerTest, PartitionKeyFromScript) {
  const GURL kCookieURL("https://example.com");
  const GURL kTopFrameURL("https://foo.com");
  const net::SiteForCookies kSiteForCookies =
      net::SiteForCookies::FromUrl(kTopFrameURL);
  const url::Origin kTopFrameOrigin = url::Origin::Create(kTopFrameURL);
  const net::IsolationInfo kIsolationInfo =
      net::IsolationInfo::CreateForInternalRequest(kTopFrameOrigin);

  service_->OverrideIsolationInfoForTesting(kIsolationInfo);
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateForTesting(
          kCookieURL,
          "__Host-foo=bar; Secure; SameSite=None; Path=/; Partitioned",
          base::Time::Now(), std::nullopt /* server_time */,
          net::CookiePartitionKey::FromScript()),
      kCookieURL, kSiteForCookies, kTopFrameOrigin,
      net::StorageAccessApiStatus::kNone));

  auto options = mojom::CookieManagerGetOptions::New();
  options->name = "";
  options->match_type = mojom::CookieMatchType::STARTS_WITH;

  net::CookieList cookies = sync_service_->GetAllForUrl(
      kCookieURL, kSiteForCookies, kTopFrameOrigin,
      net::StorageAccessApiStatus::kNone, std::move(options));
  ASSERT_EQ(1u, cookies.size());
  EXPECT_TRUE(cookies[0].IsPartitioned());
  EXPECT_EQ(cookies[0].PartitionKey().value(),
            net::CookiePartitionKey::FromURLForTesting(kTopFrameURL));
  EXPECT_EQ("__Host-foo", cookies[0].Name());
}

TEST_P(RestrictedCookieManagerTest, PartitionKeyWithNonce) {
  // TODO crbug.com/328043119 remove code associated with
  // kAncestorChainBitEnabledInPartitionedCookies
  // after it's enabled by default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kAncestorChainBitEnabledInPartitionedCookies);

  const GURL kCookieURL("https://example.com");
  const GURL kTopFrameURL("https://foo.com");
  const net::SiteForCookies kSiteForCookies =
      net::SiteForCookies::FromUrl(kTopFrameURL);
  const url::Origin kTopFrameOrigin = url::Origin::Create(kTopFrameURL);
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();
  const net::IsolationInfo kNoncedIsolationInfo = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, kTopFrameOrigin,
      kTopFrameOrigin, kSiteForCookies, kNonce);

  const std::optional<net::CookiePartitionKey> kNoncedPartitionKey =
      net::CookiePartitionKey::FromNetworkIsolationKey(
          net::NetworkIsolationKey(net::SchemefulSite(kTopFrameURL),
                                   net::SchemefulSite(kTopFrameURL), kNonce),
          kNoncedIsolationInfo.site_for_cookies(),
          net::SchemefulSite(kTopFrameURL),
          kNoncedIsolationInfo.IsMainFrameRequest());

  const net::IsolationInfo kUnnoncedIsolationInfo =
      net::IsolationInfo::CreateForInternalRequest(kTopFrameOrigin);

  service_->OverrideIsolationInfoForTesting(kNoncedIsolationInfo);
  EXPECT_TRUE(sync_service_->SetCanonicalCookie(
      *net::CanonicalCookie::CreateForTesting(
          kCookieURL, "__Host-foo=bar; Secure; SameSite=None; Path=/;",
          base::Time::Now(), std::nullopt /* server_time */,
          net::CookiePartitionKey::FromScript()),
      kCookieURL, kSiteForCookies, kTopFrameOrigin,
      net::StorageAccessApiStatus::kNone));

  {
    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    net::CookieList cookies = sync_service_->GetAllForUrl(
        kCookieURL, kSiteForCookies, kTopFrameOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));
    ASSERT_EQ(1u, cookies.size());
    EXPECT_TRUE(cookies[0].IsPartitioned());
    EXPECT_EQ(cookies[0].PartitionKey().value(), kNoncedPartitionKey);
    EXPECT_EQ("__Host-foo", cookies[0].Name());
  }

  {  // Test that an unnonced partition cannot see the cookies or observe
     // changes to them.
    service_->OverrideIsolationInfoForTesting(kUnnoncedIsolationInfo);

    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    net::CookieList cookies = sync_service_->GetAllForUrl(
        kCookieURL, kSiteForCookies, kTopFrameOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));
    ASSERT_EQ(0u, cookies.size());

    auto listener =
        CreateCookieChangeListener(kCookieURL, kSiteForCookies, kTopFrameOrigin,
                                   net::StorageAccessApiStatus::kNone);

    service_->OverrideIsolationInfoForTesting(kNoncedIsolationInfo);
    auto second_listener =
        CreateCookieChangeListener(kCookieURL, kSiteForCookies, kTopFrameOrigin,
                                   net::StorageAccessApiStatus::kNone);

    // Update partitioned cookie Max-Age: None -> 7200.
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-foo=bar; Secure; SameSite=None; Path=/; Max-Age=7200",
            base::Time::Now(), std::nullopt /* server_time */,
            kNoncedPartitionKey),
        "https", false /* can_modify_httponly */));

    second_listener->WaitForChange();
    ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));
  }

  {  // Test that the nonced partition cannot observe changes to the unnonced
     // partition and unpartitioned cookies.
    // Set an unpartitioned cookie.
    service_->OverrideIsolationInfoForTesting(kUnnoncedIsolationInfo);
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-unpartitioned=123; Secure; SameSite=None; Path=/;",
            base::Time::Now()),
        "https", false /* can_modify_httponly */));
    // Set a partitioned cookie in the unnonced partition.
    EXPECT_TRUE(sync_service_->SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-bar=baz; Secure; SameSite=None; Path=/; Partitioned;",
            base::Time::Now(), std::nullopt /* server_time */,
            net::CookiePartitionKey::FromScript()),
        kCookieURL, kSiteForCookies, kTopFrameOrigin,
        net::StorageAccessApiStatus::kNone));

    service_->OverrideIsolationInfoForTesting(kNoncedIsolationInfo);

    auto options = mojom::CookieManagerGetOptions::New();
    options->name = "";
    options->match_type = mojom::CookieMatchType::STARTS_WITH;

    // Should only be able to see the nonced partitioned cookie only.
    net::CookieList cookies = sync_service_->GetAllForUrl(
        kCookieURL, kSiteForCookies, kTopFrameOrigin,
        net::StorageAccessApiStatus::kNone, std::move(options));
    ASSERT_EQ(1u, cookies.size());
    ASSERT_EQ("__Host-foo", cookies[0].Name());

    // Create a listener in the nonced partition.
    auto listener =
        CreateCookieChangeListener(kCookieURL, kSiteForCookies, kTopFrameOrigin,
                                   net::StorageAccessApiStatus::kNone);

    // Create a second listener in an unnonced partition.
    service_->OverrideIsolationInfoForTesting(kUnnoncedIsolationInfo);
    auto second_listener =
        CreateCookieChangeListener(kCookieURL, kSiteForCookies, kTopFrameOrigin,
                                   net::StorageAccessApiStatus::kNone);

    // Update unpartitioned cookie Max-Age: None -> 7200.
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-unpartitioned=123; Secure; SameSite=None; Path=/; "
            "Max-Age=7200",
            base::Time::Now()),
        "https", false /* can_modify_httponly */));
    // Test that the nonced partition cannot observe the change.
    second_listener->WaitForChange();
    ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));

    // Update unnonced partitioned cookie Max-Age: None -> 7200.
    EXPECT_TRUE(SetCanonicalCookie(
        *net::CanonicalCookie::CreateForTesting(
            kCookieURL,
            "__Host-bar=baz; Secure; SameSite=None; Path=/; Partitioned; "
            "Max-Age=7200",
            base::Time::Now()),
        "https", false /* can_modify_httponly */));
    // Test that the nonced partition cannot observe the change.
    second_listener->WaitForChange();
    ASSERT_THAT(listener->observed_changes(), testing::SizeIs(0));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RestrictedCookieManagerTest,
    testing::Values(mojom::RestrictedCookieManagerRole::SCRIPT,
                    mojom::RestrictedCookieManagerRole::NETWORK));

}  // namespace network
