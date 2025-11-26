// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_bounce_detector.h"

#include <string_view>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/content_settings/core/common/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/common/content_features.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::Bucket;
using base::PassKey;
using testing::AllOf;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::IsEmpty;
using testing::Pair;
using testing::SizeIs;

namespace content {

// Encodes data about a bounce (the url, time of bounce, and
// whether it's stateful) for use when testing that the bounce is
// recorded by the BtmBounceDetector.
using BounceTuple = std::tuple<GURL, base::Time, bool>;
// Encodes data about an event recorded by a BTM event (the url, time of
// bounce, and type of event) for use when testing that the event is recorded
// by the BtmBounceDetector.
using EventTuple = std::tuple<GURL, base::Time, BtmRecordedEvent>;

enum class UserGestureStatus { kNoUserGesture, kWithUserGesture };
constexpr auto kNoUserGesture = UserGestureStatus::kNoUserGesture;
constexpr auto kWithUserGesture = UserGestureStatus::kWithUserGesture;

// Returns a simplified URL representation for ease of comparison in tests.
// Just host+path.
std::string FormatURL(const GURL& url) {
  return base::StrCat({url.host(), url.path()});
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const BtmRedirectInfo& redirect,
                    const BtmRedirectChainInfo& chain) {
  redirects->push_back(base::StringPrintf(
      "[%zu/%zu] %s -> %s (%s) -> %s", redirect.chain_index.value() + 1,
      chain.length, FormatURL(chain.initial_url).c_str(),
      FormatURL(redirect.redirector_url).c_str(),
      BtmDataAccessTypeToString(redirect.access_type).data(),
      FormatURL(chain.final_url).c_str()));
}

std::string URLForRedirectSourceId(ukm::TestUkmRecorder* ukm_recorder,
                                   ukm::SourceId source_id) {
  return FormatURL(ukm_recorder->GetSourceForSourceId(source_id)->url());
}

class FakeNavigation;

class TestBounceDetectorDelegate : public BtmBounceDetectorDelegate {
 public:
  // BtmBounceDetectorDelegate overrides:
  GURL GetLastCommittedURL() const override { return committed_url_; }
  ukm::SourceId GetLastCommittedSourceId() const override { return source_id_; }

  void HandleRedirectChain(std::vector<BtmRedirectInfoPtr> redirects,
                           BtmRedirectChainInfoPtr chain) override {
    chain->cookie_mode = BtmCookieMode::kBlock3PC;
    size_t redirect_index = chain->length - redirects.size();

    for (auto& redirect : redirects) {
      redirect->site_had_user_activation =
          GetSiteHasUserActivation(redirect->redirector_url);
      redirect->site_had_webauthn_assertion =
          GetSiteHasWebAuthnAssertion(redirect->redirector_url);
      redirect->chain_id = chain->chain_id;
      redirect->chain_index = redirect_index;
      redirect->has_3pc_exception = false;
      DCHECK(redirect->access_type != BtmDataAccessType::kUnknown);
      AppendRedirect(&redirects_, *redirect, *chain);

      BtmServiceImpl::RecordRedirectMetricsForTesting(*redirect, *chain);
      RecordBounce(*redirect, *chain);

      redirect_index++;
    }
  }

  // The version of this method in the BtmWebContentsObserver checks
  // BtmStorage for interactions and runs |callback| with the returned list of
  // sites without interaction. However, for the purpose of testing here, this
  // method just records the sites reported to it in |reported_sites_| without
  // filtering.
  void ReportRedirectors(const std::set<std::string> sites) override {
    if (sites.size() == 0) {
      return;
    }

    reported_sites_.push_back(base::JoinString(
        std::vector<std::string_view>(sites.begin(), sites.end()), ", "));
  }

  bool Are3PcsGenerallyEnabled() const override { return false; }

  // Get the (committed) URL that the SourceId was generated for.
  const std::string& URLForSourceId(ukm::SourceId source_id) {
    return url_by_source_id_[source_id];
  }

  bool GetSiteHasUserActivation(const GURL& url) {
    return sites_with_user_activation_.contains(GetSiteForBtm(url));
  }

  void SetSiteHasUserActivation(const GURL& url) {
    sites_with_user_activation_.insert(GetSiteForBtm(url));
  }

  bool GetSiteHasWebAuthnAssertion(const GURL& url) {
    return sites_with_webauthn_assertion_.contains(GetSiteForBtm(url));
  }

  void SetSiteHasWebAuthnAssertion(const GURL& url) {
    sites_with_webauthn_assertion_.insert(GetSiteForBtm(url));
  }

  void SetCommittedURL(PassKey<FakeNavigation>,
                       const GURL& url,
                       ukm::SourceId source_id) {
    committed_url_ = url;
    source_id_ = source_id;
    url_by_source_id_[source_id_] = FormatURL(url);
  }

  const std::set<BounceTuple>& GetRecordedBounces() const {
    return recorded_bounces_;
  }

  const std::vector<std::string>& GetReportedSites() const {
    return reported_sites_;
  }

  const std::vector<std::string>& redirects() const { return redirects_; }

  int stateful_bounce_count() const { return stateful_bounce_count_; }

 private:
  void RecordBounce(const BtmRedirectInfo& redirect,
                    const BtmRedirectChainInfo& chain) {
    bool stateful = redirect.access_type > BtmDataAccessType::kRead;

    recorded_bounces_.insert(
        std::make_tuple(redirect.redirector_url, redirect.time, stateful));
    if (stateful) {
      stateful_bounce_count_++;
    }
  }

