// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_CHANGE_UNITTEST_H_
#define NET_COOKIES_COOKIE_STORE_CHANGE_UNITTEST_H_

#include "base/functional/bind.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_change_dispatcher_test_helpers.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

// Used to sort CookieChanges when testing stores without exact change ordering.
//
// The ordering relation must match the order in which the tests below issue
// cookie calls. Changes to this method should be tested by running the tests
// below with CookieMonsterTestTraits::has_exact_change_ordering set to both
// true and false.
bool CookieChangeLessThan(const CookieChangeInfo& lhs,
                          const CookieChangeInfo& rhs) {
  if (lhs.cookie.Name() != rhs.cookie.Name())
    return lhs.cookie.Name() < rhs.cookie.Name();

  if (lhs.cookie.Value() != rhs.cookie.Value())
    return lhs.cookie.Value() < rhs.cookie.Value();

  if (lhs.cookie.Domain() != rhs.cookie.Domain())
    return lhs.cookie.Domain() < rhs.cookie.Domain();

  return lhs.cause < rhs.cause;
}

}  // namespace

// Google Test supports at most 50 tests per typed case, so the tests here are
// broken up into multiple cases.
template <class CookieStoreTestTraits>
class CookieStoreChangeTestBase
    : public CookieStoreTest<CookieStoreTestTraits> {
 protected:
  using CookieStoreTest<CookieStoreTestTraits>::FindAndDeleteCookie;

  // Drains all pending tasks on the run loop(s) involved in the test.
  void DeliverChangeNotifications() {
    CookieStoreTestTraits::DeliverChangeNotifications();
  }

  bool FindAndDeleteCookie(CookieStore* cs,
                           const std::string& domain,
                           const std::string& name,
                           const std::string& path) {
    for (auto& cookie : this->GetAllCookies(cs)) {
      if (cookie.Domain() == domain && cookie.Name() == name &&
          cookie.Path() == path) {
        return this->DeleteCanonicalCookie(cs, cookie);
      }
    }

    return false;
  }

  // Could be static, but it's actually easier to have it be a member function.
  ::testing::AssertionResult MatchesCause(CookieChangeCause expected_cause,
                                          CookieChangeCause actual_cause) {
    if (!CookieChangeCauseIsDeletion(expected_cause) ||
        CookieStoreTestTraits::has_exact_change_cause) {
      if (expected_cause == actual_cause)
        return ::testing::AssertionSuccess();
      return ::testing::AssertionFailure()
             << "expected " << expected_cause << " got " << actual_cause;
    }
    if (CookieChangeCauseIsDeletion(actual_cause))
      return ::testing::AssertionSuccess();
    return ::testing::AssertionFailure()
           << "expected a deletion cause, got " << actual_cause;
  }

  bool IsExpectedAccessSemantics(net::CookieAccessSemantics expected_semantics,
                                 net::CookieAccessSemantics actual_semantics) {
    if (CookieStoreTestTraits::supports_cookie_access_semantics)
      return expected_semantics == actual_semantics;
    return actual_semantics == net::CookieAccessSemantics::UNKNOWN;
  }

  static void OnCookieChange(std::vector<CookieChangeInfo>* changes,
                             const CookieChangeInfo& notification) {
    if (CookieStoreTestTraits::has_exact_change_ordering) {
      changes->push_back(notification);
    } else {
      // Assumes the vector is sorted before the insertion. If true, the vector
      // will remain sorted.
      changes->insert(std::upper_bound(changes->begin(), changes->end(),
                                       notification, CookieChangeLessThan),
                      notification);
    }
  }
};

template <class CookieStoreTestTraits>
class CookieStoreChangeGlobalTest
    : public CookieStoreChangeTestBase<CookieStoreTestTraits> {};
TYPED_TEST_SUITE_P(CookieStoreChangeGlobalTest);

template <class CookieStoreTestTraits>
class CookieStoreChangeUrlTest
    : public CookieStoreChangeTestBase<CookieStoreTestTraits> {};
TYPED_TEST_SUITE_P(CookieStoreChangeUrlTest);

template <class CookieStoreTestTraits>
class CookieStoreChangeNamedTest
    : public CookieStoreChangeTestBase<CookieStoreTestTraits> {};
TYPED_TEST_SUITE_P(CookieStoreChangeNamedTest);

