// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state_chrome.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/cert/key_pinning_state.h"
#include "net/cert/key_pinning_state_chrome_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const char kHost[] = "example.test";

// GoodPin1 is in the `static_spki_hashes` of both withoutRejectedPins and
// withRejectedPins pinsets of the static_unittest_default pin data.
constexpr char kGoodPin1[] =
    "sha256/Nn8jk5By4Vkq6BeOVZ7R7AC6XUUBZsWmUbJR1f1Y5FY=";

// TestSPKI1 is in the `static_spki_hashes` of withoutRejectedPins, but is in
// the `bad_static_spki_hashes` of withRejectedPins pinset of the
// static_unittest_default pin data.
constexpr char kTestSPKI1[] =
    "sha256/w3y7Yg3RzkAyhCeBoLHm71YRnuuUW87AAR/DVpLMTw4=";

// kGoodPath is a list of hashes that will be allowed by CheckPublicKeyPins for
// both withRejectedPins and withoutRejectedPins pinsets. It contains kGoodPin1
// plus some other unknown hashes.
constexpr auto kGoodPath = std::to_array<const char*>({
    "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
    "sha256/fzP+pVAbH0hRoUphJKenIP8+2tD/d2QH9J+kQNieM6Q=",
    "sha256/9vRUVdjloCa4wXUKfDWotV5eUXYD7vu0v0z9SRzQdzg=",
    kGoodPin1,
});

// kMaybeRejectedPath is a list of hashes that will be allowed by
// CheckPublicKeyPins for withoutRejectedPins but will fail for the
// withRejectedPins pinset which has TestSPKI1 in `bad_static_spki_hashes`.
constexpr auto kMaybeRejectedPath = std::to_array<const char*>({
    "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
    "sha256/fzP+pVAbH0hRoUphJKenIP8+2tD/d2QH9J+kQNieM6Q=",
    kTestSPKI1,
    kGoodPin1,
});

// kBadPath is a list of hashes that will be rejected by CheckPublicKeyPins for
// both withRejectedPins and withoutRejectedPins pinsets. It contains only
// unknown hashes.
constexpr auto kBadPath = std::to_array<const char*>({
    "sha256/1111111111111111111111111111111111111111111=",
    "sha256/2222222222222222222222222222222222222222222=",
    "sha256/3333333333333333333333333333333333333333333=",
});

std::vector<SHA256HashValue> DeserializeHashes(
    base::span<const char* const> serialized_hashes) {
  std::vector<SHA256HashValue> result;
  for (const auto* serialized_hash : serialized_hashes) {
    std::optional<net::HashValue> h =
        net::HashValue::FromString(std::string_view(serialized_hash));
    CHECK(h);
    result.push_back(h->sha256hashvalue());
  }
  return result;
}

TEST(KeyPinningStateChrome, LongNames) {
  auto state = KeyPinningStateChrome::CreateForCompiledIn();
  const char kLongName[] =
      "lookupByWaveIdHashAndWaveIdIdAndWaveIdDomainAndWaveletIdIdAnd"
      "WaveletIdDomainAndBlipBlipid";
  KeyPinningState::PKPState pkp_state;
  // Just checks that we don't hit a NOTREACHED
  EXPECT_FALSE(state->GetPKPState(kLongName, &pkp_state));
  EXPECT_FALSE(state->HasPublicKeyPins(kLongName));
}