  GURL committed_url_;
  ukm::SourceId source_id_;
  std::map<ukm::SourceId, std::string> url_by_source_id_;
  std::set<std::string> sites_with_user_activation_;
  std::set<std::string> sites_with_webauthn_assertion_;
  std::vector<std::string> redirects_;
  std::set<BounceTuple> recorded_bounces_;
  std::vector<std::string> reported_sites_;
  int stateful_bounce_count_ = 0;
};

class FakeNavigation : public BtmNavigationHandle {
 public:
  FakeNavigation(BtmBounceDetector* detector,
                 TestBounceDetectorDelegate* parent,
                 const GURL& url,
                 bool has_user_gesture)
      : detector_(detector),
        delegate_(parent),
        has_user_gesture_(has_user_gesture) {
    chain_.push_back(url);
    detector_->DidStartNavigation(this);
  }
  ~FakeNavigation() override { CHECK(finished_); }

  FakeNavigation& RedirectTo(std::string url) {
    chain_.emplace_back(std::move(url));
    detector_->DidRedirectNavigation(this);
    return *this;
  }

  FakeNavigation& AccessCookie(CookieOperation op) {
    detector_->OnServerCookiesAccessed(this, GetURL(), op);
    return *this;
  }

  void Finish(bool commit) {
    CHECK(!finished_);
    finished_ = true;
    has_committed_ = commit;
    if (commit) {
      previous_url_ = delegate_->GetLastCommittedURL();
      delegate_->SetCommittedURL(PassKey<FakeNavigation>(), GetURL(),
                                 GetNextPageUkmSourceId());
    }
    detector_->DidFinishNavigation(this);
  }

 private:
  // BtmNavigationHandle overrides:
  bool HasUserGesture() const override { return has_user_gesture_; }
  ServerBounceDetectionState* GetServerState() override { return &state_; }
  bool HasCommitted() const override { return has_committed_; }
  ukm::SourceId GetNextPageUkmSourceId() override { return next_source_id_; }
  const GURL& GetPreviousPrimaryMainFrameURL() const override {
    return previous_url_;
  }
  // TODO (crbug.com/1442658): Add support for simulating opening a link in a
  // new tab.
  const GURL GetInitiator() const override {
    return previous_url_.is_empty() ? GURL("about:blank") : previous_url_;
  }
  const std::vector<GURL>& GetRedirectChain() const override { return chain_; }
  bool WasResponseCached() override { return false; }
  int GetHTTPResponseCode() override { return net::HTTP_FOUND; }

  raw_ptr<BtmBounceDetector> detector_;
  raw_ptr<TestBounceDetectorDelegate> delegate_;
  const bool has_user_gesture_;
  bool finished_ = false;
  const ukm::SourceId next_source_id_ = ukm::AssignNewSourceId();

  ServerBounceDetectionState state_;
  bool has_committed_ = false;
  GURL previous_url_;
  std::vector<GURL> chain_;
};

class BtmBounceDetectorTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FakeNavigation StartNavigation(const std::string& url,
                                 UserGestureStatus status) {
    return FakeNavigation(&detector_, &delegate_, GURL(url),
                          status == kWithUserGesture);
  }

  void NavigateTo(const std::string& url, UserGestureStatus status) {
    StartNavigation(url, status).Finish(true);
  }

  void AccessClientCookie(CookieOperation op) {
    detector_.OnClientSiteDataAccessed(delegate_.GetLastCommittedURL(), op);
  }

  void LateAccessClientCookie(const std::string& url, CookieOperation op) {
    if (!detector_.AddLateCookieAccess(GURL(url), op)) {
      detector_.OnClientSiteDataAccessed(GURL(url), op);
    }
  }

  void ActivatePage() { detector_.OnUserActivation(); }
  void TriggerWebAuthnAssertionRequestSucceeded() {
    detector_.WebAuthnAssertionRequestSucceeded();
  }

  const BtmRedirectContext& CommittedRedirectContext() {
    return detector_.CommittedRedirectContext();
  }

  void AdvanceBtmTime(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
    task_environment_.RunUntilIdle();
  }

  // Advances the mocked clock by `features::kBtmClientBounceDetectionTimeout`
  // to trigger the closure of the pending redirect chain.
  void EndPendingRedirectChain() {
    AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get());
  }

  const std::string& URLForNavigationSourceId(ukm::SourceId source_id) {
    return delegate_.URLForSourceId(source_id);
  }

  void SetSiteHasUserActivation(const std::string& url) {
    return delegate_.SetSiteHasUserActivation(GURL(url));
  }

  std::set<BounceTuple> GetRecordedBounces() const {
    return delegate_.GetRecordedBounces();
  }

  BounceTuple MakeBounceTuple(const std::string& url,
                              const base::Time& time,
                              bool stateful) {
    return std::make_tuple(GURL(url), time, stateful);
  }

  EventTuple MakeEventTuple(const std::string& url,
                            const base::Time& time,
                            BtmRecordedEvent event) {
    return std::make_tuple(GURL(url), time, event);
  }

  const std::vector<std::string>& GetReportedSites() const {
    return delegate_.GetReportedSites();
  }

  base::Time GetCurrentTime() {
    return task_environment_.GetMockClock()->Now();
  }

  const std::vector<std::string>& redirects() const {
    return delegate_.redirects();
  }

  int stateful_bounce_count() const {
    return delegate_.stateful_bounce_count();
  }

 private:
  TestBounceDetectorDelegate delegate_;
  BtmBounceDetector detector_{&delegate_, task_environment_.GetMockTickClock(),
                              task_environment_.GetMockClock()};
};