TYPED_TEST_P(CookieStoreChangeGlobalTest, NoCookie) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, InitialCookie) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  this->SetCookie(cs, this->http_www_foo_.url(), "A=B");
  this->DeliverChangeNotifications();
  std::unique_ptr<CookieChangeSubscription> subscription(
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes))));
  this->DeliverChangeNotifications();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, InsertOne) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, InsertMany) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "G=H"));
  this->DeliverChangeNotifications();

  // Check that the cookie changes are dispatched before calling GetCookies.
  // This is not an ASSERT because the following expectations produce useful
  // debugging information if they fail.
  EXPECT_EQ(4u, cookie_changes.size());
  EXPECT_EQ("A=B; C=D; E=F", this->GetCookies(cs, this->http_www_foo_.url()));
  EXPECT_EQ("G=H", this->GetCookies(cs, this->http_bar_com_.url()));

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("C", cookie_changes[1].cookie.Name());
  EXPECT_EQ("D", cookie_changes[1].cookie.Value());

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[2].cause));
  EXPECT_EQ("E", cookie_changes[2].cookie.Name());
  EXPECT_EQ("F", cookie_changes[2].cookie.Value());

  ASSERT_LE(4u, cookie_changes.size());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes[3].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[3].cause));
  EXPECT_EQ("G", cookie_changes[3].cookie.Name());
  EXPECT_EQ("H", cookie_changes[3].cookie.Value());

  EXPECT_EQ(4u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeleteOne) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "A"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeleteTwo) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "G=H"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(4u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "C"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_bar_com_.url().host(), "G"));
  this->DeliverChangeNotifications();

  // Check that the cookie changes are dispatched before calling GetCookies.
  // This is not an ASSERT because the following expectations produce useful
  // debugging information if they fail.
  EXPECT_EQ(2u, cookie_changes.size());
  EXPECT_EQ("A=B; E=F", this->GetCookies(cs, this->http_www_foo_.url()));
  EXPECT_EQ("", this->GetCookies(cs, this->http_bar_com_.url()));

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));
  EXPECT_EQ("C", cookie_changes[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes[0].cookie.Value());

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[1].cause));
  EXPECT_EQ("G", cookie_changes[1].cookie.Name());
  EXPECT_EQ("H", cookie_changes[1].cookie.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, Overwrite) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get an extra notification.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=C"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("A", cookie_changes[1].cookie.Name());
  EXPECT_EQ("C", cookie_changes[1].cookie.Value());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, OverwriteWithHttpOnly) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  // Insert a cookie "A" for path "/path1"
  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "A=B; path=/path1"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_FALSE(cookie_changes[0].cookie.IsHttpOnly());
  cookie_changes.clear();

  // Insert a cookie "A" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  allow_httponly.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  EXPECT_TRUE(this->CreateAndSetCookie(cs, this->http_www_foo_.url(),
                                       "A=C; path=/path1; httponly",
                                       allow_httponly));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_FALSE(cookie_changes[0].cookie.IsHttpOnly());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("A", cookie_changes[1].cookie.Name());
  EXPECT_EQ("C", cookie_changes[1].cookie.Value());
  EXPECT_TRUE(cookie_changes[1].cookie.IsHttpOnly());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, Deregister) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it is seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  cookie_changes.clear();

  // De-register the subscription.
  subscription.reset();

  // Insert a second cookie and make sure that it's not visible.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->DeliverChangeNotifications();

  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeregisterMultiple) {
  if (!TypeParam::supports_global_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChangeInfo> cookie_changes_1;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes_1)));

  std::vector<CookieChangeInfo> cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
  cookie_changes_2.clear();

  // De-register the second subscription.
  subscription2.reset();

  // Insert a second cookie and make sure that it's only visible in one
  // change array.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("C", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_1[0].cookie.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(0u, cookie_changes_2.size());
}

// Confirm that a listener does not receive notifications for changes that
// happened right before the subscription was established.
TYPED_TEST_P(CookieStoreChangeGlobalTest, DispatchRace) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  // This cookie insertion should not be seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  // DeliverChangeNotifications() must NOT be called before the subscription is
  // established.

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->DeliverChangeNotifications();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("C", cookie_changes[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes[0].cookie.Value());

  ASSERT_EQ(1u, cookie_changes.size());
}