TEST(KeyPinningStateChrome, PinValidationWithCompiledInPins) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable;

  std::vector<SHA256HashValue> good_hashes = DeserializeHashes(kGoodPath);
  std::vector<SHA256HashValue> maybe_rejected_hashes =
      DeserializeHashes(kMaybeRejectedPath);
  std::vector<SHA256HashValue> bad_hashes = DeserializeHashes(kBadPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  struct TestCase {
    std::string_view hostname;
    bool has_pins;
    std::vector<std::vector<SHA256HashValue>> good_hash_lists;
    std::vector<std::vector<SHA256HashValue>> fail_hash_lists;
  };
  const TestCase kTestcases[] = {
      {.hostname = "no-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes, maybe_rejected_hashes},
       .fail_hash_lists = {bad_hashes}},
      {.hostname = "subdomain.no-rejected-pins.test", .has_pins = false},
      {.hostname = "subdomain2.subdomain.no-rejected-pins.test",
       .has_pins = false},

      {.hostname = "with-subdomains-no-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes, maybe_rejected_hashes},
       .fail_hash_lists = {bad_hashes}},
      {.hostname = "subdomain.with-subdomains-no-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes, maybe_rejected_hashes},
       .fail_hash_lists = {bad_hashes}},
      {.hostname = "subdomain2.subdomain.with-subdomains-no-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes, maybe_rejected_hashes},
       .fail_hash_lists = {bad_hashes}},

      {.hostname = "rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes},
       .fail_hash_lists = {maybe_rejected_hashes, bad_hashes}},
      {.hostname = "subdomain.rejected-pins.test", .has_pins = false},
      {.hostname = "subdomain2.subdomain.rejected-pins.test",
       .has_pins = false},

      {.hostname = "with-subdomains-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes},
       .fail_hash_lists = {maybe_rejected_hashes, bad_hashes}},
      {.hostname = "subdomain.with-subdomains-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes},
       .fail_hash_lists = {maybe_rejected_hashes, bad_hashes}},
      {.hostname = "subdomain2.subdomain.with-subdomains-rejected-pins.test",
       .has_pins = true,
       .good_hash_lists = {good_hashes},
       .fail_hash_lists = {maybe_rejected_hashes, bad_hashes}},

      {.hostname = "test", .has_pins = false},
      {.hostname = "not-present.test", .has_pins = false},
  };

  for (const auto& testcase : kTestcases) {
    SCOPED_TRACE(testcase.hostname);

    KeyPinningState::PKPState pkp_state;
    EXPECT_EQ(testcase.has_pins,
              state->GetPKPState(testcase.hostname, &pkp_state));
    EXPECT_EQ(testcase.has_pins, pkp_state.HasPublicKeyPins());

    EXPECT_EQ(testcase.has_pins, state->HasPublicKeyPins(testcase.hostname));

    for (const auto& hash_list : testcase.good_hash_lists) {
      EXPECT_EQ(KeyPinningState::PKPStatus::OK,
                state->CheckPublicKeyPins(testcase.hostname, true, hash_list));
      EXPECT_TRUE(pkp_state.CheckPublicKeyPins(hash_list));
    }

    for (const auto& hash_list : testcase.fail_hash_lists) {
      EXPECT_EQ(KeyPinningState::PKPStatus::VIOLATED,
                state->CheckPublicKeyPins(testcase.hostname, true, hash_list));
      EXPECT_FALSE(pkp_state.CheckPublicKeyPins(hash_list));
    }

    // For hostnames that don't match any pins, any hashes should be allowed.
    if (!testcase.has_pins) {
      EXPECT_EQ(
          KeyPinningState::PKPStatus::OK,
          state->CheckPublicKeyPins(testcase.hostname, true, good_hashes));
      EXPECT_EQ(KeyPinningState::PKPStatus::OK,
                state->CheckPublicKeyPins(testcase.hostname, true,
                                          maybe_rejected_hashes));
      EXPECT_EQ(KeyPinningState::PKPStatus::OK,
                state->CheckPublicKeyPins(testcase.hostname, true, bad_hashes));
    }
  }
}

TEST(KeyPinningStateChrome, DisableStaticPinsByMember) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_disable(
      ScopedKeyPinningStateChromeDefaults::kForceDisabled);
  auto state = KeyPinningStateChrome::CreateForCompiledIn();
  KeyPinningState::PKPState pkp_state;

  EXPECT_FALSE(state->GetPKPState("chrome.google.com", &pkp_state));
  EXPECT_TRUE(pkp_state.spki_hashes.empty());
}