// Ensures that for every navigation, a client redirect occurring before
// `features::kBtmClientBounceDetectionTimeout` is considered a bounce whilst
// leaving server redirects unaffected.
TEST_F(BtmBounceDetectorTest,
       DetectStatefulRedirects_Before_ClientBounceDetectionTimeout) {
  NavigateTo("http://a.test", kWithUserGesture);
  auto mocked_bounce_time_1 = GetCurrentTime();
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .Finish(true);
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get() -
                 base::Seconds(1));
  auto mocked_bounce_time_2 = GetCurrentTime();
  StartNavigation("http://e.test", kNoUserGesture)
      .RedirectTo("http://f.test")
      .RedirectTo("http://g.test")
      .Finish(true);
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get() -
                 base::Seconds(1));
  auto mocked_bounce_time_3 = GetCurrentTime();
  StartNavigation("http://h.test", kWithUserGesture)
      .RedirectTo("http://i.test")
      .RedirectTo("http://j.test")
      .Finish(true);

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/5] a.test/ -> b.test/ (None) -> g.test/"),
                               ("[2/5] a.test/ -> c.test/ (None) -> g.test/"),
                               ("[3/5] a.test/ -> d.test/ (None) -> g.test/"),
                               ("[4/5] a.test/ -> e.test/ (None) -> g.test/"),
                               ("[5/5] a.test/ -> f.test/ (None) -> g.test/"),
                               ("[1/2] g.test/ -> h.test/ (None) -> j.test/"),
                               ("[2/2] g.test/ -> i.test/ (None) -> j.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://f.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://h.test", mocked_bounce_time_3,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://i.test", mocked_bounce_time_3,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

// Ensures that for every navigation, a client redirect occurring after
// `features::kBtmClientBounceDetectionTimeout` is not considered a bounce
// whilst leaving server redirects unaffected.
TEST_F(BtmBounceDetectorTest,
       DetectStatefulRedirects_After_ClientBounceDetectionTimeout) {
  NavigateTo("http://a.test", kWithUserGesture);
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get());
  auto mocked_bounce_time_1 = GetCurrentTime();
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .Finish(true);
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get());
  auto mocked_bounce_time_2 = GetCurrentTime();
  StartNavigation("http://e.test", kNoUserGesture)
      .RedirectTo("http://f.test")
      .RedirectTo("http://g.test")
      .Finish(true);

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/2] a.test/ -> b.test/ (None) -> d.test/"),
                               ("[2/2] a.test/ -> c.test/ (None) -> d.test/"),
                               ("[1/2] d.test/ -> e.test/ (None) -> g.test/"),
                               ("[2/2] d.test/ -> f.test/ (None) -> g.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time_1,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time_2,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://f.test", mocked_bounce_time_2,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Server) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .AccessCookie(CookieOperation::kRead)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(),
              testing::ElementsAre(
                  ("[1/3] a.test/ -> b.test/ (Read) -> e.test/"),
                  ("[2/3] a.test/ -> c.test/ (Write) -> e.test/"),
                  ("[3/3] a.test/ -> d.test/ (ReadWrite) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/true),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 2);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Server_OnStartUp) {
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .AccessCookie(CookieOperation::kRead)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/3] blank -> b.test/ (Read) -> e.test/"),
                           ("[2/3] blank -> c.test/ (Write) -> e.test/"),
                           ("[3/3] blank -> d.test/ (ReadWrite) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/true),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/true)));
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Server_LateNotification) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .RedirectTo("http://e.test")
      .Finish(true);

  LateAccessClientCookie("http://b.test", CookieOperation::kChange);
  LateAccessClientCookie("http://c.test", CookieOperation::kRead);
  LateAccessClientCookie("http://d.test", CookieOperation::kChange);
  LateAccessClientCookie("http://e.test", CookieOperation::kRead);
  LateAccessClientCookie("http://e.test", CookieOperation::kChange);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/3] a.test/ -> b.test/ (ReadWrite) -> e.test/"),
                           ("[2/3] a.test/ -> c.test/ (Read) -> e.test/"),
                           ("[3/3] a.test/ -> d.test/ (Write) -> e.test/")));

  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/true),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 2);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Client) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get() -
                 base::Seconds(1));
  NavigateTo("http://c.test", kNoUserGesture);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/1] a.test/ -> b.test/ (None) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://b.test", mocked_bounce_time, /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Client_OnStartUp) {
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kRead);
  AccessClientCookie(CookieOperation::kChange);
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get() -
                 base::Seconds(1));
  NavigateTo("http://b.test", kNoUserGesture);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/1] blank -> a.test/ (ReadWrite) -> b.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://a.test", mocked_bounce_time, /*stateful=*/true)));
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Client_MergeCookies) {
  NavigateTo("http://a.test", kWithUserGesture);
  // Server cookie read:
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kRead)
      .Finish(true);
  // Client cookie write:
  // NOTE: This navigation's client redirect will always be considered a bounce
  // because of the (frozen) mocked clock.
  AccessClientCookie(CookieOperation::kChange);
  NavigateTo("http://c.test", kNoUserGesture);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  // Redirect cookie access is reported as ReadWrite.
  EXPECT_THAT(redirects(),
              testing::ElementsAre(
                  ("[1/1] a.test/ -> b.test/ (ReadWrite) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(MakeBounceTuple(
                  "http://b.test", mocked_bounce_time, /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 1);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_ServerClientServer) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .Finish(true);
  StartNavigation("http://d.test", kNoUserGesture)
      .RedirectTo("http://e.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> e.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> e.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> e.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Server_Uncommitted) {
  NavigateTo("http://a.test", kWithUserGesture);
  StartNavigation("http://b.test", kWithUserGesture)
      .RedirectTo("http://c.test")
      .RedirectTo("http://d.test")
      .Finish(false);
  // Because the previous navigation didn't commit, the following chain still
  // starts from http://a.test/.
  StartNavigation("http://e.test", kWithUserGesture)
      .RedirectTo("http://f.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> a.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> a.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> a.test/"),
                               ("[1/1] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(BtmBounceDetectorTest, DetectStatefulRedirect_Client_Uncommitted) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  StartNavigation("http://c.test", kNoUserGesture)
      .RedirectTo("http://d.test")
      .Finish(false);
  // Because the previous navigation didn't commit, the following chain still
  // starts from http://a.test/.
  StartNavigation("http://e.test", kNoUserGesture)
      .RedirectTo("http://f.test")
      .Finish(true);

  auto mocked_bounce_time = GetCurrentTime();

  EndPendingRedirectChain();

  EXPECT_THAT(redirects(), testing::ElementsAre(
                               ("[1/3] a.test/ -> b.test/ (None) -> b.test/"),
                               ("[2/3] a.test/ -> c.test/ (None) -> b.test/"),
                               ("[3/3] a.test/ -> d.test/ (None) -> b.test/"),
                               ("[1/2] a.test/ -> b.test/ (None) -> f.test/"),
                               ("[2/2] a.test/ -> e.test/ (None) -> f.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::UnorderedElementsAre(
                  MakeBounceTuple("http://b.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://c.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://d.test", mocked_bounce_time,
                                  /*stateful=*/false),
                  MakeBounceTuple("http://e.test", mocked_bounce_time,
                                  /*stateful=*/false)));
  EXPECT_EQ(stateful_bounce_count(), 0);
}

TEST_F(BtmBounceDetectorTest,
       ReportRedirectorsInChain_OnEachFinishedNavigation) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));
  // Access cookies on d.test.
  AccessClientCookie(CookieOperation::kChange);

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test.
  StartNavigation("http://e.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, e.test"));
}