// Confirm that deregistering a subscription blocks the notification if the
// deregistration happened after the change but before the notification was
// received.
TYPED_TEST_P(CookieStoreChangeGlobalTest, DeregisterRace) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  cookie_changes.clear();

  // Insert a cookie, confirm it is not seen, deregister the subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy the subscription so as to lose the race and make sure the
  // task posted arrives after the subscription was destroyed.
  subscription.reset();
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeregisterRaceMultiple) {
  if (!TypeParam::supports_global_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
  cookie_changes_2.clear();

  // Insert a cookie, confirm it is not seen, deregister a subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes_2.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy one of the subscriptions so as to lose the race and make
  // sure the task posted arrives after the subscription was destroyed.
  subscription2.reset();
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("C", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_1[0].cookie.Value());

  // No late notification was received.
  ASSERT_EQ(0u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, MultipleSubscriptions) {
  if (!TypeParam::supports_global_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1U, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());

  ASSERT_EQ(1U, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, ChangeIncludesCookieAccessSemantics) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  // if !supports_cookie_access_semantics, the delegate will be stored but will
  // not be used.
  auto access_delegate = std::make_unique<TestCookieAccessDelegate>();
  access_delegate->SetExpectationForCookieDomain("domain1.test",
                                                 CookieAccessSemantics::LEGACY);
  access_delegate->SetExpectationForCookieDomain(
      "domain2.test", CookieAccessSemantics::NONLEGACY);
  access_delegate->SetExpectationForCookieDomain(
      "domain3.test", CookieAccessSemantics::UNKNOWN);
  cs->SetCookieAccessDelegate(std::move(access_delegate));

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&cookie_changes)));

  this->CreateAndSetCookie(cs, GURL("http://domain1.test"), "cookie=1",
                           CookieOptions::MakeAllInclusive());
  this->CreateAndSetCookie(cs, GURL("http://domain2.test"), "cookie=1",
                           CookieOptions::MakeAllInclusive());
  this->CreateAndSetCookie(cs, GURL("http://domain3.test"), "cookie=1",
                           CookieOptions::MakeAllInclusive());
  this->CreateAndSetCookie(cs, GURL("http://domain4.test"), "cookie=1",
                           CookieOptions::MakeAllInclusive());
  this->DeliverChangeNotifications();

  ASSERT_EQ(4u, cookie_changes.size());

  EXPECT_EQ("domain1.test", cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->IsExpectedAccessSemantics(
      CookieAccessSemantics::LEGACY,
      cookie_changes[0].access_result.access_semantics));
  EXPECT_EQ("domain2.test", cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(this->IsExpectedAccessSemantics(
      CookieAccessSemantics::NONLEGACY,
      cookie_changes[1].access_result.access_semantics));
  EXPECT_EQ("domain3.test", cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(this->IsExpectedAccessSemantics(
      CookieAccessSemantics::UNKNOWN,
      cookie_changes[2].access_result.access_semantics));
  EXPECT_EQ("domain4.test", cookie_changes[3].cookie.Domain());
  EXPECT_TRUE(this->IsExpectedAccessSemantics(
      CookieAccessSemantics::UNKNOWN,
      cookie_changes[3].access_result.access_semantics));
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, PartitionedCookies) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_partitioned_cookies) {
    return;
  }

  CookieStore* cs = this->GetCookieStore();

  // Test that all partitioned cookies are visible to global change listeners.
  std::vector<CookieChangeInfo> all_cookie_changes;
  std::unique_ptr<CookieChangeSubscription> global_subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
          base::Unretained(&all_cookie_changes)));
  // Set two cookies in two separate partitions, one with nonce.
  this->CreateAndSetCookie(
      cs, GURL("https://www.example2.com"),
      "__Host-a=1; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));
  this->CreateAndSetCookie(
      cs, GURL("https://www.example2.com"),
      "__Host-a=2; Secure; Path=/; Partitioned; Max-Age=7200",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(
          GURL("https://www.bar.com"),
          CookiePartitionKey::AncestorChainBit::kCrossSite,
          base::UnguessableToken::Create()));
  this->DeliverChangeNotifications();
  ASSERT_EQ(2u, all_cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, NoCookie) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, InitialCookie) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  this->SetCookie(cs, this->http_www_foo_.url(), "A=B");
  this->DeliverChangeNotifications();
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, InsertOne) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());

  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
}

