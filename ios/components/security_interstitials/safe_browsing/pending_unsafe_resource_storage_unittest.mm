// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/pending_unsafe_resource_storage.h"

#import "base/functional/bind.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

using safe_browsing::SBThreatType;
using security_interstitials::UnsafeResource;

class PendingUnsafeResourceStorageTest : public PlatformTest {
 protected:
  PendingUnsafeResourceStorageTest()
      : url_("http://www.chromium.test"),
        threat_type_(safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING) {
    // Create a resource and add it as a pending decision.
    UnsafeResource resource;
    resource.url = url_;
    resource.navigation_url = url_;
    resource.weak_web_state = web_state_.GetWeakPtr();
    resource.threat_type = threat_type_;
    resource.callback =
        base::BindRepeating(&PendingUnsafeResourceStorageTest::ResourceCallback,
                            base::Unretained(this));
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    allow_list()->AddPendingUnsafeNavigationDecision(url_, threat_type_);

    // Create a storage for `resource`.
    storage_ = PendingUnsafeResourceStorage(resource);
  }

  SafeBrowsingUrlAllowList* allow_list() {
    return SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  }

  void ResourceCallback(UnsafeResource::UrlCheckResult result) {
    resource_callback_executed_ = true;
  }

  web::FakeWebState web_state_;
  const GURL url_;
  const SBThreatType threat_type_;
  bool resource_callback_executed_ = false;
  PendingUnsafeResourceStorage storage_;
};

// Tests that the unsafe resource is returned while the decision is still
// pending.
TEST_F(PendingUnsafeResourceStorageTest, PendingResourceValue) {
  const UnsafeResource* resource = storage_.resource();
  ASSERT_TRUE(resource);
  EXPECT_EQ(url_, resource->url);
  EXPECT_EQ(threat_type_, resource->threat_type);
}

// Tests that the unsafe resource's callback is reset to a no-op callback.
TEST_F(PendingUnsafeResourceStorageTest, NoOpCallback) {
  const UnsafeResource* resource = storage_.resource();
  ASSERT_TRUE(resource);
  UnsafeResource::UrlCheckResult result(
      /*proceed=*/false, /*showed_interstitial=*/false,
      /*has_post_commit_interstitial_skipped=*/false);
  resource->callback.Run(result);
  EXPECT_FALSE(resource_callback_executed_);
}

// Tests that the unsafe resource is reset if the threat type is allowed.
TEST_F(PendingUnsafeResourceStorageTest, AllowPendingResource) {
  ASSERT_TRUE(storage_.resource());

  allow_list()->AllowUnsafeNavigations(url_, threat_type_);
  EXPECT_FALSE(storage_.resource());
}

// Tests that the unsafe resource is reset if the pending decision is removed.
TEST_F(PendingUnsafeResourceStorageTest, RemovePendingDecision) {
  ASSERT_TRUE(storage_.resource());

  allow_list()->RemovePendingUnsafeNavigationDecisions(url_);
  EXPECT_FALSE(storage_.resource());
}

// Tests that the constructors and assign operator work correctly.
TEST_F(PendingUnsafeResourceStorageTest, Constructors) {
  ASSERT_TRUE(storage_.resource());

  // Verify copy constructor.
  PendingUnsafeResourceStorage storage_copy(storage_);
  ASSERT_TRUE(storage_copy.resource());
  EXPECT_EQ(url_, storage_copy.resource()->url);
  EXPECT_EQ(threat_type_, storage_copy.resource()->threat_type);

  // Verify assignment operator.
  PendingUnsafeResourceStorage assigned_storage = storage_;
  ASSERT_TRUE(assigned_storage.resource());
  EXPECT_EQ(url_, assigned_storage.resource()->url);
  EXPECT_EQ(threat_type_, assigned_storage.resource()->threat_type);

  // Verify that the default constructo creates an empty storage.
  storage_ = PendingUnsafeResourceStorage();
  EXPECT_FALSE(storage_.resource());

  // Verify that the copied storages reset properly when the pending decision is
  // finished.
  allow_list()->RemovePendingUnsafeNavigationDecisions(url_);
  EXPECT_FALSE(storage_copy.resource());
  EXPECT_FALSE(assigned_storage.resource());
}