TEST_F(BtmBounceDetectorTest,
       ReportRedirectorsInChain_IncludingUncommittedNavigations) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Start a redirect chain that doesn't commit.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(false);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test, c.test, d.test"));

  // Because the previous navigation didn't commit, the following chain still
  // starts from http://a.test/.
  StartNavigation("http://e.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test, c.test, d.test", "e.test"));
}

TEST_F(BtmBounceDetectorTest,
       ReportRedirectorsInChain_IncludingNonStatefulRedirects) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which accesses cookies,
  // then S-redirects to c.test (which doesn't access cookies).
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test (which doesn't
  // access cookies).
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));

  // Navigate without a click (i.e. by C-redirecting) to e.test, which accesses
  // cookies, then S-redirects to f.test.
  StartNavigation("http://e.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, e.test"));
}

// This test verifies that sites in a redirect chain that are the same as the
// starting site (i.e., last site before the redirect chain started) are not
// reported.
TEST_F(BtmBounceDetectorTest,
       ReportRedirectorsInChain_OmitSitesMatchingStartSite) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // a.test, which S-redirects to c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://a.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to a.test.
  NavigateTo("http://a.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));
  // Access cookies via JS on a.test.
  AccessClientCookie(CookieOperation::kChange);

  // Navigate without a click (i.e. by C-redirecting) to d.test, which
  // S-redirects to e.test, which S-redirects to f.test.
  StartNavigation("http://d.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, e.test"));
}

// This test verifies that sites in a (server) redirect chain that are the same
// as the ending site of a navigation are not reported.
TEST_F(BtmBounceDetectorTest,
       ReportRedirectorsInChain_OmitSitesMatchingEndSite) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test, which S-redirects to c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test", "c.test"));
  // Access cookies via JS on d.test.
  AccessClientCookie(CookieOperation::kChange);

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test, which S-redirects to e.test.
  StartNavigation("http://e.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://f.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://e.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(true);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test", "c.test", "d.test, f.test"));
}

TEST_F(BtmBounceDetectorTest,
       ReportRedirectorsInChain_OmitSitesMatchingEndSite_Uncommitted) {
  // Visit initial page on a.test and access cookies via JS.
  NavigateTo("http://a.test", kWithUserGesture);
  AccessClientCookie(CookieOperation::kChange);

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test, which S-redirects to c.test.
  StartNavigation("http://b.test", kWithUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://c.test")
      .AccessCookie(CookieOperation::kChange)
      .Finish(false);
  EXPECT_THAT(GetReportedSites(), testing::ElementsAre("b.test, c.test"));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  // NOTE: Because the previous navigation didn't commit, the chain still
  // starts from http://a.test/.
  NavigateTo("http://d.test", kNoUserGesture);
  EXPECT_THAT(GetReportedSites(),
              testing::ElementsAre("b.test, c.test", "a.test"));
}

const std::vector<std::string>& GetAllRedirectMetrics() {
  static const std::vector<std::string> kAllRedirectMetrics = {
      // clang-format off
      "ClientBounceDelay",
      "CookieAccessType",
      "HasStickyActivation",
      "InitialAndFinalSitesSame",
      "RedirectAndFinalSiteSame",
      "RedirectAndInitialSiteSame",
      "RedirectChainIndex",
      "RedirectChainLength",
      "RedirectType",
      "SiteEngagementLevel",
      "WebAuthnAssertionRequestSucceeded",
      "SiteHadUserActivation",
      "SiteHadWebAuthnAssertion",
      // clang-format on
  };
  return kAllRedirectMetrics;
}

TEST_F(BtmBounceDetectorTest, Histograms_UMA) {
  base::HistogramTester histograms;

  SetSiteHasUserActivation("http://b.test");

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceBtmTime(base::Seconds(3));
  AccessClientCookie(CookieOperation::kRead);
  StartNavigation("http://c.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .Finish(true);
  EndPendingRedirectChain();

  base::HistogramTester::CountsMap expected_counts;
  expected_counts["Privacy.DIPS.BounceCategoryClient.Block3PC"] = 1;
  expected_counts["Privacy.DIPS.BounceCategoryServer.Block3PC"] = 1;
  EXPECT_THAT(histograms.GetTotalCountsForPrefix("Privacy.DIPS.BounceCategory"),
              testing::ContainerEq(expected_counts));
  // Verify the proper values were recorded. b.test has user engagement and read
  // cookies, while c.test has no user engagement and wrote cookies.
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryClient.Block3PC"),
      testing::ElementsAre(
          // b.test
          Bucket((int)BtmRedirectCategory::kReadCookies_HasEngagement, 1)));
  EXPECT_THAT(
      histograms.GetAllSamples("Privacy.DIPS.BounceCategoryServer.Block3PC"),
      testing::ElementsAre(
          // c.test
          Bucket((int)BtmRedirectCategory::kWriteCookies_NoEngagement, 1)));

  // Verify the time-to-bounce metric was recorded for the client bounce.
  histograms.ExpectBucketCount(
      "Privacy.DIPS.TimeFromNavigationCommitToClientBounce",
      static_cast<base::HistogramBase::Sample32>(
          base::Seconds(3).InMilliseconds()),
      /*expected_count=*/1);
}

TEST_F(BtmBounceDetectorTest, Histograms_UKM) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  SetSiteHasUserActivation("http://c.test");

  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);
  AdvanceBtmTime(base::Seconds(2));
  AccessClientCookie(CookieOperation::kRead);
  TriggerWebAuthnAssertionRequestSucceeded();
  StartNavigation("http://c.test", kNoUserGesture)
      .AccessCookie(CookieOperation::kChange)
      .RedirectTo("http://d.test")
      .Finish(true);

  EndPendingRedirectChain();

  std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> ukm_entries =
      ukm_recorder.GetEntries("BTM.Redirect", GetAllRedirectMetrics());
  ASSERT_EQ(2u, ukm_entries.size());

  EXPECT_THAT(URLForNavigationSourceId(ukm_entries[0].source_id),
              Eq("b.test/"));
  EXPECT_THAT(
      ukm_entries[0].metrics,
      ElementsAre(Pair("ClientBounceDelay", 2),
                  Pair("CookieAccessType", (int)BtmDataAccessType::kRead),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 0), Pair("RedirectChainLength", 2),
                  Pair("RedirectType", (int)BtmRedirectType::kClient),
                  Pair("SiteHadUserActivation", false),
                  Pair("SiteHadWebAuthnAssertion", false),
                  Pair("WebAuthnAssertionRequestSucceeded", true)));

  EXPECT_THAT(URLForRedirectSourceId(&ukm_recorder, ukm_entries[1].source_id),
              Eq("c.test/"));
  EXPECT_THAT(
      ukm_entries[1].metrics,
      ElementsAre(Pair("ClientBounceDelay", 0),
                  Pair("CookieAccessType", (int)BtmDataAccessType::kWrite),
                  Pair("HasStickyActivation", false),
                  Pair("InitialAndFinalSitesSame", false),
                  Pair("RedirectAndFinalSiteSame", false),
                  Pair("RedirectAndInitialSiteSame", false),
                  Pair("RedirectChainIndex", 1), Pair("RedirectChainLength", 2),
                  Pair("RedirectType", (int)BtmRedirectType::kServer),
                  Pair("SiteHadUserActivation", true),
                  Pair("SiteHadWebAuthnAssertion", false),
                  Pair("WebAuthnAssertionRequestSucceeded", false)));
}

