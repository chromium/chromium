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
#include "net/cookies/first_party_set_metadata.h"
#include "net/cookies/same_party_context.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using ::testing::IsEmpty;
using Type = net::SamePartyContext::Type;
using OverrideSets =
    base::flat_map<net::SchemefulSite, absl::optional<net::SchemefulSite>>;

namespace network {

namespace {

const net::SchemefulSite kSet1Owner(GURL("https://example.test"));
const net::SchemefulSite kSet1Member1(GURL("https://member1.test"));
const net::SchemefulSite kSet1Member2(GURL("https://member3.test"));
const net::SchemefulSite kSet2Owner(GURL("https://foo.test"));
const net::SchemefulSite kSet2Member1(GURL("https://member2.test"));
const net::SchemefulSite kSet3Owner(GURL("https://bar.test"));
const net::SchemefulSite kSet3Member1(GURL("https://member4.test"));

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
    first_party_sets_manager_.SetCompleteSets({
        {kSet1Member1, kSet1Owner},
        {kSet1Member2, kSet1Owner},
        {kSet1Owner, kSet1Owner},
        {kSet2Member1, kSet2Owner},
        {kSet2Owner, kSet2Owner},
    });
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

TEST_F(NoopFirstPartySetsAccessDelegateTest, Sets) {
  EXPECT_THAT(delegate().Sets(base::NullCallback()),
              FirstPartySetsAccessDelegate::SetsByOwner({
                  {kSet1Owner, {kSet1Owner, kSet1Member1, kSet1Member2}},
                  {kSet2Owner, {kSet2Owner, kSet2Member1}},
              }));
}

TEST_F(NoopFirstPartySetsAccessDelegateTest, FindOwner) {
  EXPECT_THAT(delegate().FindOwner(kSet1Owner, base::NullCallback()),
              absl::make_optional(kSet1Owner));
  EXPECT_THAT(delegate().FindOwner(kSet2Member1, base::NullCallback()),
              absl::make_optional(kSet2Owner));
}

TEST_F(NoopFirstPartySetsAccessDelegateTest, FindOwners) {
  EXPECT_THAT(
      delegate().FindOwners({kSet1Member1, kSet2Member1}, base::NullCallback()),
      FirstPartySetsAccessDelegate::OwnersResult({
          {kSet1Member1, kSet1Owner},
          {kSet2Member1, kSet2Owner},
      }));
}

class FirstPartySetsAccessDelegateTest : public ::testing::Test {
 public:
  explicit FirstPartySetsAccessDelegateTest(bool enabled)
      : first_party_sets_manager_(/*enabled=*/true),
        delegate_(delegate_remote_.BindNewPipeAndPassReceiver(),
                  CreateFirstPartySetsAccessDelegateParams(enabled),
                  &first_party_sets_manager_) {
    first_party_sets_manager_.SetCompleteSets({
        {kSet1Member1, kSet1Owner},
        {kSet1Member2, kSet1Owner},
        {kSet1Owner, kSet1Owner},
        {kSet2Member1, kSet2Owner},
        {kSet2Owner, kSet2Owner},
    });
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

  FirstPartySetsAccessDelegate::SetsByOwner SetsAndWait() {
    base::test::TestFuture<FirstPartySetsAccessDelegate::SetsByOwner> future;
    absl::optional<FirstPartySetsAccessDelegate::SetsByOwner> result =
        delegate_.Sets(future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
  }

  FirstPartySetsAccessDelegate::OwnerResult FindOwnerAndWait(
      const net::SchemefulSite& site) {
    base::test::TestFuture<FirstPartySetsAccessDelegate::OwnerResult> future;
    absl::optional<FirstPartySetsAccessDelegate::OwnerResult> result =
        delegate_.FindOwner(site, future.GetCallback());
    return result.has_value() ? result.value() : future.Get();
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

class FirstPartySetsAccessDelegateDisabledTest
    : public FirstPartySetsAccessDelegateTest {
 public:
  FirstPartySetsAccessDelegateDisabledTest()
      : FirstPartySetsAccessDelegateTest(false) {}
};

TEST_F(FirstPartySetsAccessDelegateDisabledTest, ComputeMetadata) {
  EXPECT_THAT(ComputeMetadataAndWait(kSet1Member1, &kSet1Member1,
                                     {kSet1Member1, kSet1Owner})
                  .context(),
              net::SamePartyContext(Type::kCrossParty, Type::kCrossParty,
                                    Type::kSameParty));
}

TEST_F(FirstPartySetsAccessDelegateDisabledTest, Sets_IsEmpty) {
  EXPECT_THAT(SetsAndWait(), IsEmpty());
}

TEST_F(FirstPartySetsAccessDelegateDisabledTest, FindOwner) {
  EXPECT_FALSE(FindOwnerAndWait(kSet1Owner));
  EXPECT_FALSE(FindOwnerAndWait(kSet1Member1));
}

TEST_F(FirstPartySetsAccessDelegateDisabledTest, FindOwners) {
  EXPECT_THAT(FindOwnersAndWait({kSet1Member1, kSet2Member1}), IsEmpty());
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

  EXPECT_EQ(future.Get(),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &kSet1Owner,
                &kSet1Owner, net::FirstPartySetsContextType::kHomogeneous));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, QueryBeforeReady_Sets) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::SetsByOwner> future;
  EXPECT_FALSE(delegate().Sets(future.GetCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  EXPECT_THAT(future.Get(),
              FirstPartySetsAccessDelegate::SetsByOwner({
                  {kSet1Owner, {kSet1Owner, kSet1Member1, kSet1Member2}},
                  {kSet2Owner, {kSet2Owner, kSet2Member1}},
              }));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, QueryBeforeReady_FindOwner) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::OwnerResult> future;
  EXPECT_FALSE(delegate().FindOwner(kSet1Member1, future.GetCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  EXPECT_THAT(future.Get(), absl::make_optional(kSet1Owner));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, QueryBeforeReady_FindOwners) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::OwnersResult> future;
  EXPECT_FALSE(delegate().FindOwners({kSet1Member1, kSet2Member1},
                                     future.GetCallback()));

  delegate_remote()->NotifyReady(mojom::FirstPartySetsReadyEvent::New());

  EXPECT_THAT(future.Get(), FirstPartySetsAccessDelegate::OwnersResult({
                                {kSet1Member1, kSet1Owner},
                                {kSet2Member1, kSet2Owner},
                            }));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_ComputeMetadata) {
  base::test::TestFuture<net::FirstPartySetMetadata> future;
  {
    // Force deallocation to provoke a UAF if the impl just copies the pointer.
    net::SchemefulSite local_member1(kSet1Member1);
    EXPECT_FALSE(delegate().ComputeMetadata(
        kSet1Owner, &local_member1, {kSet1Member1}, future.GetCallback()));
  }

  // The member of an override set is also an member of an existing set as an
  // addition.
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
      {kSet1Member1, {kSet3Owner}},
      {kSet1Member2, {kSet3Owner}},
      {kSet1Owner, {kSet3Owner}},
      {kSet3Owner, {kSet3Owner}},
  }));

  EXPECT_EQ(future.Get(),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &kSet3Owner,
                &kSet3Owner, net::FirstPartySetsContextType::kHomogeneous));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_Sets) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::SetsByOwner> future;
  EXPECT_FALSE(delegate().Sets(future.GetCallback()));
  // The member of an override set is also an owner of an existing set as an
  // addition.
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
      {kSet3Member1, {kSet3Owner}},
      {kSet1Owner, {kSet3Owner}},
      {kSet1Member1, {kSet3Owner}},
      {kSet1Member2, {kSet3Owner}},
      {kSet3Owner, {kSet3Owner}},
  }));

  EXPECT_THAT(
      future.Get(),
      FirstPartySetsAccessDelegate::SetsByOwner({
          {kSet2Owner, {kSet2Owner, kSet2Member1}},
          {kSet3Owner,
           {kSet3Owner, kSet3Member1, kSet1Owner, kSet1Member1, kSet1Member2}},
      }));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_FindOwner) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::OwnerResult> future;
  EXPECT_FALSE(delegate().FindOwner(kSet1Member1, future.GetCallback()));

  // The owner of an override set is also an member of an existing set as an
  // addition.
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
      {kSet1Owner, {kSet1Member1}},
      {kSet1Member1, {kSet1Member1}},
      {kSet1Member2, {kSet1Member1}},
  }));

  EXPECT_THAT(future.Get(), absl::make_optional(kSet1Member1));
}