TYPED_TEST_P(CookieStoreChangeUrlTest, InsertMany) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("C", cookie_changes[1].cookie.Name());
  EXPECT_EQ("D", cookie_changes[1].cookie.Value());

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[2].cause));
  EXPECT_EQ("E", cookie_changes[2].cookie.Name());
  EXPECT_EQ("F", cookie_changes[2].cookie.Value());

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, InsertFiltering) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->www_foo_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=D; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F; path=/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "G=H; path=/foo/bar"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "I=J; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "K=L; domain=foo.com"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("I", cookie_changes[1].cookie.Name());
  EXPECT_EQ("J", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("K", cookie_changes[2].cookie.Name());
  EXPECT_EQ("L", cookie_changes[2].cookie.Value());
  EXPECT_EQ("/", cookie_changes[2].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[2].cause));

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DeleteOne) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "A"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  ASSERT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DeleteTwo) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "G=H"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(4u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "C"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "G"));
  this->DeliverChangeNotifications();

  // Check that the cookie changes are dispatched before calling GetCookies.
  // This is not an ASSERT because the following expectations produce useful
  // debugging information if they fail.
  EXPECT_EQ(2u, cookie_changes.size());
  EXPECT_EQ("A=B; E=F", this->GetCookies(cs, this->http_www_foo_.url()));

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));
  EXPECT_EQ("C", cookie_changes[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes[0].cookie.Value());

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[1].cause));
  EXPECT_EQ("G", cookie_changes[1].cookie.Name());
  EXPECT_EQ("H", cookie_changes[1].cookie.Value());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DeleteFiltering) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->www_foo_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=D; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F; path=/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "G=H; path=/foo/bar"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "I=J; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "K=L; domain=foo.com"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(3u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "A"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_bar_com_.url().host(), "C"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "E"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "G"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "I"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, ".foo.com", "K"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("I", cookie_changes[1].cookie.Name());
  EXPECT_EQ("J", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[1].cause));

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("K", cookie_changes[2].cookie.Name());
  EXPECT_EQ("L", cookie_changes[2].cookie.Value());
  EXPECT_EQ("/", cookie_changes[2].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[2].cause));

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, Overwrite) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get an extra notification.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=C"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("A", cookie_changes[1].cookie.Name());
  EXPECT_EQ("C", cookie_changes[1].cookie.Value());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, OverwriteFiltering) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->www_foo_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=D; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F; path=/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "G=H; path=/foo/bar"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "I=J; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "K=L; domain=foo.com"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(3u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get two notifications per overwrite.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=b; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=d; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=f; path=/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "G=h; path=/foo/bar"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "I=j; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "K=l; domain=foo.com"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[1].cookie.Name());
  EXPECT_EQ("b", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].cause);
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("I", cookie_changes[2].cookie.Name());
  EXPECT_EQ("J", cookie_changes[2].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[2].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[2].cause));

  ASSERT_LE(4u, cookie_changes.size());
  EXPECT_EQ("I", cookie_changes[3].cookie.Name());
  EXPECT_EQ("j", cookie_changes[3].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[3].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[3].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[3].cause));

  ASSERT_LE(5u, cookie_changes.size());
  EXPECT_EQ("K", cookie_changes[4].cookie.Name());
  EXPECT_EQ("L", cookie_changes[4].cookie.Value());
  EXPECT_EQ("/", cookie_changes[4].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[4].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[4].cause));

  ASSERT_LE(6u, cookie_changes.size());
  EXPECT_EQ("K", cookie_changes[5].cookie.Name());
  EXPECT_EQ("l", cookie_changes[5].cookie.Value());
  EXPECT_EQ("/", cookie_changes[5].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[5].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[5].cause));

  EXPECT_EQ(6u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, OverwriteWithHttpOnly) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  // Insert a cookie "A" for path "/foo".
  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->www_foo_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B; path=/foo"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_FALSE(cookie_changes[0].cookie.IsHttpOnly());
  cookie_changes.clear();

  // Insert a cookie "A" for path "/foo", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  allow_httponly.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  EXPECT_TRUE(this->CreateAndSetCookie(cs, this->http_www_foo_.url(),
                                       "A=C; path=/foo; httponly",
                                       allow_httponly));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  EXPECT_FALSE(cookie_changes[0].cookie.IsHttpOnly());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("A", cookie_changes[1].cookie.Name());
  EXPECT_EQ("C", cookie_changes[1].cookie.Value());
  EXPECT_TRUE(cookie_changes[1].cookie.IsHttpOnly());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, Deregister) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it is seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  cookie_changes.clear();

  // De-register the subscription.
  subscription.reset();

  // Insert a second cookie and make sure it's not visible.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->DeliverChangeNotifications();

  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DeregisterMultiple) {
  if (!TypeParam::supports_url_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
  cookie_changes_2.clear();

  // De-register the second registration.
  subscription2.reset();

  // Insert a second cookie and make sure that it's only visible in one
  // change array.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("C", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_1[0].cookie.Value());

  EXPECT_EQ(0u, cookie_changes_2.size());
}

// Confirm that a listener does not receive notifications for changes that
// happened right before the subscription was established.
TYPED_TEST_P(CookieStoreChangeUrlTest, DispatchRace) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  // This cookie insertion should not be seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  // DeliverChangeNotifications() must NOT be called before the subscription is
  // established.

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->DeliverChangeNotifications();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("C", cookie_changes[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes[0].cookie.Value());

  ASSERT_EQ(1u, cookie_changes.size());
}

// Confirm that deregistering a subscription blocks the notification if the
// deregistration happened after the change but before the notification was
// received.
TYPED_TEST_P(CookieStoreChangeUrlTest, DeregisterRace) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes[0].cookie.Value());
  cookie_changes.clear();

  // Insert a cookie, confirm it is not seen, deregister the subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy the subscription so as to lose the race and make sure the
  // task posted arrives after the subscription was destroyed.
  subscription.reset();
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DeregisterRaceMultiple) {
  if (!TypeParam::supports_url_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
  cookie_changes_2.clear();

  // Insert a cookie, confirm it is not seen, deregister a subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes_2.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy one of the subscriptions so as to lose the race and make
  // sure the task posted arrives after the subscription was destroyed.
  subscription2.reset();
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("C", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_1[0].cookie.Value());

  // No late notification was received.
  ASSERT_EQ(0u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DifferentSubscriptionsDisjoint) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_bar_com_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=D"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("C", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].cookie.Domain());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DifferentSubscriptionsDomains) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_bar_com_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=D"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("C", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].cookie.Domain());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DifferentSubscriptionsPaths) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->www_foo_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D; path=/foo"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_1[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_LE(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_2[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].cookie.Domain());

  ASSERT_LE(2u, cookie_changes_2.size());
  EXPECT_EQ("C", cookie_changes_2[1].cookie.Name());
  EXPECT_EQ("D", cookie_changes_2[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_2[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[1].cookie.Domain());

  EXPECT_EQ(2u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, DifferentSubscriptionsFiltering) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::vector<CookieChangeInfo> cookie_changes_3;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_bar_com_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  std::unique_ptr<CookieChangeSubscription> subscription3 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->www_foo_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_3)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());
  EXPECT_EQ(1u, cookie_changes_3.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "C=D"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ(1u, cookie_changes_3.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F; path=/foo"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());
  EXPECT_EQ(1u, cookie_changes_1.size());

  ASSERT_LE(1u, cookie_changes_2.size());
  EXPECT_EQ("C", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("D", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].cookie.Domain());
  EXPECT_EQ(1u, cookie_changes_2.size());

  ASSERT_LE(1u, cookie_changes_3.size());
  EXPECT_EQ("A", cookie_changes_3[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_3[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_3[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_3[0].cookie.Domain());

  ASSERT_LE(2u, cookie_changes_3.size());
  EXPECT_EQ("E", cookie_changes_3[1].cookie.Name());
  EXPECT_EQ("F", cookie_changes_3[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_3[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_3[1].cookie.Domain());

  EXPECT_EQ(2u, cookie_changes_3.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, MultipleSubscriptions) {
  if (!TypeParam::supports_url_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          this->http_www_foo_.url(), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1U, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_1[0].cookie.Value());

  ASSERT_EQ(1U, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("B", cookie_changes_2[0].cookie.Value());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, ChangeIncludesCookieAccessSemantics) {
  if (!TypeParam::supports_url_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  // if !supports_cookie_access_semantics, the delegate will be stored but will
  // not be used.
  auto access_delegate = std::make_unique<TestCookieAccessDelegate>();
  access_delegate->SetExpectationForCookieDomain("domain1.test",
                                                 CookieAccessSemantics::LEGACY);
  cs->SetCookieAccessDelegate(std::move(access_delegate));

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          GURL("http://domain1.test"), std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  this->CreateAndSetCookie(cs, GURL("http://domain1.test"), "cookie=1",
                           CookieOptions::MakeAllInclusive());
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes.size());

  EXPECT_EQ("domain1.test", cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->IsExpectedAccessSemantics(
      CookieAccessSemantics::LEGACY,
      cookie_changes[0].access_result.access_semantics));
}

TYPED_TEST_P(CookieStoreChangeUrlTest, PartitionedCookies) {
  if (!TypeParam::supports_url_cookie_tracking ||
      !TypeParam::supports_partitioned_cookies)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          GURL("https://www.example.com/"),
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  // Unpartitioned cookie
  this->CreateAndSetCookie(cs, GURL("https://www.example.com/"),
                           "__Host-a=1; Secure; Path=/",
                           CookieOptions::MakeAllInclusive());
  // Partitioned cookie with the same partition key
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com/"),
      "__Host-b=2; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://sub.foo.com")));
  // Partitioned cookie with a different partition key
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-c=3; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com")));
  this->DeliverChangeNotifications();

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_FALSE(cookie_changes[0].cookie.IsPartitioned());
  EXPECT_EQ("__Host-a", cookie_changes[0].cookie.Name());
  EXPECT_TRUE(cookie_changes[1].cookie.IsPartitioned());
  EXPECT_EQ(CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
            cookie_changes[1].cookie.PartitionKey().value());
  EXPECT_EQ("__Host-b", cookie_changes[1].cookie.Name());

  // Test that when the partition key parameter is nullopt that all Partitioned
  // cookies do not emit events.

  std::vector<CookieChangeInfo> other_cookie_changes;
  std::unique_ptr<CookieChangeSubscription> other_subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          GURL("https://www.example.com/"),
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&other_cookie_changes)));
  // Update Max-Age: None -> 7200
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-b=2; Secure; Path=/; Partitioned; Max-Age=7200",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, other_cookie_changes.size());
  // Check that the other listener was invoked.
  ASSERT_LT(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeUrlTest, PartitionedCookies_WithNonce) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_partitioned_cookies) {
    return;
  }

  CookieStore* cs = this->GetCookieStore();
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForUrl(
          GURL("https://www.example.com"),
          CookiePartitionKey::FromURLForTesting(
              GURL("https://www.foo.com"),
              CookiePartitionKey::AncestorChainBit::kCrossSite, nonce),
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  // Should not see changes to an unpartitioned cookie.
  this->CreateAndSetCookie(cs, GURL("https://www.example.com"),
                           "__Host-a=1; Secure; Path=/",
                           CookieOptions::MakeAllInclusive());
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Set partitioned cookie without nonce. Should not see the change.
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=2; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Set partitioned cookie with nonce.
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=3; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(
          GURL("https://www.foo.com"),
          CookiePartitionKey::AncestorChainBit::kCrossSite, nonce));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, NoCookie) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InitialCookie) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  this->SetCookie(cs, this->http_www_foo_.url(), "abc=def");
  this->DeliverChangeNotifications();
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InsertOne) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());

  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InsertTwo) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("hij", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InsertFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=ghi; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=jkl; path=/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno; path=/foo/bar"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr; path=/foo"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu; domain=foo.com"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("pqr", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[2].cookie.Name());
  EXPECT_EQ("stu", cookie_changes[2].cookie.Value());
  EXPECT_EQ("/", cookie_changes[2].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[2].cause));

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeleteOne) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "abc"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeleteTwo) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(2u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/foo"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("hij", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[1].cause));
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeleteFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=def; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu; domain=foo.com"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(3u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "xyz"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_bar_com_.url().host(), "abc"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/foo/bar"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/foo"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, ".foo.com", "abc", "/"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("mno", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("pqr", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[1].cause));

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[2].cookie.Name());
  EXPECT_EQ("stu", cookie_changes[2].cookie.Value());
  EXPECT_EQ("/", cookie_changes[2].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::EXPLICIT, cookie_changes[2].cause));

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, Overwrite) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get an extra notification.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=ghi"));
  this->DeliverChangeNotifications();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));

  EXPECT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("ghi", cookie_changes[1].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, OverwriteFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx1; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=def1; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=hij1; path=/foo/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno1; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr1; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu1; domain=foo.com"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(3u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get two notifications per overwrite.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx2; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=def2; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=hij2; path=/foo/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno2; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr2; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu2; domain=foo.com"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("mno1", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("mno2", cookie_changes[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[2].cookie.Name());
  EXPECT_EQ("pqr1", cookie_changes[2].cookie.Value());
  EXPECT_EQ("/", cookie_changes[2].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[2].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[2].cause));

  ASSERT_LE(4u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[3].cookie.Name());
  EXPECT_EQ("pqr2", cookie_changes[3].cookie.Value());
  EXPECT_EQ("/", cookie_changes[3].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[3].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[3].cause));

  ASSERT_LE(5u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[4].cookie.Name());
  EXPECT_EQ("stu1", cookie_changes[4].cookie.Value());
  EXPECT_EQ("/", cookie_changes[4].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[4].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[4].cause));

  ASSERT_LE(6u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[5].cookie.Name());
  EXPECT_EQ("stu2", cookie_changes[5].cookie.Value());
  EXPECT_EQ("/", cookie_changes[5].cookie.Path());
  EXPECT_EQ(".foo.com", cookie_changes[5].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[5].cause));

  EXPECT_EQ(6u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, OverwriteWithHttpOnly) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  // Insert a cookie "abc" for path "/foo".
  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[0].cause));
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_FALSE(cookie_changes[0].cookie.IsHttpOnly());
  cookie_changes.clear();

  // Insert a cookie "a" for path "/foo", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  allow_httponly.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());

  EXPECT_TRUE(this->CreateAndSetCookie(cs, this->http_www_foo_.url(),
                                       "abc=hij; path=/foo; httponly",
                                       allow_httponly));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[0].cookie.Domain());
  EXPECT_TRUE(this->MatchesCause(CookieChangeCause::OVERWRITE,
                                 cookie_changes[0].cause));
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_FALSE(cookie_changes[0].cookie.IsHttpOnly());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes[1].cookie.Domain());
  EXPECT_TRUE(
      this->MatchesCause(CookieChangeCause::INSERTED, cookie_changes[1].cause));
  EXPECT_EQ("abc", cookie_changes[1].cookie.Name());
  EXPECT_EQ("hij", cookie_changes[1].cookie.Value());
  EXPECT_TRUE(cookie_changes[1].cookie.IsHttpOnly());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, Deregister) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it is seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[0].cookie.Path());
  cookie_changes.clear();

  // De-register the subscription.
  subscription.reset();

  // Insert a second cookie and make sure it's not visible.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));
  this->DeliverChangeNotifications();

  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeregisterMultiple) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_1[0].cookie.Path());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_2[0].cookie.Path());
  cookie_changes_2.clear();

  // De-register the second registration.
  subscription2.reset();

  // Insert a second cookie and make sure that it's only visible in one
  // change array.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("hij", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_1[0].cookie.Path());

  EXPECT_EQ(0u, cookie_changes_2.size());
}