TEST_F(BtmBounceDetectorTest, SiteHadUserActivationInteraction) {
  NavigateTo("http://a.test", kWithUserGesture);
  ActivatePage();
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get() +
                 base::Seconds(1));

  StartNavigation("http://b.test", kNoUserGesture)
      .RedirectTo("http://c.test")
      .Finish(/*commit=*/true);
  ActivatePage();
  NavigateTo("http://d.test", kNoUserGesture);

  // Expect one initial URL (a.test) and two redirects (b.test, c.test).
  EXPECT_EQ(CommittedRedirectContext().GetInitialURLForTesting(),
            GURL("http://a.test"));
  EXPECT_EQ(CommittedRedirectContext().GetRedirectChainLength(), 2u);

  EXPECT_TRUE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("a.test"));
  EXPECT_FALSE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("b.test"));
  EXPECT_TRUE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("c.test"));
  EXPECT_FALSE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("d.test"));
}

TEST_F(BtmBounceDetectorTest, SiteHadWebAuthnInteraction) {
  NavigateTo("http://a.test", kWithUserGesture);
  ActivatePage();
  AdvanceBtmTime(features::kBtmClientBounceDetectionTimeout.Get() +
                 base::Seconds(1));

  StartNavigation("http://b.test", kNoUserGesture)
      .RedirectTo("http://c.test")
      .Finish(/*commit=*/true);
  TriggerWebAuthnAssertionRequestSucceeded();
  NavigateTo("http://d.test", kNoUserGesture);

  // Expect one initial URL (a.test) and two redirects (b.test, c.test).
  EXPECT_EQ(CommittedRedirectContext().GetInitialURLForTesting(),
            GURL("http://a.test"));
  EXPECT_EQ(CommittedRedirectContext().GetRedirectChainLength(), 2u);

  EXPECT_TRUE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("a.test"));
  EXPECT_FALSE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("b.test"));
  EXPECT_TRUE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("c.test"));
  EXPECT_FALSE(
      CommittedRedirectContext().SiteHadUserActivationOrAuthn("d.test"));
}

TEST_F(BtmBounceDetectorTest, ClientCookieAccessDuringNavigation) {
  NavigateTo("http://a.test", kWithUserGesture);
  NavigateTo("http://b.test", kWithUserGesture);

  auto nav = StartNavigation("http://c.test", kNoUserGesture);
  // b.test accesses cookies after the navigation started.
  AccessClientCookie(CookieOperation::kChange);
  nav.Finish(true);

  EndPendingRedirectChain();

  // The b.test bounce is considered stateful.
  EXPECT_THAT(
      redirects(),
      testing::ElementsAre(("[1/1] a.test/ -> b.test/ (Write) -> c.test/")));
  EXPECT_THAT(GetRecordedBounces(),
              testing::ElementsAre(testing::FieldsAre(
                  GURL("http://b.test"), testing::_, /*stateful=*/true)));
  EXPECT_EQ(stateful_bounce_count(), 1);
}

using ChainPair =
    std::pair<BtmRedirectChainInfoPtr, std::vector<BtmRedirectInfoPtr>>;