TEST(KeyPinningStateChrome, DisableStaticPinsByFeature) {
  base::test::ScopedFeatureList feature_list(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kStaticKeyPinningEnforcement});
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable(
      ScopedKeyPinningStateChromeDefaults::kForceEnableProduction);
  auto state = KeyPinningStateChrome::CreateForCompiledIn();
  KeyPinningState::PKPState pkp_state;

  EXPECT_FALSE(state->GetPKPState("chrome.google.com", &pkp_state));
  EXPECT_TRUE(pkp_state.spki_hashes.empty());
}

TEST(KeyPinningStateChrome, PreloadedProductionStaticPins) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable(
      ScopedKeyPinningStateChromeDefaults::kForceEnableProduction);
  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  static constexpr std::string_view kExpectedPinnedDomains[] = {
      "1.docs.google.com",
      "a.googlegroups.com",
      "accounts.google.com",
      "ad.doubleclick.net",
      "ajax.googleapis.com",
      "apis.google.com",
      "appengine.google.com",
      "appspot.com",
      "chatenabled.mail.google.com",
      "checkout.google.com",
      "chrome.google.com",
      "docs.google.com",
      "doubleclick.net",
      "drive.google.com",
      "encrypted.google.com",
      "foo.google.com",
      "gmail.com",
      "google",
      "google.com",
      "googleadservices.com",
      "googleapis.com",
      "googlecode.com",
      "googlegroups.com",
      "googlemail.com",
      "googlesyndication.com",
      "googleusercontent.com",
      "groups.google.com",
      "gstatic.com",
      "hostedtalkgadget.google.com",
      "i.ytimg.com",
      "kibbles.googlecode.com",
      "mail-attachment.googleusercontent.com",
      "mail.google.com",
      "market.android.com",
      "pagead2.googleadservices.com",
      "plus.google.com",
      "profiles.google.com",
      "redirector.gvt1.com",
      "sites.google.com",
      "spreadsheets.google.com",
      "ssl.google-analytics.com",
      "ssl.gstatic.com",
      "talk.google.com",
      "talkgadget.google.com",
      "wallet.google.com",
      "www.gmail.com",
      "www.google",
      "www.google-analytics.com",
      "www.google.com",
      "www.googlemail.com",
      "www.googleplex.com",
      "www.googleusercontent.com",
      "www.gstatic.com",
      "www.youtube.com",
      "youtube.com",
      "ytimg.com",
  };

  static constexpr std::string_view kExpectedNonPinnedDomains[] = {
      "facebook.com", "foo.facebook.com", "foo.www.facebook.com",
      "paypal.com",   "www.facebook.com", "www.paypal.com",
  };

  for (const auto& domain : kExpectedPinnedDomains) {
    SCOPED_TRACE(domain);
    EXPECT_TRUE(state->HasPublicKeyPins(domain));
    KeyPinningState::PKPState pkp_state;
    EXPECT_TRUE(state->GetPKPState(domain, &pkp_state));
    EXPECT_FALSE(pkp_state.spki_hashes.empty());

    // If the entry has a static pin list, then it should fail the pinning
    // check against an empty hash list.
    EXPECT_FALSE(pkp_state.CheckPublicKeyPins({}));

    // ... also should fail against an arbitrary hash.
    SHA256HashValue hash;
    hash.fill(1);
    EXPECT_FALSE(pkp_state.CheckPublicKeyPins({hash}));
  }

  for (const auto& domain : kExpectedNonPinnedDomains) {
    SCOPED_TRACE(domain);
    EXPECT_FALSE(state->HasPublicKeyPins(domain));
    KeyPinningState::PKPState pkp_state;
    EXPECT_FALSE(state->GetPKPState(domain, &pkp_state));
    EXPECT_TRUE(pkp_state.spki_hashes.empty());
  }
}