// Confirm that a listener does not receive notifications for changes that
// happened right before the subscription was established.
TYPED_TEST_P(CookieStoreChangeNamedTest, DispatchRace) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  // This cookie insertion should not be seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  // DeliverChangeNotifications() must NOT be called before the subscription is
  // established.

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));
  this->DeliverChangeNotifications();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("hij", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes[0].cookie.Path());

  ASSERT_EQ(1u, cookie_changes.size());
}

// Confirm that deregistering a subscription blocks the notification if the
// deregistration happened after the change but before the notification was
// received.
TYPED_TEST_P(CookieStoreChangeNamedTest, DeregisterRace) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes[0].cookie.Path());
  cookie_changes.clear();

  // Insert a cookie, confirm it is not seen, deregister the subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy the subscription so as to lose the race and make sure the
  // task posted arrives after the subscription was destroyed.
  subscription.reset();
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeregisterRaceMultiple) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_1[0].cookie.Path());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_2[0].cookie.Path());
  cookie_changes_2.clear();

  // Insert a cookie, confirm it is not seen, deregister a subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes_2.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy one of the subscriptions so as to lose the race and make
  // sure the task posted arrives after the subscription was destroyed.
  subscription2.reset();
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("hij", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_1[0].cookie.Path());

  // No late notification was received.
  ASSERT_EQ(0u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsDisjoint) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_bar_com_.url(), "ghi",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "ghi=jkl"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("ghi", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("jkl", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].cookie.Domain());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsDomains) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_bar_com_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "abc=ghi"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("ghi", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].cookie.Domain());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsNames) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "ghi",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "ghi=jkl"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("ghi", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("jkl", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].cookie.Domain());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsPaths) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=ghi; path=/foo"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_1[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());

  ASSERT_LE(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_2[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].cookie.Domain());

  ASSERT_LE(2u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[1].cookie.Name());
  EXPECT_EQ("ghi", cookie_changes_2[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_2[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[1].cookie.Domain());

  EXPECT_EQ(2u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::vector<CookieChangeInfo> cookie_changes_3, cookie_changes_4;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "hij",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  std::unique_ptr<CookieChangeSubscription> subscription3 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_bar_com_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_3)));
  std::unique_ptr<CookieChangeSubscription> subscription4 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_4)));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());
  EXPECT_EQ(0u, cookie_changes_4.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());
  EXPECT_EQ(1u, cookie_changes_4.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "hij=mno"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());
  EXPECT_EQ(1u, cookie_changes_4.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "hij=pqr"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "xyz=zyx"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "abc=stu"));
  this->DeliverChangeNotifications();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ(1u, cookie_changes_3.size());
  EXPECT_EQ(1u, cookie_changes_4.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=vwx; path=/foo"));
  this->DeliverChangeNotifications();

  ASSERT_LE(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].cookie.Domain());
  EXPECT_EQ(1u, cookie_changes_1.size());

  ASSERT_LE(1u, cookie_changes_2.size());
  EXPECT_EQ("hij", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("mno", cookie_changes_2[0].cookie.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].cookie.Domain());
  EXPECT_EQ(1u, cookie_changes_2.size());

  ASSERT_LE(1u, cookie_changes_3.size());
  EXPECT_EQ("abc", cookie_changes_3[0].cookie.Name());
  EXPECT_EQ("stu", cookie_changes_3[0].cookie.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_3[0].cookie.Domain());
  EXPECT_EQ(1u, cookie_changes_3.size());

  ASSERT_LE(1u, cookie_changes_4.size());
  EXPECT_EQ("abc", cookie_changes_4[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_4[0].cookie.Value());
  EXPECT_EQ("/", cookie_changes_4[0].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_4[0].cookie.Domain());

  ASSERT_LE(2u, cookie_changes_4.size());
  EXPECT_EQ("abc", cookie_changes_4[1].cookie.Name());
  EXPECT_EQ("vwx", cookie_changes_4[1].cookie.Value());
  EXPECT_EQ("/foo", cookie_changes_4[1].cookie.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_4[1].cookie.Domain());

  EXPECT_EQ(2u, cookie_changes_4.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, MultipleSubscriptions) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChangeInfo> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes_2)));
  this->DeliverChangeNotifications();

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->DeliverChangeNotifications();

  ASSERT_EQ(1U, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_1[0].cookie.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1U, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].cookie.Name());
  EXPECT_EQ("def", cookie_changes_2[0].cookie.Value());
  cookie_changes_2.clear();
}