void AppendChainPair(std::vector<ChainPair>& vec,
                     std::vector<BtmRedirectInfoPtr> redirects,
                     BtmRedirectChainInfoPtr chain) {
  vec.emplace_back(std::move(chain), std::move(redirects));
}

std::vector<BtmRedirectInfoPtr> MakeServerRedirects(
    std::vector<std::string> urls,
    BtmDataAccessType access_type = BtmDataAccessType::kReadWrite) {
  std::vector<BtmRedirectInfoPtr> redirects;
  for (const auto& url : urls) {
    redirects.push_back(BtmRedirectInfo::CreateForServer(
        /*redirector_url=*/GURL(url),
        /*redirector_source_id=*/ukm::AssignNewSourceId(),
        /*access_type=*/access_type,
        /*time=*/base::Time::Now(),
        /*was_response_cached=*/false,
        /*response_code=*/net::HTTP_FOUND,
        /*server_bounce_delay=*/base::TimeDelta()));
  }
  return redirects;
}

BtmRedirectInfoPtr MakeClientRedirect(
    std::string url,
    BtmDataAccessType access_type = BtmDataAccessType::kReadWrite,
    bool has_sticky_activation = false,
    bool has_web_authn_assertion = false) {
  return BtmRedirectInfo::CreateForClient(
      /*redirector_url=*/GURL(url),
      /*redirector_source_id=*/ukm::AssignNewSourceId(),
      /*access_type=*/access_type,
      /*time=*/base::Time::Now(),
      /*client_bounce_delay=*/base::Seconds(1),
      /*has_sticky_activation=*/has_sticky_activation,
      /*web_authn_assertion_request_succeeded*/ has_web_authn_assertion);
}

Btm3PcSettingsCallback GetAre3pcsAllowedCallback() {
  return base::BindRepeating([] { return false; });
}

MATCHER_P(HasUrl, url, "") {
  *result_listener << "whose url is " << arg->redirector_url;
  return ExplainMatchResult(Eq(url), arg->redirector_url, result_listener);
}

MATCHER_P(HasRedirectType, redirect_type, "") {
  *result_listener << "whose redirect_type is "
                   << BtmRedirectTypeToString(arg->redirect_type);
  return ExplainMatchResult(Eq(redirect_type), arg->redirect_type,
                            result_listener);
}

MATCHER_P(HasBtmDataAccessType, access_type, "") {
  *result_listener << "whose access_type is "
                   << BtmDataAccessTypeToString(arg->access_type);
  return ExplainMatchResult(Eq(access_type), arg->access_type, result_listener);
}

MATCHER_P(HasInitialUrl, url, "") {
  *result_listener << "whose initial_url is " << arg->initial_url;
  return ExplainMatchResult(Eq(url), arg->initial_url, result_listener);
}

MATCHER_P(HasFinalUrl, url, "") {
  *result_listener << "whose final_url is " << arg->final_url;
  return ExplainMatchResult(Eq(url), arg->final_url, result_listener);
}

MATCHER_P(HasLength, length, "") {
  *result_listener << "whose length is " << arg->length;
  return ExplainMatchResult(Eq(length), arg->length, result_listener);
}

TEST(BtmRedirectContextTest, OneAppend) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  context.EndChain(GURL("http://d.test/"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_FALSE(chains[0].first->are_3pcs_generally_enabled);
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));
}

TEST(BtmRedirectContextTest, TwoAppends_NoClientRedirect) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://d.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://e.test/"}), GURL("http://f.test/"),
      ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 1u);
  context.EndChain(GURL("http://f.test/"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 2u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));

  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://d.test/"),
                    HasFinalUrl("http://f.test/"), HasLength(1u)));
  EXPECT_THAT(chains[1].second, ElementsAre(HasUrl("http://e.test/")));
}

TEST(BtmRedirectContextTest, TwoAppends_WithClientRedirect) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      MakeClientRedirect("http://d.test/"),
      MakeServerRedirects({"http://e.test/", "http://f.test/"}),
      GURL("http://g.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  context.EndChain(GURL("http://g.test/"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://g.test/"), HasLength(5u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(AllOf(HasUrl("http://b.test/"),
                                HasRedirectType(BtmRedirectType::kServer)),
                          AllOf(HasUrl("http://c.test/"),
                                HasRedirectType(BtmRedirectType::kServer)),
                          AllOf(HasUrl("http://d.test/"),
                                HasRedirectType(BtmRedirectType::kClient)),
                          AllOf(HasUrl("http://e.test/"),
                                HasRedirectType(BtmRedirectType::kServer)),
                          AllOf(HasUrl("http://f.test/"),
                                HasRedirectType(BtmRedirectType::kServer))));
}

TEST(BtmRedirectContextTest, OnlyClientRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()), {},
      GURL("http://b.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(MakeClientRedirect("http://b.test/"), {},
                          GURL("http://c.test/"), ukm::AssignNewSourceId(),
                          false);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(MakeClientRedirect("http://c.test/"), {},
                          GURL("http://d.test/"), ukm::AssignNewSourceId(),
                          false);
  ASSERT_EQ(chains.size(), 0u);
  context.EndChain(GURL("http://d.test"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));
}