TEST(KeyPinningStateChrome, UpdateKeyPinsListValidPin) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  std::vector<SHA256HashValue> bad_hashes = DeserializeHashes(kBadPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, bad_hashes should be rejected.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));

  // Update the pins list, adding bad_hashes to the accepted hashes for this
  // host.
  KeyPinningStateChrome::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/bad_hashes,
      /*bad_static_spki_hashes=*/{});
  KeyPinningStateChrome::PinSetInfo test_pinsetinfo(
      /*hostname=*/kHost, /*pinset_name=*/"test",
      /*include_subdomains=*/false);
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // Hashes should now be accepted.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));
}

TEST(KeyPinningStateChrome, UpdateKeyPinsListNotValidPin) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  std::vector<SHA256HashValue> good_hashes = DeserializeHashes(kGoodPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, good_hashes should be accepted
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, good_hashes));

  // Update the pins list, adding good_hashes to the rejected hashes for this
  // host.
  KeyPinningStateChrome::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{},
      /*bad_static_spki_hashes=*/good_hashes);
  KeyPinningStateChrome::PinSetInfo test_pinsetinfo(
      /*hostname=*/kHost, /* pinset_name=*/"test",
      /*include_subdomains=*/false);
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // Hashes should now be rejected.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins(kHost, true, good_hashes));

  // Hashes should also be rejected if the hostname has a trailing dot.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins("example.test", true, good_hashes));

  // Hashes should also be rejected if the hostname has different
  // capitalization.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins("ExAmpLe.tEsT", true, good_hashes));
}

TEST(KeyPinningStateChrome, UpdateKeyPinsEmptyList) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  std::vector<SHA256HashValue> bad_hashes = DeserializeHashes(kBadPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, bad_hashes should be rejected.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));

  // Update the pins list with an empty list.
  state = KeyPinningStateChrome::CreateForUpdated({}, {}, base::Time::Now());

  // Hashes should now be accepted.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));
}

TEST(KeyPinningStateChrome, UpdateKeyPinsIncludeSubdomains) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  // unpinned_hashes is a set of hashes that (after the update) won't match the
  // expected hashes for the tld of this domain. kGoodPath is used here because
  // it's a path that is accepted prior to any updates, and this test will
  // validate it is rejected afterwards.
  std::vector<SHA256HashValue> unpinned_hashes = DeserializeHashes(kGoodPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, unpinned_hashes should be accepted
  EXPECT_EQ(
      KeyPinningStateChrome::PKPStatus::OK,
      state->CheckPublicKeyPins("example.sub.test", true, unpinned_hashes));

  // Update the pins list, adding kBadPath to the accepted hashes for this
  // host, relying on include_subdomains for enforcement. The contents of the
  // hashes don't matter as long as they are different from unpinned_hashes,
  // kBadPath is used for convenience.
  KeyPinningStateChrome::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/DeserializeHashes(kBadPath),
      /*bad_static_spki_hashes=*/{});
  // The host used in the test is "example.sub.test", so this pinset will only
  // match due to include subdomains.
  KeyPinningStateChrome::PinSetInfo test_pinsetinfo(
      /*hostname=*/"sub.test", /* pinset_name=*/"test",
      /*include_subdomains=*/true);
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // The path that was accepted before updating the pins should now be rejected.
  EXPECT_EQ(
      KeyPinningStateChrome::PKPStatus::VIOLATED,
      state->CheckPublicKeyPins("example.sub.test", true, unpinned_hashes));
}