TYPED_TEST_P(CookieStoreChangeNamedTest, SubscriptionOutlivesStore) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      this->GetCookieStore()->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));
  this->ResetCookieStore();

  // |subscription| outlives cookie_store - crash should not happen.
  subscription.reset();
}

TYPED_TEST_P(CookieStoreChangeNamedTest, ChangeIncludesCookieAccessSemantics) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  // if !supports_cookie_access_semantics, the delegate will be stored but will
  // not be used.
  auto access_delegate = std::make_unique<TestCookieAccessDelegate>();
  access_delegate->SetExpectationForCookieDomain("domain1.test",
                                                 CookieAccessSemantics::LEGACY);
  cs->SetCookieAccessDelegate(std::move(access_delegate));

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          GURL("http://domain1.test"), "cookie",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  this->CreateAndSetCookie(cs, GURL("http://domain1.test"), "cookie=1",
                           CookieOptions::MakeAllInclusive());
  this->DeliverChangeNotifications();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("domain1.test", cookie_changes[0].cookie.Domain());
  EXPECT_EQ("cookie", cookie_changes[0].cookie.Name());
  EXPECT_TRUE(this->IsExpectedAccessSemantics(
      CookieAccessSemantics::LEGACY,
      cookie_changes[0].access_result.access_semantics));
}