TEST(BtmRedirectContextTest, OverflowMaxChain_TrimsFromFront) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()), {},
      GURL("http://c.test/"), ukm::AssignNewSourceId(), false);
  for (size_t ind = 0; ind < kBtmRedirectChainMax; ind++) {
    std::string redirect_url =
        base::StrCat({"http://", base::NumberToString(ind), ".test/"});
    context.AppendCommitted(MakeClientRedirect(redirect_url), {},
                            GURL("http://c.test/"), ukm::AssignNewSourceId(),
                            false);
  }
  // Each redirect was added to the chain.
  ASSERT_EQ(context.size(), kBtmRedirectChainMax);
  ASSERT_EQ(chains.size(), 0u);

  // The next redirect overflows the chain and evicts the first one.
  context.AppendCommitted(MakeClientRedirect("http://b.test/"), {},
                          GURL("http://c.test/"), ukm::AssignNewSourceId(),
                          false);
  ASSERT_EQ(context.size(), kBtmRedirectChainMax);
  ASSERT_EQ(chains.size(), 1u);
  context.EndChain(GURL("http://c.test/"), ukm::AssignNewSourceId(), false);

  // Expect two chains handled: one partial chain with the dropped redirect, and
  // one with the other redirects.
  ASSERT_EQ(chains.size(), 2u);
  EXPECT_THAT(chains[0].first, AllOf(HasInitialUrl("http://a.test/"),
                                     HasLength(kBtmRedirectChainMax + 1)));
  ASSERT_THAT(chains[0].second, SizeIs(1));
  EXPECT_THAT(chains[0].second.at(0),
              AllOf(HasUrl("http://0.test/"),
                    HasRedirectType(BtmRedirectType::kClient)));

  // BtmRedirectChainInfo.length is computed from BtmRedirectInfo.index, so it
  // includes the length of the partial chains.
  EXPECT_THAT(chains[1].first, AllOf(HasInitialUrl("http://a.test/"),
                                     HasFinalUrl("http://c.test/"),
                                     HasLength(kBtmRedirectChainMax + 1)));
  ASSERT_THAT(chains[1].second, SizeIs(kBtmRedirectChainMax));
  // Check that the first redirect in the chain is the second that was added in
  // the setup.
  EXPECT_THAT(chains[1].second.at(0),
              AllOf(HasUrl("http://1.test/"),
                    HasRedirectType(BtmRedirectType::kClient)));
  // Check the last redirect in the full chain.
  EXPECT_THAT(chains[1].second.back(),
              AllOf(HasUrl("http://b.test/"),
                    HasRedirectType(BtmRedirectType::kClient)));
}

