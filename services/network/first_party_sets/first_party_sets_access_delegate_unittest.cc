// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_access_delegate.h"

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_entry.h"
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "services/network/public/mojom/first_party_sets.mojom.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using Type = net::SamePartyContext::Type;
using OverrideSets =
    base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>;

namespace network {

namespace {

const net::SchemefulSite kSet1Owner(GURL("https://set1owner.test"));
const net::SchemefulSite kSet1Member1(GURL("https://set1member1.test"));
const net::SchemefulSite kSet1Member2(GURL("https://set1member2.test"));
const net::SchemefulSite kSet2Owner(GURL("https://set2owner.test"));
const net::SchemefulSite kSet2Member1(GURL("https://set2member1.test"));
const net::SchemefulSite kSet3Owner(GURL("https://set3owner.test"));
const net::SchemefulSite kSet3Member1(GURL("https://set3member1.test"));

mojom::FirstPartySetsAccessDelegateParamsPtr
CreateFirstPartySetsAccessDelegateParams(bool enabled) {
  auto params = mojom::FirstPartySetsAccessDelegateParams::New();
  params->enabled = enabled;
  return params;
}

mojom::FirstPartySetsReadyEventPtr CreateFirstPartySetsReadyEvent(
    OverrideSets override_sets) {
  auto ready_event = mojom::FirstPartySetsReadyEvent::New();
  ready_event->customizations = std::move(override_sets);
  return ready_event;
}

mojom::PublicFirstPartySetsPtr CreatePublicFirstPartySets(
    FirstPartySetsAccessDelegate::FlattenedSets sets) {
  mojom::PublicFirstPartySetsPtr public_sets =
      mojom::PublicFirstPartySets::New();
  public_sets->sets = sets;
  return public_sets;
}

}  // namespace

// No-op FirstPartySetsAccessDelegate should just pass queries to
// FirstPartySetsManager synchronously.
class NoopFirstPartySetsAccessDelegateTest : public ::testing::Test {
 public:
  NoopFirstPartySetsAccessDelegateTest()
      : first_party_sets_manager_(/*enabled=*/true),
        delegate_(
            /*receiver=*/mojo::NullReceiver(),
            /*params=*/nullptr,
            &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets(CreatePublicFirstPartySets({
        {kSet1Member1,
         net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 0)},
        {kSet1Member2,
         net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 1)},
        {kSet1Owner, net::FirstPartySetEntry(
                         kSet1Owner, net::SiteType::kPrimary, absl::nullopt)},
        {kSet2Member1,
         net::FirstPartySetEntry(kSet2Owner, net::SiteType::kAssociated, 0)},
        {kSet2Owner, net::FirstPartySetEntry(
                         kSet2Owner, net::SiteType::kPrimary, absl::nullopt)},
    }));
  }

  FirstPartySetsAccessDelegate& delegate() { return delegate_; }

 private:
  FirstPartySetsManager first_party_sets_manager_;
  FirstPartySetsAccessDelegate delegate_;
};

TEST_F(NoopFirstPartySetsAccessDelegateTest, IsEnabled) {
  EXPECT_TRUE(delegate().is_enabled());
}

TEST_F(NoopFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  EXPECT_THAT(
      delegate()
          .ComputeMetadata(kSet1Member1, &kSet1Owner,
                           {kSet1Member1, kSet1Owner}, base::NullCallback())
          ->context(),
      net::SamePartyContext(Type::kSameParty));
}

TEST_F(NoopFirstPartySetsAccessDelegateTest, FindOwners) {
  EXPECT_THAT(
      delegate().FindOwners({kSet1Member1, kSet2Member1}, base::NullCallback()),
      FirstPartySetsAccessDelegate::OwnersResult({
          {kSet1Member1,
           net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 0)},
          {kSet2Member1,
           net::FirstPartySetEntry(kSet2Owner, net::SiteType::kAssociated, 0)},
      }));
}

class FirstPartySetsAccessDelegateTest : public ::testing::Test {
 public:
  explicit FirstPartySetsAccessDelegateTest(bool enabled)
      : first_party_sets_manager_(/*enabled=*/true),
        delegate_(delegate_remote_.BindNewPipeAndPassReceiver(),
                  CreateFirstPartySetsAccessDelegateParams(enabled),
                  &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets(CreatePublicFirstPartySets({
        {kSet1Member1,
         net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 0)},
        {kSet1Member2,
         net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 1)},
        {kSet1Owner, net::FirstPartySetEntry(
                         kSet1Owner, net::SiteType::kPrimary, absl::nullopt)},
        {kSet2Member1,
         net::FirstPartySetEntry(kSet2Owner, net::SiteType::kAssociated, 0)},
        {kSet2Owner, net::FirstPartySetEntry(
                         kSet2Owner, net::SiteType::kPrimary, absl::nullopt)},
    }));
  }

  net::FirstPartySetMetadata ComputeMetadataAndWait(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context) {
    base::test::TestFuture<net::FirstPartySetMetadata> future;
    absl::optional<net::FirstPartySetMetadata> result =
        delegate_.ComputeMetadata(site, top_frame_site, party_context,
                                  future.GetCallback());
    return result.has_value() ? std::move(result).value() : future.Take();
  }

  FirstPartySetsAccessDelegate::OwnersResult FindOwnersAndWait(
      const base::flat_set<net::SchemefulSite>& site) {
    base::test::TestFuture<FirstPartySetsAccessDelegate::OwnersResult> future;
    absl::optional<FirstPartySetsAccessDelegate::OwnersResult> result =
        delegate_.FindOwners(site, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsAccessDelegate& delegate() { return delegate_; }

  mojom::FirstPartySetsAccessDelegate* delegate_remote() {
    return delegate_remote_.get();
  }

 private:
  base::test::TaskEnvironment env_;
  FirstPartySetsManager first_party_sets_manager_;
  mojo::Remote<mojom::FirstPartySetsAccessDelegate> delegate_remote_;
  FirstPartySetsAccessDelegate delegate_;
};

// Since the FPSs is disabled for the context, none of the callbacks
// should ever be called, and the return values should all be non-nullopt.
class FirstPartySetsAccessDelegateDisabledTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  FirstPartySetsAccessDelegateDisabledTest()
      : FirstPartySetsAccessDelegateTest(false) {}
};