TYPED_TEST_P(CookieStoreChangeNamedTest, PartitionedCookies) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_partitioned_cookies)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          GURL("https://www.example.com"), "__Host-a",
          CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  // Unpartitioned cookie
  this->CreateAndSetCookie(cs, GURL("https://www.example.com"),
                           "__Host-a=1; Secure; Path=/",
                           CookieOptions::MakeAllInclusive());
  // Partitioned cookie with the same partition key
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=2; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://sub.foo.com")));
  // Partitioned cookie with a different partition key
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=3; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com")));
  this->DeliverChangeNotifications();

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_FALSE(cookie_changes[0].cookie.IsPartitioned());
  EXPECT_EQ("1", cookie_changes[0].cookie.Value());
  EXPECT_TRUE(cookie_changes[1].cookie.IsPartitioned());
  EXPECT_EQ(CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
            cookie_changes[1].cookie.PartitionKey().value());
  EXPECT_EQ("2", cookie_changes[1].cookie.Value());

  // Test that when the partition key parameter is nullopt that all Partitioned
  // cookies do not emit events.
  std::vector<CookieChangeInfo> other_cookie_changes;
  std::unique_ptr<CookieChangeSubscription> other_subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          GURL("https://www.example.com"), "__Host-a",
          std::nullopt /* cookie_partition_key */,
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&other_cookie_changes)));
  // Update Max-Age: None -> 7200
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=2; Secure; Path=/; Partitioned; Max-Age=7200",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, other_cookie_changes.size());
  // Check that the other listener was invoked.
  ASSERT_LT(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, PartitionedCookies_WithNonce) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_partitioned_cookies) {
    return;
  }

  CookieStore* cs = this->GetCookieStore();
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  std::vector<CookieChangeInfo> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          GURL("https://www.example.com"), "__Host-a",
          CookiePartitionKey::FromURLForTesting(
              GURL("https://www.foo.com"),
              CookiePartitionKey::AncestorChainBit::kCrossSite, nonce),
          base::BindRepeating(
              &CookieStoreChangeTestBase<TypeParam>::OnCookieChange,
              base::Unretained(&cookie_changes)));

  // Should not see changes to an unpartitioned cookie.
  this->CreateAndSetCookie(cs, GURL("https://www.example.com"),
                           "__Host-a=1; Secure; Path=/",
                           CookieOptions::MakeAllInclusive());
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Set partitioned cookie without nonce. Should not see the change.
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=2; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")));
  this->DeliverChangeNotifications();
  ASSERT_EQ(0u, cookie_changes.size());

  // Set partitioned cookie with nonce.
  this->CreateAndSetCookie(
      cs, GURL("https://www.example.com"),
      "__Host-a=3; Secure; Path=/; Partitioned",
      CookieOptions::MakeAllInclusive(), std::nullopt /* server_time */,
      std::nullopt /* system_time */,
      CookiePartitionKey::FromURLForTesting(
          GURL("https://www.foo.com"),
          CookiePartitionKey::AncestorChainBit::kCrossSite, nonce));
  this->DeliverChangeNotifications();
  ASSERT_EQ(1u, cookie_changes.size());
}