TEST_F(AsyncFirstPartySetsAccessDelegateTest, OverrideSets_FindOwners) {
  base::test::TestFuture<FirstPartySetsAccessDelegate::OwnersResult> future;
  EXPECT_FALSE(
      delegate().FindOwners({kSet1Member1, kSet1Owner}, future.GetCallback()));

  // The owner of a override set is also a member of an existing set as a
  // replacement.
  delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
      {kSet1Member2, {kSet1Member1}},
      {kSet1Member1, {kSet1Member1}},
      {kSet1Owner, absl::nullopt},
  }));

  EXPECT_THAT(future.Get(), FirstPartySetsAccessDelegate::OwnersResult({
                                {kSet1Member1, kSet1Member1},
                            }));
}

class SyncFirstPartySetsAccessDelegateTest
    : public AsyncFirstPartySetsAccessDelegateTest {
 public:
  SyncFirstPartySetsAccessDelegateTest() {
    delegate_remote()->NotifyReady(CreateFirstPartySetsReadyEvent({
        {kSet3Member1, {kSet3Owner}},
        {kSet3Owner, {kSet3Owner}},
    }));
  }
};

TEST_F(SyncFirstPartySetsAccessDelegateTest, ComputeMetadata) {
  EXPECT_EQ(ComputeMetadataAndWait(kSet1Member1, &kSet1Member1, {kSet1Member1}),
            net::FirstPartySetMetadata(
                net::SamePartyContext(Type::kSameParty), &kSet1Owner,
                &kSet1Owner, net::FirstPartySetsContextType::kHomogeneous));
}

TEST_F(SyncFirstPartySetsAccessDelegateTest, Sets) {
  EXPECT_THAT(SetsAndWait(),
              FirstPartySetsAccessDelegate::SetsByOwner({
                  {kSet1Owner, {kSet1Owner, kSet1Member1, kSet1Member2}},
                  {kSet2Owner, {kSet2Owner, kSet2Member1}},
                  {kSet3Owner, {kSet3Owner, kSet3Member1}},
              }));
}

TEST_F(SyncFirstPartySetsAccessDelegateTest, FindOwner) {
  EXPECT_THAT(FindOwnerAndWait(kSet1Member1), absl::make_optional(kSet1Owner));
}

TEST_F(SyncFirstPartySetsAccessDelegateTest, FindOwners) {
  EXPECT_THAT(FindOwnersAndWait({kSet1Member1, kSet2Member1, kSet3Member1}),
              FirstPartySetsAccessDelegate::OwnersResult({
                  {kSet1Member1, kSet1Owner},
                  {kSet2Member1, kSet2Owner},
                  {kSet3Member1, kSet3Owner},
              }));
}

}  // namespace network