TEST(KeyPinningStateChrome, UpdateKeyPinsIncludeSubdomainsTLD) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  // unpinned_hashes is a set of hashes that (after the update) won't match the
  // expected hashes for the tld of this domain. kGoodPath is used here because
  // it's a path that is accepted prior to any updates, and this test will
  // validate it is rejected afterwards.
  std::vector<SHA256HashValue> unpinned_hashes = DeserializeHashes(kGoodPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, unpinned_hashes should be accepted
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, unpinned_hashes));

  // Update the pins list, adding kBadPath to the accepted hashes for this
  // host, relying on include_subdomains for enforcement. The contents of the
  // hashes don't matter as long as they are different from unpinned_hashes,
  // kBadPath is used for convenience.
  KeyPinningStateChrome::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/DeserializeHashes(kBadPath),
      /*bad_static_spki_hashes=*/{});
  // The host used in the test is "example.test", so this pinset will only match
  // due to include subdomains.
  KeyPinningStateChrome::PinSetInfo test_pinsetinfo(
      /*hostname=*/"test", /* pinset_name=*/"test",
      /*include_subdomains=*/true);
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // The path that was accepted before updating the pins should now be rejected.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins(kHost, true, unpinned_hashes));
}

TEST(KeyPinningStateChrome, UpdateKeyPinsDontIncludeSubdomains) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  // unpinned_hashes is a set of hashes that (after the update) won't match the
  // expected hashes for the tld of this domain. kGoodPath is used here because
  // it's a path that is accepted prior to any updates, and this test will
  // validate it is accepted or rejected afterwards depending on whether the
  // domain is an exact match.
  std::vector<SHA256HashValue> unpinned_hashes = DeserializeHashes(kGoodPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, unpinned_hashes should be accepted
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, unpinned_hashes));

  // Update the pins list, adding kBadPath to the accepted hashes for the
  // tld of this host, but without include_subdomains set. The contents of the
  // hashes don't matter as long as they are different from unpinned_hashes,
  // kBadPath is used for convenience.
  KeyPinningStateChrome::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/DeserializeHashes(kBadPath),
      /*bad_static_spki_hashes=*/{});
  // The host used in the test is "example.test", so this pinset will not match
  // due to include subdomains not being set.
  KeyPinningStateChrome::PinSetInfo test_pinsetinfo(
      /*hostname=*/"test", /* pinset_name=*/"test",
      /*include_subdomains=*/false);
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now());

  // Hashes that were accepted before the update should still be accepted since
  // include subdomains is not set for the pinset, and this is not an exact
  // match.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, unpinned_hashes));

  // Hashes should be rejected for an exact match of the hostname.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins("test", true, unpinned_hashes));
}

TEST(KeyPinningStateChrome, UpdateKeyPinsListTimestamp) {
  base::test::ScopedFeatureList feature_list(
      features::kStaticKeyPinningEnforcement);
  ScopedKeyPinningStateChromeDefaults scoped_static_pinning_enable_;
  std::vector<SHA256HashValue> bad_hashes = DeserializeHashes(kBadPath);

  auto state = KeyPinningStateChrome::CreateForCompiledIn();

  // Prior to updating the list, bad_hashes should be rejected.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));

  // ScopedKeyPinningStateChromeDefaults normally sets a flag that results in
  // considering the pins list as always timely. We need to disable that so we
  // can test that the timestamp supplied with component update data has the
  // required effect.
  scoped_static_pinning_enable_.Reset(
      ScopedKeyPinningStateChromeDefaults::kUnittestDefaultWithoutAlwaysTimely);

  KeyPinningStateChrome::PinSet test_pinset(
      /*name=*/"test",
      /*static_spki_hashes=*/{},
      /*bad_static_spki_hashes=*/bad_hashes);
  KeyPinningStateChrome::PinSetInfo test_pinsetinfo(
      /*hostname=*/kHost, /* pinset_name=*/"test",
      /*include_subdomains=*/false);
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now() - base::Days(70));

  // Hashes should now be accepted.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::OK,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));

  // Update the pins list again, with a timestamp <70 days old.
  state = KeyPinningStateChrome::CreateForUpdated(
      {test_pinset}, {test_pinsetinfo}, base::Time::Now() - base::Days(69));

  // Hashes should now be rejected.
  EXPECT_EQ(KeyPinningStateChrome::PKPStatus::VIOLATED,
            state->CheckPublicKeyPins(kHost, true, bad_hashes));
}

}  // namespace
}  // namespace net