REGISTER_TYPED_TEST_SUITE_P(CookieStoreChangeGlobalTest,
                            NoCookie,
                            InitialCookie,
                            InsertOne,
                            InsertMany,
                            DeleteOne,
                            DeleteTwo,
                            Overwrite,
                            OverwriteWithHttpOnly,
                            Deregister,
                            DeregisterMultiple,
                            DispatchRace,
                            DeregisterRace,
                            DeregisterRaceMultiple,
                            MultipleSubscriptions,
                            ChangeIncludesCookieAccessSemantics,
                            PartitionedCookies);

REGISTER_TYPED_TEST_SUITE_P(CookieStoreChangeUrlTest,
                            NoCookie,
                            InitialCookie,
                            InsertOne,
                            InsertMany,
                            InsertFiltering,
                            DeleteOne,
                            DeleteTwo,
                            DeleteFiltering,
                            Overwrite,
                            OverwriteFiltering,
                            OverwriteWithHttpOnly,
                            Deregister,
                            DeregisterMultiple,
                            DispatchRace,
                            DeregisterRace,
                            DeregisterRaceMultiple,
                            DifferentSubscriptionsDisjoint,
                            DifferentSubscriptionsDomains,
                            DifferentSubscriptionsPaths,
                            DifferentSubscriptionsFiltering,
                            MultipleSubscriptions,
                            ChangeIncludesCookieAccessSemantics,
                            PartitionedCookies,
                            PartitionedCookies_WithNonce);

REGISTER_TYPED_TEST_SUITE_P(CookieStoreChangeNamedTest,
                            NoCookie,
                            InitialCookie,
                            InsertOne,
                            InsertTwo,
                            InsertFiltering,
                            DeleteOne,
                            DeleteTwo,
                            DeleteFiltering,
                            Overwrite,
                            OverwriteFiltering,
                            OverwriteWithHttpOnly,
                            Deregister,
                            DeregisterMultiple,
                            DispatchRace,
                            DeregisterRace,
                            DeregisterRaceMultiple,
                            DifferentSubscriptionsDisjoint,
                            DifferentSubscriptionsDomains,
                            DifferentSubscriptionsNames,
                            DifferentSubscriptionsPaths,
                            DifferentSubscriptionsFiltering,
                            MultipleSubscriptions,
                            SubscriptionOutlivesStore,
                            ChangeIncludesCookieAccessSemantics,
                            PartitionedCookies,
                            PartitionedCookies_WithNonce);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_CHANGE_UNITTEST_H_