TEST(BtmRedirectContextTest, Uncommitted_NoClientRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  context.HandleUncommitted(
      std::make_pair(GURL("http://d.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://e.test/", "http://f.test/"}));
  ASSERT_EQ(chains.size(), 1u);
  context.AppendCommitted(
      std::make_pair(GURL("http://h.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://i.test/"}), GURL("http://j.test/"),
      ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 2u);
  context.EndChain(GURL("http://j.test/"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 3u);
  // First, the uncommitted (middle) chain.
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://d.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(2u)));
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://e.test/"), HasUrl("http://f.test/")));
  // Then the initially-started chain.
  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://h.test/"), HasLength(2u)));
  EXPECT_THAT(chains[1].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/")));
  // Then the last chain.
  EXPECT_THAT(chains[2].first,
              AllOf(HasInitialUrl("http://h.test/"),
                    HasFinalUrl("http://j.test/"), HasLength(1u)));
  EXPECT_THAT(chains[2].second, ElementsAre(HasUrl("http://i.test/")));
}

TEST(BtmRedirectContextTest, Uncommitted_IncludingClientRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects({"http://b.test/", "http://c.test/"}),
      GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);
  // Uncommitted navigation:
  context.HandleUncommitted(
      MakeClientRedirect("http://d.test/"),
      MakeServerRedirects({"http://e.test/", "http://f.test/"}));
  ASSERT_EQ(chains.size(), 1u);
  context.AppendCommitted(MakeClientRedirect("http://h.test/"),
                          MakeServerRedirects({"http://i.test/"}),
                          GURL("http://j.test/"), ukm::AssignNewSourceId(),
                          false);
  ASSERT_EQ(chains.size(), 1u);
  context.EndChain(GURL("http://j.test/"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 2u);
  // First, the uncommitted chain. The overall length includes the
  // already-committed part of the chain (2 redirects, starting from a.test)
  // plus the uncommitted part (3 redirects, starting from d.test).
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://d.test/"), HasLength(5u)));
  // But only the 3 uncommitted redirects are included in the vector.
  EXPECT_THAT(chains[0].second,
              ElementsAre(HasUrl("http://d.test/"), HasUrl("http://e.test/"),
                          HasUrl("http://f.test/")));
  // Then the initially-started chain.
  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://j.test/"), HasLength(4u)));
  // Committed chains include all redirects in the vector.
  EXPECT_THAT(chains[1].second,
              ElementsAre(HasUrl("http://b.test/"), HasUrl("http://c.test/"),
                          HasUrl("http://h.test/"), HasUrl("http://i.test/")));
}

TEST(BtmRedirectContextTest, NoRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(chains.size(), 0u);

  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()), {},
      GURL("http://b.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 0u);

  context.AppendCommitted(
      std::make_pair(GURL("http://b.test/"), ukm::AssignNewSourceId()), {},
      GURL("http://c.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 1u);

  context.EndChain(GURL("http://e.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(chains.size(), 2u);

  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://b.test/"), HasLength(0u)));
  EXPECT_THAT(chains[0].second, IsEmpty());

  EXPECT_THAT(chains[1].first,
              AllOf(HasInitialUrl("http://b.test/"),
                    HasFinalUrl("http://e.test/"), HasLength(0u)));
  EXPECT_THAT(chains[1].second, IsEmpty());
}

TEST(BtmRedirectContextTest, AddLateCookieAccess) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      MakeServerRedirects(
          {"http://b.test/", "http://c.test/", "http://d.test/"},
          BtmDataAccessType::kNone),
      GURL("http://e.test/"), ukm::AssignNewSourceId(), false);

  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://d.test/"),
                                          CookieOperation::kChange));
  // Update c.test even though it preceded d.test:
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://c.test/"),
                                          CookieOperation::kRead));

  context.AppendCommitted(
      MakeClientRedirect("http://e.test/", BtmDataAccessType::kNone),
      MakeServerRedirects({"http://f.test/", "http://g.test/"},
                          BtmDataAccessType::kRead),
      GURL("http://h.test/"), ukm::AssignNewSourceId(), false);

  context.AppendCommitted(
      MakeClientRedirect("http://h.test/", BtmDataAccessType::kNone),
      MakeServerRedirects({"http://i.test/"}, BtmDataAccessType::kRead),
      GURL("http://j.test/"), ukm::AssignNewSourceId(), false);

  // Since kMaxLookback=5, AddLateCookieAccess() can attribute late accesses to
  // the last 5 redirects:
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://i.test/"),
                                          CookieOperation::kRead));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://h.test/"),
                                          CookieOperation::kRead));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://g.test/"),
                                          CookieOperation::kChange));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://f.test/"),
                                          CookieOperation::kRead));
  EXPECT_TRUE(context.AddLateCookieAccess(GURL("http://e.test/"),
                                          CookieOperation::kRead));
  // But it will fail to update d.test since it's too far back in the chain.
  EXPECT_FALSE(context.AddLateCookieAccess(GURL("http://d.test/"),
                                           CookieOperation::kRead));

  context.EndChain(GURL("http://j.test/"), ukm::AssignNewSourceId(), false);

  ASSERT_EQ(chains.size(), 1u);
  EXPECT_THAT(chains[0].first,
              AllOf(HasInitialUrl("http://a.test/"),
                    HasFinalUrl("http://j.test/"), HasLength(8u)));
  EXPECT_THAT(
      chains[0].second,
      ElementsAre(AllOf(HasUrl("http://b.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kNone)),
                  AllOf(HasUrl("http://c.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kRead)),
                  AllOf(HasUrl("http://d.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kWrite)),
                  AllOf(HasUrl("http://e.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kRead)),
                  AllOf(HasUrl("http://f.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kRead)),
                  AllOf(HasUrl("http://g.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kReadWrite)),
                  AllOf(HasUrl("http://h.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kRead)),
                  AllOf(HasUrl("http://i.test/"),
                        HasBtmDataAccessType(BtmDataAccessType::kRead))));
}

TEST(BtmRedirectContextTest,
     GetServerRedirectsSinceLastPrimaryPageChangeNoRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  ASSERT_EQ(context.size(), 0u);

  base::span<const BtmRedirectInfoPtr> server_redirects =
      context.GetServerRedirectsSinceLastPrimaryPageChange();

  EXPECT_EQ(server_redirects.size(), 0u);
}

TEST(BtmRedirectContextTest,
     GetServerRedirectsSinceLastPrimaryPageChangeOnlyClientSideRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  context.AppendCommitted(
      MakeClientRedirect("http://a.test/", BtmDataAccessType::kNone, false,
                         true),
      {}, GURL("http://b.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(context.size(), 1u);

  base::span<const BtmRedirectInfoPtr> server_redirects =
      context.GetServerRedirectsSinceLastPrimaryPageChange();

  EXPECT_EQ(server_redirects.size(), 0u);
}

TEST(BtmRedirectContextTest,
     GetServerRedirectsSinceLastPrimaryPageChangeOnlyServerSideRedirects) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      {MakeServerRedirects({"http://b.test/", "http://c.test/"})},
      GURL("http://d.test"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(context.size(), 2u);

  base::span<const BtmRedirectInfoPtr> server_redirects =
      context.GetServerRedirectsSinceLastPrimaryPageChange();

  EXPECT_EQ(server_redirects.size(), 2u);
  EXPECT_EQ(server_redirects[0]->redirector_url, "http://b.test/");
  EXPECT_EQ(server_redirects[0]->redirect_type, BtmRedirectType::kServer);
  EXPECT_EQ(server_redirects[1]->redirector_url, "http://c.test/");
  EXPECT_EQ(server_redirects[1]->redirect_type, BtmRedirectType::kServer);
}

TEST(
    BtmRedirectContextTest,
    GetServerRedirectsSinceLastPrimaryPageChangeNoServerSideRedirectsSinceLastClientSideRedirect) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      {MakeServerRedirects({"http://b.test"})}, GURL("http://c.test/"),
      ukm::AssignNewSourceId(), false);
  context.AppendCommitted(
      MakeClientRedirect("http://b.test/", BtmDataAccessType::kNone, false,
                         true),
      {}, GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(context.size(), 2u);

  base::span<const BtmRedirectInfoPtr> server_redirects =
      context.GetServerRedirectsSinceLastPrimaryPageChange();

  EXPECT_EQ(server_redirects.size(), 0u);
}

TEST(
    BtmRedirectContextTest,
    GetServerRedirectsSinceLastPrimaryPageChangeServerSideRedirectsPrecededByClientSideRedirect) {
  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);
  context.AppendCommitted(
      std::make_pair(GURL("http://a.test/"), ukm::AssignNewSourceId()),
      {MakeServerRedirects({"http://b.test/"})}, GURL("http://c.test/"),
      ukm::AssignNewSourceId(), false);
  context.AppendCommitted(
      MakeClientRedirect("http://b.test/", BtmDataAccessType::kNone, false,
                         true),
      MakeServerRedirects({"http://a.test/server-redirect/"}),
      GURL("http://d.test/"), ukm::AssignNewSourceId(), false);
  ASSERT_EQ(context.size(), 3u);
  ASSERT_EQ(context[0].redirect_type, BtmRedirectType::kServer);
  ASSERT_EQ(context[1].redirect_type, BtmRedirectType::kClient);
  ASSERT_EQ(context[2].redirect_type, BtmRedirectType::kServer);

  base::span<const BtmRedirectInfoPtr> server_redirects =
      context.GetServerRedirectsSinceLastPrimaryPageChange();

  EXPECT_EQ(server_redirects.size(), 1u);
  EXPECT_EQ(server_redirects[0]->redirector_url,
            "http://a.test/server-redirect/");
  EXPECT_EQ(server_redirects[0]->redirect_type, BtmRedirectType::kServer);
}

}  // namespace content