TEST_F(FirstPartySetsAccessDelegateDisabledTest, ComputeMetadata) {
  // Same as the default ctor, but just to be explicit:
  net::FirstPartySetMetadata expected_metadata(net::SamePartyContext(),
                                               /*frame_entry=*/nullptr,
                                               /*top_frame_entry=*/nullptr);

  EXPECT_THAT(delegate().ComputeMetadata(
                  kSet1Member1, &kSet1Member1, {kSet1Member1, kSet1Owner},
                  base::BindOnce([](net::FirstPartySetMetadata) { FAIL(); })),
              Optional(std::ref(expected_metadata)));
}

TEST_F(FirstPartySetsAccessDelegateDisabledTest, FindOwners) {
  EXPECT_THAT(
      delegate().FindOwners(
          {kSet1Member1, kSet2Member1},
          base::BindOnce([](FirstPartySetsManager::OwnersResult) { FAIL(); })),
      Optional(IsEmpty()));
}

// Test fixture that allows precise control over when the instance gets FPS
// data. Useful for testing async flows.
class AsyncFirstPartySetsAccessDelegateTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  AsyncFirstPartySetsAccessDelegateTest()
      : FirstPartySetsAccessDelegateTest(true) {}
};

TEST_F(AsyncFirstPartySetsAccessDelegateTest,
       QueryBeforeReady_ComputeMetadata) {
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  {
    // Force deallocation to provoke a UAF if the impl just copies the pointer.
    net::SchemefulSite local_member1(kSet1Member1);
    EXPECT_FALSE(delegate().ComputeMetadata(
        kSet1Member1, &local_member1, {kSet1Member1}, future.GetCallback()));
  }

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  net::FirstPartySetEntry entry(kSet1Owner, net::SiteType::kAssociated, 0);
  EXPECT_EQ(future.Get(),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &entry, &entry));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, QueryBeforeReady_FindOwners) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::OwnersResult> future;
  EXPECT_FALSE(delegate().FindOwners({kSet1Member1, kSet2Member1},
                                     future.GetCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  EXPECT_THAT(
      future.Get(),
      FirstPartySetsAccessDelegate::OwnersResult({
          {kSet1Member1,
           net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 0)},
          {kSet2Member1,
           net::FirstPartySetEntry(kSet2Owner, net::SiteType::kAssociated, 0)},
      }));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_ComputeMetadata) {
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
      {kSet1Member1,
       {net::FirstPartySetEntry(kSet3Owner, net::SiteType::kAssociated, 0)}},
      {kSet3Owner,
       {net::FirstPartySetEntry(kSet3Owner, net::SiteType::kPrimary,
                                absl::nullopt)}},
  }));

  net::FirstPartySetEntry primary_entry(kSet3Owner, net::SiteType::kPrimary,
                                        absl::nullopt);
  net::FirstPartySetEntry associated_entry(kSet3Owner,
                                           net::SiteType::kAssociated, 0);
  EXPECT_EQ(ComputeMetadataAndWait(kSet3Owner, &kSet1Member1, {kSet1Member1}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &primary_entry, &associated_entry));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_FindOwners) {
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
      {kSet3Owner,
       {net::FirstPartySetEntry(kSet3Owner, net::SiteType::kPrimary,
                                absl::nullopt)}},
  }));

  EXPECT_THAT(FindOwnersAndWait({kSet3Owner}),
              UnorderedElementsAre(Pair(kSet3Owner, _)));
}

class SyncFirstPartySetsAccessDelegateTest
    : public AsyncFirstPartySetsAccessDelegateTest {
 public:
  SyncFirstPartySetsAccessDelegateTest() {
    delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
        {kSet3Member1,
         {net::FirstPartySetEntry(kSet3Owner, net::SiteType::kAssociated, 0)}},
        {kSet3Owner,
         {net::FirstPartySetEntry(kSet3Owner, net::SiteType::kPrimary,
                                  absl::nullopt)}},
    }));
  }
};

TEST_F(SyncFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  net::FirstPartySetEntry entry(kSet1Owner, net::SiteType::kAssociated, 0);
  EXPECT_EQ(ComputeMetadataAndWait(kSet1Member1, &kSet1Member1, {kSet1Member1}),
            net::FirstPartySetMetadata(net::SamePartyContext(Type::kSameParty),
                                       &entry, &entry));
}

TEST_F(SyncFirstPartySetsAccessDelegateTest, FindOwners) {
  EXPECT_THAT(
      FindOwnersAndWait({kSet1Member1, kSet2Member1, kSet3Member1}),
      FirstPartySetsAccessDelegate::OwnersResult({
          {kSet1Member1,
           net::FirstPartySetEntry(kSet1Owner, net::SiteType::kAssociated, 0)},
          {kSet2Member1,
           net::FirstPartySetEntry(kSet2Owner, net::SiteType::kAssociated, 0)},
          {kSet3Member1,
           net::FirstPartySetEntry(kSet3Owner, net::SiteType::kAssociated, 0)},
      }));
}

}  // namespace network
